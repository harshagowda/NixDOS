#include "DRAW.CPP"
#include "STRING.CPP"
#include "WELCOME.CPP"
#include "COMMANDS.CPP"
#include "DT&TIME.CPP"
#include "EQUIP.CPP"
#include "EDITOR.CPP"
int xc=3,yc=0;          //x coordinate & y coordinate
void help()
{
  if(yc>10)
 {
    scrollpgup((13-(24-yc)),0,0,24,79);
  xc=3;
  yc=10;
   }

print("  List of commands...",0,++yc);
print("    help   -> List of cmds",0,++yc);
print("    clr    -> Clears screen",0,++yc);
print("    vers   -> Version of Operating System",0,++yc);
print("    time   -> Display the current time reading it from BIOS",0,++yc);
print("    ctime  -> Set to new time ",0,++yc);
print("	   date   -> Display the current date reading it from BIOS",0,++yc);
print("    cdate  -> Set to new date ",0,++yc);
print("    clock  -> Real time clock",0,++yc);
print("    ccolor -> Change text Background color",0,++yc);
print("    ndedit -> A mini TEXT EDITOR for mini O.S",0,++yc);
print("    prtmsg -> Print the  message ",0,++yc);
print("    prtscr -> Print the  screen contents(hit print screen key) ",0,++yc);
print("    equip  -> Print equipment list",0,++yc);
print("    rboot  -> Reboot",0,++yc);
print("    sdown  -> Shutdown",0,++yc);
print("  There will be more commands to see in future....",0,++yc);
++yc;
}

void main()
{
welcome();
clear();
setmode(0x3);
clear();			     //command interpreter


unsigned char *x,cmd[80];
int cmdi=0;
   scrollpgup(0,0,0,24,79);
   clear();
   setcolor(0,0,0);
   while(*x!=27)           //keeps the shell alive
   {

    alive:
	   if(yc==24)
	    {
	       lift1up();
	       yc--;
	    }
	  xc=0;
	  yc++;
	 // gotoxy(10,5);
	  cmdi=0;

	  print(" $] ",xc,yc);
	  xc=xc+3;
	  gotoxy(xc,yc);
	  *x=getch();

	  if(*x==13) goto alive;     //enter
	  if(*x==27) break;          //exit
	  if(*x==8) {print(" ",xc,yc);}
	  if(*x!=8)   //other than bk space store to cmd
	  {
	  cmd[cmdi]=*x; cmdi++;       //copy the key to cmd array
	  *(x+1)='\0';
	  *(x+2)='\0';
	  *(x+3)='\0';
	  *(x+4)='\0';
	  print(x,xc,yc);
	  setcurpos(++xc,yc);
	  }
	  *x=getch();
	  if(*x==8)
	  {print(" ",xc,yc);}

	  while(*x!=13)
	  {
	     if(*x==8&&xc>3)
	     {
		 if(*x==4) {print(" ",xc,yc);}
	      cmd[--cmdi]='\0';
	      print(" ",xc-1,yc);
	      xc--;
	      setcurpos(xc,yc);
	      }
	      else
	      {
		gotoxy(xc,yc);
		print(x,xc,yc);
		cmd[cmdi]=*x;
		cmdi++;

		setcurpos(++xc,yc);
		}
		*x=getch();

	     }    //end of while

	     int c=0;
	     //c=retint(cmd);

 /*help-1*/   if(((cmd[cmdi-1]=='p' && cmd[cmdi-2]=='l' && cmd[cmdi-3]=='e' && cmd[cmdi-4]=='h') || (cmd[cmdi-1]=='P' && cmd[cmdi-2]=='L' && cmd[cmdi-3]=='E' && cmd[cmdi-4]=='H')) || cmdi==4) c=1;
 /*clr-2*/    if(((cmd[cmdi-1]=='r' && cmd[cmdi-2]=='l' && cmd[cmdi-3]=='c' ) || (cmd[cmdi-1]=='R' && cmd[cmdi-2]=='L' && cmd[cmdi-3]=='C')) && cmdi==3) c=2;
/*time-3*/    if(((cmd[cmdi-1]=='e' && cmd[cmdi-2]=='m' && cmd[cmdi-3]=='i' && cmd[cmdi-4]=='t') || (cmd[cmdi-1]=='E' && cmd[cmdi-2]=='M' && cmd[cmdi-3]=='I' && cmd[cmdi-4]=='T')) && cmdi==4) c=3;
/*ctime-4*/   if(((cmd[cmdi-1]=='e' && cmd[cmdi-2]=='m' && cmd[cmdi-3]=='i' && cmd[cmdi-4]=='t'&& cmd[cmdi-5]=='c') || (cmd[cmdi-1]=='E' && cmd[cmdi-2]=='M' && cmd[cmdi-3]=='I' && cmd[cmdi-4]=='T'&& cmd[cmdi-5]=='C')) &&cmdi==5) c=4;
/*date-5*/    if(((cmd[cmdi-1]=='e' && cmd[cmdi-2]=='t' && cmd[cmdi-3]=='a' && cmd[cmdi-4]=='d') || (cmd[cmdi-1]=='E' && cmd[cmdi-2]=='T' && cmd[cmdi-3]=='A' && cmd[cmdi-4]=='D')) &&cmdi==4) c=5;
/*cdate-6*/   if(((cmd[cmdi-1]=='e' && cmd[cmdi-2]=='t' && cmd[cmdi-3]=='a' && cmd[cmdi-4]=='d'&& cmd[cmdi-5]=='c') || (cmd[cmdi-1]=='E' && cmd[cmdi-2]=='T' && cmd[cmdi-3]=='A' && cmd[cmdi-4]=='D'&& cmd[cmdi-5]=='C')) &&cmdi==5) c=6;
/*vers-7*/    if(((cmd[cmdi-1]=='s' && cmd[cmdi-2]=='r' && cmd[cmdi-3]=='e' && cmd[cmdi-4]=='v') || (cmd[cmdi-1]=='S' && cmd[cmdi-2]=='R' && cmd[cmdi-3]=='E' && cmd[cmdi-4]=='V')) &&cmdi==4) c=7;
/*rboot-8*/   if(((cmd[cmdi-1]=='t' && cmd[cmdi-2]=='o' && cmd[cmdi-3]=='o' && cmd[cmdi-4]=='b'&& cmd[cmdi-5]=='r') || (cmd[cmdi-1]=='r' && cmd[cmdi-2]=='O' && cmd[cmdi-3]=='O' && cmd[cmdi-4]=='B'&& cmd[cmdi-5]=='R')) &&cmdi==5) c=8;
/*sdown-9*/   if(((cmd[cmdi-1]=='n' && cmd[cmdi-2]=='w' && cmd[cmdi-3]=='o' && cmd[cmdi-4]=='d'&& cmd[cmdi-5]=='s') || (cmd[cmdi-1]=='N' && cmd[cmdi-2]=='W' && cmd[cmdi-3]=='O' && cmd[cmdi-4]=='D'&& cmd[cmdi-5]=='S')) &&cmdi==5) c=9;
/*clock-10 */ if(((cmd[cmdi-1]=='k' && cmd[cmdi-2]=='c' && cmd[cmdi-3]=='o' && cmd[cmdi-4]=='l'&& cmd[cmdi-5]=='c') || (cmd[cmdi-1]=='K' && cmd[cmdi-2]=='C' && cmd[cmdi-3]=='O' && cmd[cmdi-4]=='L'&& cmd[cmdi-5]=='C')) &&cmdi==5) c=10;
/*ccolor-11*/ if(((cmd[cmdi-1]=='r' && cmd[cmdi-2]=='o' && cmd[cmdi-3]=='l' && cmd[cmdi-4]=='o'&& cmd[cmdi-5]=='c' && cmd[cmdi-6]=='c') || (cmd[cmdi-1]=='R' &&cmd[cmdi-2]=='O' && cmd[cmdi-3]=='L' && cmd[cmdi-4]=='O' && cmd[cmdi-5]=='C'&& cmd[cmdi-6]=='C')) &&cmdi==6) c=11;
/*prtmsg-12*/ if(((cmd[cmdi-1]=='g' && cmd[cmdi-2]=='s' && cmd[cmdi-3]=='m' && cmd[cmdi-4]=='t'&& cmd[cmdi-5]=='r' && cmd[cmdi-6]=='p') || (cmd[cmdi-1]=='G' &&cmd[cmdi-2]=='S' && cmd[cmdi-3]=='M' && cmd[cmdi-4]=='T' && cmd[cmdi-5]=='R'&& cmd[cmdi-6]=='P')) &&cmdi==6) c=12;

/*clock-13 */ if(((cmd[cmdi-1]=='p' && cmd[cmdi-2]=='i' && cmd[cmdi-3]=='u' && cmd[cmdi-4]=='q'&& cmd[cmdi-5]=='e') || (cmd[cmdi-1]=='P' && cmd[cmdi-2]=='I' && cmd[cmdi-3]=='U' && cmd[cmdi-4]=='Q'&& cmd[cmdi-5]=='E')) &&cmdi==5) c=13;
/*prtscr-14*/ if(((cmd[cmdi-1]=='r' && cmd[cmdi-2]=='c' && cmd[cmdi-3]=='s' && cmd[cmdi-4]=='t'&& cmd[cmdi-5]=='r' && cmd[cmdi-6]=='p') || (cmd[cmdi-1]=='R' &&cmd[cmdi-2]=='C' && cmd[cmdi-3]=='S' && cmd[cmdi-4]=='T' && cmd[cmdi-5]=='R'&& cmd[cmdi-6]=='P')) &&cmdi==6) c=14;
/*ndedit-15*/ if(((cmd[cmdi-1]=='t' && cmd[cmdi-2]=='i' && cmd[cmdi-3]=='d' && cmd[cmdi-4]=='e'&& cmd[cmdi-5]=='d' && cmd[cmdi-6]=='n') || (cmd[cmdi-1]=='T' &&cmd[cmdi-2]=='I' && cmd[cmdi-3]=='D' && cmd[cmdi-4]=='E' && cmd[cmdi-5]=='D'&& cmd[cmdi-6]=='N')) &&cmdi==6) c=15;
	      switch(c)
	      {


	      case 1:                 //help
			help();
			break;

	     case 2:                 //clr
			scrollpgup(0,0,0,24,79);
			xc=3;
			yc=0;
			break;
					//show time
	     case 3:
		       if(yc>22)  {scrollpgup(3,0,0,24,79); yc=21;}
		       print("Current time is........",xc,++yc);          //time
		      yc=time(yc);
		       break;

					 //change time
	     case 4:
			if(yc>22)  {scrollpgup(3,0,0,24,79); yc=21;}
			print("Now time is........",xc,++yc);          //time
			yc=time(yc);
			yc=settime(yc);
			break;

					//current date
	      case 5:
		       if(yc>22)  {scrollpgup(3,0,0,24,79); yc=21;}
		       print("Todays date is........",xc,++yc);          //time
		       yc=date(yc);
		       break;

				       //change date
	     case 6:
			if(yc>22)  {scrollpgup(3,0,0,24,79); yc=21;}
			print("System date is........",xc,++yc);          //time
			yc=date(yc);
			yc=setdate(yc);
			break;


					 //display version of OS
	     case 7:
			if(yc>22)  {scrollpgup(3,0,0,24,79); yc=21;}
			print("Version : NIXDOS-1.0",xc,++yc);
			yc++;
			break;

					//reboot
	     case 8:
			asm int 0x19
			break;


					//sdown
	     case 9:
			shutdown();
			break;


	     case 10:        //clock
			 int temp=yc;
			if(yc>22)  {scrollpgup(5,0,0,24,79); yc=21;}
			while(!kbhit())
			{

		       print("Current time is........",xc,++yc);          //time
		       delay(1);
		      yc=time(yc);
			  yc=temp;
			  }
			  usc a=getch();
			  yc=yc+3;
			break;


	     case 11:   changecolor();
			yc=0;
			break;

	     case 12: prtmsg(yc);
		       yc=yc+4;
		       if(yc>22)  {scrollpgup(3,0,0,24,79); yc=21;}
		       break;

	     case 13:
			yc=equip(yc);
			yc++;
			break;


	    case 14:    initialize_prtr();
			asm int 0x9
			break;


	    case 15:    yc=ndedit();
			break;

	    case 0:
			if(yc==24)
			 {
			  scrollpgup(2,0,0,24,79);
			  xc=3;
			  yc=22;
			 }
			print("  Erreneous command or file name,Type help",0,++yc);
			++yc;
			break;
	   }


	if(c==9) { delay(100);break;}

   }//end of while
}


