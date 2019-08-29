//#include "c:\tc\bin\string.cpp"
  int equip(int yc)
  {
  {
     int mult=0x1,temp,a[16],b[16];
     int xc=3;
     asm int 0x11;
     asm mov temp,ax;

     for(int i=0;i<=15;i++)
     {
	 a[i]=temp&mult;
	 mult*=2;

	 if(a[i]>0)
		a[i]=1;
	 else
		a[i]=0;
	 //printf("i%d   val=%d\n",i,a[i]);
      }

      print("  Following Hardware is detected on your system...  ",++xc,++yc);
      yc=yc+2;

      print("     Disk  Drive      ",xc,++yc);
      if(a[0]==1) print("[PRESENT   ]",50,yc); else print("[ABSENT   ]",50,yc);

      print("  Math  Co-processor  ",xc,++yc);
      if(a[1]==1) print("[PRESENT   ]",50,yc); else print("[ABSENT   ]",50,yc);

      print("     RAM  in  KB      ",xc,++yc);print("[          ]",50,yc);

      print(" Initial Video  Mode  ",xc,++yc);print("[          ]",50,yc);

      print(" Direct Memory Access ",xc,++yc);
      if(a[8]==0) print("[PRESENT   ]",50,yc); else print("[ABSENT    ]",50,yc);

      print("Number of Serial Ports",xc,++yc);
      print("[          ]",50,yc);
      if(a[11]==0&&a[10]==0&&a[9]==0) print("ZERO",51,yc);
      if(a[11]==0&&a[10]==0&&a[9]==1) print("ONE",51,yc);
      if(a[11]==0&&a[10]==1&&a[9]==0) print("TWO",51,yc);
      if(a[11]==0&&a[10]==1&&a[9]==1) print("THREE",51,yc);
      if(a[11]==1&&a[10]==0&&a[9]==0) print("FOUR",51,yc);
      if(a[11]==1&&a[10]==0&&a[9]==1) print("FIVE",51,yc);
      if(a[11]==1&&a[10]==1&&a[9]==0) print("SIX",51,yc);
      if(a[11]==1&&a[10]==1&&a[9]==1) print("SEVEN",51,yc);
      print("     Game  Port       ",xc,++yc);
      if(a[12]==1) print("[PRESENT   ]",50,yc); else print("[ABSENT    ]",50,yc);

      print("    Serial  Printer   ",xc,++yc);
      if(a[13]==1) print("[PRESENT   ]",50,yc); else print("[ABSENT    ]",50,yc);

      print("  number of Printers  ",xc,++yc);
      print("[          ]",50,yc);
      if(a[14]==0&&a[15]==0) print("ZERO",51,yc);
      if(a[14]==1&&a[15]==0) print("ONE",51,yc);
      if(a[14]==0&&a[15]==1) print("TWO",51,yc);
      if(a[14]==1&&a[15]==1) print("THREE",51,yc);
      yc++;
      return yc;




/*
{
int read()

{
  asm mov ah,0x02
  asm mov al,6
  asm mov ch,50
  asm mov cl,1
  asm mov dh,5
  asm mov dl,0x80
 // asm mov cf,0
  asm int 0x13
   }

  */




  }


 }