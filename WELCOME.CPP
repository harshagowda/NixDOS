


void welcome()
{

clear();		       //PAGE 1
setmode(0x06);
setcolor(0x17,0x30,0x50);
rectangle(0,0,635,195);
rectangle(5,5,630,190);

print("***************  WELCOME TO NIXDOS ***************",15,2);
print("  RURAL ENGINEERING COLLEGE  ,BHALKI",20,6);
print("Project Name  ->NIXDOS ( A mini  Operating System )",10,15);
print("Project Team  -> 1> Harsha",10,17);
print("              -> 2> Mahesh",10,18);
print("              -> 3> Akash",10,19);
print("              -> 4> Pavan",10,20);
print("Project Guide ->Mr.Krishnamurthy",10,22);
getch();
			//page2  loading.....
clear();
setmode(0x06);
setcolor(0x80,0x00,0xff);
rectangle(0,0,635,195);
rectangle(5,5,630,190);
print("Wait NIXDOS's  shell is loading...... ",25,2);
print("        This may take few seconds...",20,15);

for(int i=10;i<=70;i++)
printa(176,i,20);         //base

for(i=10;i<=70;i++)
{
printa(219,i,20);        //cover
//delay(1);
}

clear();

}

void shutdown()
{

clear();
setmode(0x06);
setcolor(0x80,0x00,0xff);
rectangle(0,0,635,195);
rectangle(5,5,630,190);
print("Wait NIXDOS's  shell is Unloading...... ",25,2);
print("        This may take few seconds...",20,15);

for(int i=10;i<=70;i++)
printa(219,i,20);         //base

for(i=70;i>=10;i--)
{
printa(176,i,20);        //cover
delay(1);
}

print("   NOW IT IS SAFE TO TURN OFF YOUR COMPUTER",20,10);
delay(1);
   asm{
	mov ah,0x42
	mov al,00
	int 0x15
	}

return;
}
