#!/usr/bin/env python3
"""NixDOS 2 end-to-end tests: boots the OS in QEMU and drives it over serial.

usage: run_tests.py [build/nixdos2.img]

The NixC test programs in tests/c/ are also compiled with the host GCC; the
output of both compilers must match exactly (differential testing).
"""
import os
import shutil
import subprocess
import sys
import tempfile
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from nixdos import NixDOS  # noqa: E402

DIFF_TESTS = ['t_arith.c', 't_ptr.c', 't_ctrl.c']

passed = 0
failed = []


def check(name, cond, detail=''):
    global passed
    if cond:
        passed += 1
        print('  ok    ' + name)
    else:
        failed.append(name)
        print('  FAIL  ' + name + ('\n' + detail if detail else ''))


def host_output(src):
    """Compile a test with the host GCC (the reference) and return its output."""
    tmp = tempfile.mkdtemp()
    try:
        exe = os.path.join(tmp, 'a.out')
        subprocess.run(['gcc', '-m32', '-w', '-fno-builtin', '-include', 'stdio.h',
                        '-include', 'string.h', '-include', 'stdlib.h', '-o', exe, src],
                       check=True)
        return subprocess.run([exe], capture_output=True, text=True).stdout.strip('\n')
    finally:
        shutil.rmtree(tmp)


def build_test_image(base_image):
    """Same kernel, file system pre-loaded with sample programs + tests/c/*."""
    tmp = tempfile.mkdtemp(prefix='nixdos-img-')
    progs = os.path.join(tmp, 'programs')
    os.mkdir(progs)
    for d in (os.path.join(ROOT, 'src', 'programs'), os.path.join(HERE, 'c')):
        for f in os.listdir(d):
            shutil.copy(os.path.join(d, f), progs)
    out = os.path.join(tmp, 'test.img')
    subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'mkimage.py'),
                    os.path.join(ROOT, 'build', 'boot.bin'),
                    os.path.join(ROOT, 'build', 'kernel.bin'), progs, out],
                   check=True, stdout=subprocess.DEVNULL)
    return tmp, out


def test_boot(vm):
    print('boot')
    vm.boot()
    out = vm.output()
    check('banner', 'WELCOME TO NIXDOS' in out, out[:500])
    check('A20 and protected mode', 'A20 enabled' in out)
    check('disk detected', 'disk: QEMU HARDDISK' in out, out)
    check('ver', 'NixDOS 2.0' in vm.run('ver'))
    check('help lists cc', 'Compile C' in vm.run('help'))
    check('unknown command', 'Erroneous command' in vm.run('frobnicate'))


def test_compiler(vm):
    print('compiler (differential vs host gcc)')
    for t in DIFF_TESTS:
        src = os.path.join(HERE, 'c', t)
        if not os.path.exists(src):
            continue
        want = host_output(src)
        got = vm.run('cc ' + t, timeout=60)
        check('cc ' + t, got == want, '--- nixdos:\n%s\n--- gcc:\n%s' % (got, want))

    print('executables')
    out = vm.run('cc t_ctrl.c -o ctrl.nxe')
    check('cc -o writes executable', '-> ctrl.nxe' in out, out)
    want = host_output(os.path.join(HERE, 'c', 't_ctrl.c'))
    check('run ctrl.nxe', vm.run('run ctrl.nxe') == want)
    check('run by name', vm.run('ctrl') == want)
    out = vm.run('cc hello.c -o hello.nxe')
    out = vm.run('hello one "two words"')
    check('argv passing', 'argv[1] = one' in out and 'argv[2] = two words' in out, out)

    print('compiler errors')
    out = vm.run('cc t_err1.c')
    check('undeclared variable', "t_err1.c:4: error: 'y' undeclared" in out, out)
    out = vm.run('cc t_err2.c')
    check('missing semicolon', "t_err2.c:4: error: expected ';'" in out, out)
    out = vm.run('cc t_err3.c')
    check('undefined function', "undefined function 'helper'" in out, out)
    out = vm.run('cc nosuch.c')
    check('missing source', 'no such file' in out, out)

    print('fault isolation')
    out = vm.run('cc t_crash.c')
    check('divide by zero caught', 'before' in out and 'Divide error' in out and
          'not reached' not in out, out)
    check('shell alive after crash', 'NixDOS 2.0' in vm.run('ver'))

    start = len(vm.output())
    vm.send_raw('cc t_loop.c\r')
    vm.wait_for('spinning', 20, start)
    vm.send_raw('\x03')
    idx = vm.wait_for('^C', 20, start)
    vm.wait_for('$] ', 20, idx)
    check('Ctrl+C stops infinite loop', True)
    check('shell alive after Ctrl+C', 'NixDOS 2.0' in vm.run('ver'))


def test_samples(vm):
    print('sample programs')
    out = vm.run('cc primes.c')
    check('primes.c', '2 3 5 7 11 13' in out and '997' in out, out)
    out = vm.run('cc fib.c')
    check('fib.c', '6765' in out, out)
    out = vm.run('cc sort.c')
    check('sort.c', 'sorted: yes' in out, out)
    out = vm.run('cc strings.c')
    check('strings.c', 'NIXDOS' in out and 'palindrome' in out, out)
    out = vm.run('cc files.c')
    check('files.c', 'read back: written by a NixC program' in out, out)
    out = vm.run('cc queens.c')
    check('queens.c', '92 solutions' in out, out)
    out = vm.run('cc mandel.c')
    check('mandel.c', '#' in out and len(out.splitlines()) >= 20, out)
    start = len(vm.output())
    vm.send_raw('cc calc.c\r')
    vm.wait_for('calc> ', 20, start)
    vm.send_raw('2 + 3 * (4 - 1)\r')
    idx = vm.wait_for('= 11', 20, start)
    vm.send_raw('100 / 7 % 4\r')
    idx = vm.wait_for('= 2', 20, idx)
    vm.send_raw('quit\r')
    vm.wait_for('$] ', 20, idx)
    check('calc.c (interactive)', True)
    start = len(vm.output())
    vm.send_raw('cc guess.c\r')
    vm.wait_for('Your guess: ', 20, start)
    vm.send_raw('\x03')
    vm.wait_for('$] ', 20, start)
    check('guess.c (Ctrl+C at input)', True)


def test_files(vm):
    print('file system')
    vm.run('echo hello world > note.txt')
    check('echo > file / cat', vm.run('cat note.txt') == 'hello world')
    vm.run('echo second line >> note.txt')
    check('echo >> appends', vm.run('cat note.txt') == 'hello world\nsecond line')
    vm.run('cp note.txt copy.txt')
    check('cp', vm.run('cat copy.txt') == 'hello world\nsecond line')
    vm.run('mv copy.txt moved.txt')
    out = vm.run('ls')
    check('mv', 'moved.txt' in out and 'copy.txt' not in out, out)
    vm.run('rm moved.txt')
    check('rm', 'moved.txt' not in vm.run('ls'))
    check('cat missing', 'no such file' in vm.run('cat moved.txt'))
    out = vm.run('hexdump note.txt')
    check('hexdump', '68 65 6c 6c 6f' in out, out)

    print('editor')
    start = len(vm.output())
    vm.send_raw('edit made.c\r')
    vm.send_raw('int main() {\r')
    vm.send_raw('printf("edited %d\\n", 6 * 7);\r')
    vm.send_raw('}\r')
    vm.send_raw('\x13')        # Ctrl+S
    vm.send_raw('\x11')        # Ctrl+Q
    vm.wait_for('$] ', 20, start)
    out = vm.run('cat made.c')
    check('editor saves file (with auto-indent)', 'int main() {\n    printf' in out, out)
    check('compile edited file', vm.run('cc made.c') == 'edited 42')


def test_misc(vm):
    print('system commands')
    check('time', 'Current time is' in vm.run('time'))
    check('date', "Today's date is" in vm.run('date'))
    out = vm.run('mem')
    check('mem', 'Kernel heap' in out, out)
    out = vm.run('equip')
    check('equip', 'CPU' in out and 'A20 line' in out, out)
    check('uptime', vm.run('uptime').startswith('up '))


def test_persistence(image):
    print('persistence across reboot')
    vm = NixDOS(image, keep_image=True)
    try:
        vm.boot()
        vm.run('echo persistent data > keep.txt')
        vm.send_raw('shutdown\r')
        vm.proc.wait(timeout=20)
        check('shutdown powers off', vm.proc.returncode is not None)
    finally:
        vm.close()
    vm = NixDOS(image, keep_image=True)
    try:
        vm.boot()
        check('file survives reboot', vm.run('cat keep.txt') == 'persistent data')
    finally:
        vm.close()


def main():
    image = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'build', 'nixdos2.img')
    if not os.path.exists(image):
        sys.exit('image not found: %s (run make first)' % image)
    tmp, test_image = build_test_image(image)
    vm = NixDOS(test_image)
    try:
        for t in (test_boot, test_compiler, test_samples, test_files, test_misc):
            try:
                t(vm)
            except Exception:
                failed.append(t.__name__)
                traceback.print_exc()
                print(vm.output()[-2000:])
    finally:
        vm.close()
    try:
        test_persistence(test_image)
    except Exception:
        failed.append('test_persistence')
        traceback.print_exc()
    shutil.rmtree(tmp, ignore_errors=True)

    print('\n%d passed, %d failed' % (passed, len(failed)))
    if failed:
        print('failed: ' + ', '.join(failed))
        sys.exit(1)


if __name__ == '__main__':
    main()
