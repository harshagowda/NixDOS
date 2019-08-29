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
	  case 3: return('3');
		  break;
	  case 4: return('4');
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

int retint(usc a)
{
      switch(a)
      {
	  case '0': return(0);
		  break;
	  case '1': return(1);
		  break;
	  case '2': return(2);
		  break;
	  case '3': return(3);
		  break;
	  case '4': return(4);
		  break;
	  case '5': return(5);
		  break;
	  case '6': return(6);
		  break;
	  case '7': return(7);
		  break;
	  case '8': return(8);
		  break;
	  case '9': return(9);
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


     asm mov mlt,1

     for( i=1;i<=8;i++)
     {
	asm mov ah,mlt
	asm and ah,d
	asm mov se,ah

	a[i]=se;
	if(a[i]>0) a[i]=1;
	mlt=mlt*2;
     }

       o=a[1]*1 + a[2]*2 + a[3]*4 + a[4]*8;
       t=a[5]*1 + a[6]*2 + a[7]*4 + a[8]*8;

       printint(t,x,y);
       printint(o,x+1,y);
}




int time(int yc)
{
   usc sec,hr,min,dlval;

   yc++;
   asm{
	mov ah,0x02
	int 0x1a

	mov sec,dh
	mov min,cl
	mov hr,ch
	}
	BCD_to_int(hr,3,yc);
	print(":",5,yc);
	BCD_to_int(min,6,yc);
	print(":",8,yc);
	BCD_to_int(sec,9,yc);

	yc++;
	return(yc);
}


int date(int yc)
{
   usc day,month,century,year;

   yc++;
   asm{
	mov ah,0x04
	int 0x1a

	mov day,dl
	mov month,dh
	mov century,ch
	mov year,cl
	}
	BCD_to_int(day,3,yc);
	print("/",5,yc);
	BCD_to_int(month,6,yc);
	print("/",8,yc);
	BCD_to_int(century,9,yc);
	BCD_to_int(year,11,yc);


	yc++;
	return(yc);
}

usc ascii_to_BCD(usc a,usc b)
{
  int ia,ib,aia[4],aib[4];


   ib=retint(b);

   for(int i=4;i>=1;i--)
   {
    aib[i]=ib%2;
    ib=ib/2;
    }

    usc nval=0X00;
    int mult=1;
    for(i=4;i>=1;i--)
    {
       if(aib[i]==1) nval=nval|mult;
       mult=mult*2;
     }


     ia=retint(a);


  for(i=4;i>=1;i--)
  {
   aia[i]=ia%2;
   ia=ia/2;
   }

     for(i=4;i>=1;i--)
     {
	if(aia[i]==1) nval=nval|mult;
	mult=mult*2;
     }
     printf("\n* nval=%0x",nval);
     return(nval);
}

int main()
{

  int yc=2;
  clrscr();
  yc=time(yc);
  getch();
  usc h1,h2,b1,m1,m2,b2,s1,s2,nhr,nmin,nsec;
  print("Enter new time(hh:mm:ss): ",5,yc);
  h1=getch(); printa(h1,32,yc);
  h2=getch(); printa(h2,33,yc);
  b1=getch(); printa(b1,34,yc);
  m1=getch(); printa(m1,35,yc);
  m2=getch(); printa(m2,36,yc);
  b2=getch(); printa(b2,37,yc);
  s1=getch(); printa(s1,38,yc);
  s2=getch(); printa(s2,39,yc);

   nhr=ascii_to_BCD(h1,h2);
   printf("\nnhr=%0x ",nhr);
   nmin=ascii_to_BCD(m1,m2);
   nsec=ascii_to_BCD(s1,s2);
    yc++;
   printf("\nnhr=%0x nmin=%0x nsec=%0x",nhr,nmin,nsec);
   // print("new r values are",5,20);


   asm {
	mov ah,0x03
	mov ch,nhr
	mov cl,nmin
	mov dh,nsec
	mov dl,0
	int 0x1a
	}

  /*   if(yc>22)  {scrollpgup(3,0,0,24,79); yc=21;}
			print("Current time is........",3,++yc);          //time
			yc=time(yc); */
      yc=time(yc);
   return(yc);
}
