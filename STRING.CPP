
typedef unsigned char usc;
void delay(int d)
{                        //d=no. of microseconds to wait
	  asm{
	      mov ah,0x86
	      mov cx,d
	      mov dx,d
	      int 0x15

	       }
}


void printc(char c)
{
   asm{
	mov ah,0x09
	mov al,c
	mov bh,0x0
	mov bl,ATTRIB  //bk & fnt color
	mov cx,1
	int 0x10
	}
}


void printa(usc c,usc x,usc y)
{
    setcurpos(x,y);
   asm{
	mov ah,0x09
	mov al,c
	mov bh,0x0
	mov bl,ATTRIB
	mov cx,1
	int 0x10
	}
}

void print(char *s,int x,int y)
{
 setcurpos(x,y);

   while(*s)
   {
     if(*s=='\n')
     {
       printc('\n');
       printc('\r');
       ++s;
       gotoxy(0,++y);
      }
      else
       {
	 gotoxy(x++,y);
	 printc(*s);
	 s++;
       }
   } //end of while
}

usc getch()
{
usc x;
asm{
    mov ah,0x00
    int 0x16
    mov x,al
    }
    return x;
}

int kbhit()
{
 unsigned char i;
asm mov ah,0x01
asm int 0x16

asm lahf
asm and ah,64
asm mov i,ah
return((i==0)?1:0);
}

/*
int strlen(char *s)
{
}

void setattrib()
{
}
  */