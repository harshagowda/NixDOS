 typedef unsigned char usc;
 usc ascii_to_BCD(usc bk,usc txt);
 void prtscr();
 void prtchar(usc prt);
 void equp();
 void initialize_prtr();


 void initialize_prtr()
 {
     asm{
	mov ah,0x01
	mov dx,0
	int 0x17
       }
 }

 void prtchar(usc prt)
 {


       asm{
		mov ah,0x00
		mov al,prt
		mov dx,0
		int 0x17

	   }

		  /*

      asm{
		mov ah,0x0e
		mov al,prt
		mov dx,0

	   }

		      */



     }


   void prtmsg(int yc)
   {

    print("Enter the char to print(hit esc to stop editing): ",2,++yc);
    usc c[1000];
    int x=1,exit=0;
    yc++;
    setcurpos(x,yc);
    for(int i=1;i<=1000;)
      {

      c[i]=getch();

	  switch(c[i])
	  {
	     case 27:   exit=1;        //esc
			break;
	     case 8:
			c[--i]='\0';
			print(" ",--x,yc);
			setcurpos(x,yc);
			break;

	     case 13:
			yc++;
			c[i++]='\n';
			x=1;
			setcurpos(x,yc);
			break;

	     default :
			printa(c[i++],x++,yc);
			setcurpos(x,yc);
			break;
      }

      if(exit==1) break;
      }
    initialize_prtr();

    for(int j=1;j<=i;j++)
    prtchar(c[j]);
     return;

  }

int changecolor()
{
scrollpgup(0,0,0,24,79);
int x=1,y=1;
rectangle(0,0,635,195);
rectangle(5,5,630,190);
print("                To change color enter two numbers where...",x,++y);
y++;
print(" 1>BACKGROUND",x,++y);
y++;
print(" 2>TEXTCOLOR ",x,++y);

unsigned char bk,txt;
y=y+4;
print("  0 ->BLACK                          1 ->BLUE",x,++y);
print("  2 ->GREEN                          3 ->CREAM",x,++y);
print("  4 ->RED                            5 ->PINK",x,++y);
print("  6 ->ORANGE                         7 ->WHITE",x,++y);
print("  8 ->BLINK BACKGROUND               9 ->BLINK TEXT(BKG BLUE)",x,++y);
print("  a ->BLINK TEXT(BKG GREEN)          b ->BLINK TEXT(BKG CREAM)",x,++y);
print("  c ->BLINK TEXT(BKG RED)            d ->BLINK TEXT(BKG PINK)",x,++y);
print("  e ->BLINK TEXT(BKG ORANGE)         f ->BLINK TEXT(BKG WHITE)",x,++y);
y++;
print("Enter the background color code:",x,++y);
bk=getch();
setcurpos(36,y);
printa(bk,36,y);
print("Enter the font color code:",x,++y);
setcurpos(30,y);
txt=getch();
printa(txt,36,++y);

 ATTRIB=ascii_to_BCD(bk,txt);

scrollpgup(0,0,0,24,79);

return 0;
}