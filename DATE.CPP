#include<stdio.h>
#include<dos.h>
#include<conio.h>
//#include "c:\tc\bin\string.cpp"
void main()
{
   clrscr();
   unsigned char sec,hr,min,csec,chr,cmin;
   while(1)
   {
   delay(10);
   clrscr();
   asm  mov ah,2
   asm int 0x1a
   asm mov sec,dh
   asm mov hr,ch
   asm mov min,cl
  // chr=hr-11;
   printf("\nseconds=%x",sec);
   printf("\nhour= %x ",hr);
   printf("\nmin= %x",min);
      }
   getch();
   }
