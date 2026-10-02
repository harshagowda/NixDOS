"""Drive a NixDOS 2 VM in QEMU through its serial console."""
import os
import re
import shutil
import subprocess
import tempfile
import threading
import time

ANSI = re.compile(r'\x1b\[[0-9;]*[A-Za-z]')
PROMPT = '$] '


class NixDOS:
    def __init__(self, image, memory=32, keep_image=False):
        self.tmpdir = tempfile.mkdtemp(prefix='nixdos-')
        self.image = image if keep_image else os.path.join(self.tmpdir, 'disk.img')
        if not keep_image:
            shutil.copy(image, self.image)
        self.proc = subprocess.Popen(
            ['qemu-system-i386', '-m', str(memory), '-display', 'none',
             '-drive', 'file=%s,format=raw,if=ide' % self.image,
             '-serial', 'stdio', '-no-reboot'],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        self.buf = ''
        self.lock = threading.Lock()
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()

    def _read(self):
        while True:
            chunk = self.proc.stdout.read1(4096) if hasattr(self.proc.stdout, 'read1') \
                else self.proc.stdout.read(1)
            if not chunk:
                return
            with self.lock:
                self.buf += chunk.decode('latin-1')

    def output(self):
        with self.lock:
            return ANSI.sub('', self.buf).replace('\r', '')

    def wait_for(self, text, timeout=20, start=0):
        end = time.time() + timeout
        while time.time() < end:
            out = self.output()
            idx = out.find(text, start)
            if idx >= 0:
                return idx
            if self.proc.poll() is not None:
                break
            time.sleep(0.05)
        raise TimeoutError('timed out waiting for %r; output tail:\n%s'
                           % (text, self.output()[-1500:]))

    def boot(self, timeout=30):
        self.wait_for(PROMPT, timeout)

    def send_raw(self, data):
        if isinstance(data, str):
            data = data.encode('latin-1')
        # feed slowly enough for the 16-byte UART FIFO
        for i in range(0, len(data), 8):
            self.proc.stdin.write(data[i:i + 8])
            self.proc.stdin.flush()
            time.sleep(0.01)

    def run(self, cmd, timeout=30):
        """Type a command, wait for the next prompt, return what was printed."""
        start = len(self.output())
        self.send_raw(cmd + '\r')
        echo = self.wait_for(cmd, timeout, start)
        begin = echo + len(cmd)
        end = self.wait_for('\n' + PROMPT, timeout, begin)
        return self.output()[begin:end].strip('\n')

    def close(self):
        if self.proc.poll() is None:
            self.proc.kill()
            self.proc.wait()
        shutil.rmtree(self.tmpdir, ignore_errors=True)
