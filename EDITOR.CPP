typedef unsigned char usc;
#define ESC 27
#define BKSPACE 8
#define ENTER 13
#define TAB 9
#define UP 72
#define DOWN 80
#define LEFT 75
#define RIGHT 77
#define DEL 83
#define INSERT 128
#define F1 59
#define F2 60
#define F3 61
#define F4 64

void ndedithelp()
{
}

void open()
{
}

void save()
{
}

void eprint()
{
}


int ndedit()
{

   int x=0,y=2,maxx=79,maxy=22,minx=0,miny=2,exit=0;
   scrollpgup(0,0,0,24,79);

   print("A mini EDITOR for MINI O.S",20,0);
   print("ESC ->Quit  F1 ->Help F2 ->Save F3 ->Open F4 ->Print",2,24);
   setcurpos(0,y);
   usc c[1760];
    for(int i=1;i<=1760;)
      {

      c[i]=getch();

	  switch(c[i])
	  {
	     case ESC:   exit=1;        //esc
			 scrollpgup(0,0,0,24,79);
			 break;
	  case BKSPACE:
			//if(x==0&&y==1)
			if(x==0) {print(" ",maxx,--y);x=maxx;c[--i]='\0';setcurpos(x,y);}
			if(x==1) {print(" ",--x,y);c[--i]='\0';setcurpos(x,y);}
			if(x>0&&y>1&&y<=23)  {c[--i]='\0';print(" ",--x,y);}

			setcurpos(x,y);
			break;

	     case ENTER:
			if(y==24) y--;
			y++;
			c[i++]='\n';
			x=0;
			setcurpos(x,y);
			break;

	     case UP:
			if(y==2) y=3;
			setcurpos(x,--y);
			break;


	     case DOWN:
			if(y==23) y=22;
			setcurpos(x,++y);
			break;

	     case LEFT:
			if(x==0) x=1;
			setcurpos(--x,y);
			break;

	     case RIGHT:
			 if(x==79) x=78;
			 setcurpos(x++,y);

			break;

	     case DEL:
			 print(" ",x,y);
			break;

	     case TAB:
			 x=x+5;
			 setcurpos(x,y);
			break;

	     case INSERT:

			break;

	     case F1:
			ndedithelp();
			break;

	     case F2:
			save();
			break;

	     case F3:
			open();
			break;

	     case F4:
			eprint();
			break;


	     default :   if(x==79) {y++;x=0;}
			printa(c[i++],x++,y);
			setcurpos(x,y);
			break;
      }

      if(exit==1) break;
      }
   return(0);
 }