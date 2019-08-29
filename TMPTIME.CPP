#include "c:\tc\bin\string.cpp"
#include<stdio.h>
#include<conio.h>
typedef unsigned char usc;
int retascii(int a)
{
      switch(a)
      {
	  case 0: return('0');
		  break;
	  case 1: return('1');
		  break;
	  case 2: return('2');
		  break;
	  case 4: return('3');
		  break;
	  case 5: return('5');
		  break;
	  case 6: return('6');
		  break;
	  case 7: return('7');
		  break;
	  case 8: return('8');
		  break;
	  case 9: return('9');
		  break;
	 }
}

void printint(int a,usc x,usc y)
{
    usc asc_a=retascii(a);
    printa(asc_a,x,y);
}

void BCD_to_int(usc d,usc x,usc y)
{

     usc mlt,se;
     int i,a[8],o,t;

     printf("\nat bcd d=->%0x",d);
     asm mov mlt,0x01
     asm mov ch,d
    // printf("\n\n\n\n\n\n\------------\n");
     printf("\n");
     for( i=1;i<=8;i++)
     {
	asm mov ah,mlt;
	asm and ah,ch;
	asm mov se,ah;
	printf(" se=%0x",se);
	a[i]=se;
	if(a[i]>0) a[i]=1;
      //	printf("\n *     %d",a[i]);
	mlt=mlt*2;
     }
      /* printf("\n\n\n\n-------------\n");
       for(i=1;i<=8;i++)
       printf("\n%d",a[i]);*/

       o=a[1]*1 + a[2]*2 + a[3]*4 + a[4]*8;
       t=a[5]*1 + a[6]*2 + a[7]*4 + a[8]*8;

       printint(o,x,y);
       printint(t,x+1,y);
}




void main()
{
   usc sec,hr,min,dlval;

   int yc=2;
  clrscr();
   asm{
	mov ah,0x02
	int 0x1a

	mov sec,dh
	mov min,cl
	mov hr,ch
	}

	printf("\n\n\n\hr=%0x min=%0x sec=%0x ",hr,min,sec);
	getch();
	BCD_to_int(hr,3,yc);
	print(":",5,yc);
	BCD_to_int(min,6,yc);
	print(":",8,yc);
	BCD_to_int(sec,9,yc);

	yc++;
       //	return(yc);
}