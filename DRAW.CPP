
typedef unsigned char usc;
usc ATTRIB=0x07;
void putpoint(int x,int y)
{
	asm{
		mov cx,x
		mov dx,y
		mov ah,0x0c
		mov al,0x15 // pixel value
		mov bh,0x00  //page
		int 0x10
	    }
 }


void setmode(usc m)
{
	asm{
		mov ah,0x00
		mov al,m
		int 0x10
	    }
}


void setcolor(usc red,usc green,usc blue)
{
	asm{
		mov ah,0x10
		mov al,0x10
		xor bx,bx
		mov ch,red
		mov cl,green
		mov dh,blue
		int 0x10
	    }
}

void whiteonblack()
{
	asm{
		mov ah,0xf
		mov cx,7
		int 0x10
	      }
}

void setcurpos(usc x,usc y)
{
	asm{
		mov ah,0x02
		mov bh,0x0
		mov dh,y
		mov dl,x
		int 0x10
	    }
}

void gotoxy(usc x,usc y)
{
x=(x>=1&&x<=80)?x:1;
y=(y>=1&&y<=25)?y:1;
setcurpos(x,y);
}


void scrollpgup(usc nl,usc tr,usc lc,usc br,usc rc)
{
	asm{
		mov ah,0x06
		mov al,nl      //no of lines to scroll(0 clears win)
		mov bh,ATTRIB    //15 char attrib for new win
		mov ch,tr     //top row of scroll win
		mov cl,lc      //left column of scroll win
		mov dh,br     //bottom row of scroll win
		mov dl,rc     //right column of scroll win
		int 0x10
	}
}

void lift1up()
{
scrollpgup(1,0,0,24,79);
}

void clear()
{
asm{
     mov ah,0x06
     mov al,30
     mov bh,0x1
     mov bl,0x01
     mov ch,0
     mov cl,0
     mov dh,0
     mov dl,79
   }
}


/*
void clear()
{
  scrollpgup(0,0,0,0,79);
}
*/
void scrollpgdown(usc nl,usc tr,usc lc,usc br,usc rc)
{
	asm{
		mov ah,0x07
		mov al,nl      //no of lines to scroll(0 clears win)
		mov bh,ATTRIB    //char attrib for new win
		mov ch,tr     //top row of scroll win
		mov cl,lc      //left column of scroll win
		mov dh,br     //bottam row of scroll win
		mov dl,rc     //right column of scroll win
		int 0x10
	   }
}

void line(int x0,int y0,int x1,int y1)
{
int dx,dy,d,x,y,incrE,incrNE;
dx=x1-x0;
dy=y1-y0;
d=2*dy-dx;
incrE=2*dy;
incrNE=2*(dy-dx);
x=x0,y=y0;
putpoint(x,y);
  if(x0==x1)
  {
     for(y=y0;y<y1;y++)
     putpoint(x,y);
   }
   else
   {
    while(x<=x1)
    {
      if(d<=0)
      {
	d=d+incrE;
	x++;
      }
      else
      {
      d=d+incrNE;
      x++;
      y++;
      }
      putpoint(x,y);
  }
}
}
void rectangle(int x0,int y0,int x1,int y1)
{
    line(x0,y0,x1,y0); //top rt
    line(x1,y0,x1,y1); //rt dwn
    line(x0,y1,x1,y1); //btw rt
    line(x0,y0,x0,y1); //lft dwn
}

/*void drawpoly(int a[])
{
}


void circle(int x,int y,int r)
{
}

void ellipse(int x,int y,int mj,int mn,int st,int ed)
{
}

void fillrect(int x0,int y0,int x1,int y1,int clr)
{
}

void semicircle(int x,int y,int r)
{
} */