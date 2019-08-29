#include<bios.h>
#include<stdio.h>

/*different drives*/

#define _floppy1 0x0
#define _floppy2 0x1
#define _hd1 0x80
#define _hd2 0x81
#define _hd3 0x83

/*writes the bootsector*/

void writebootsector(char *buffer,int drive=_floppy1)
{
  printf("\nwritting.........");
  int result;
  if((result=biosdisk(3,0x0,0,0,1,1,buffer))==0)
  {
    printf("1 sector copied successfully");
  }
  else
  {
    printf("error %d",result);
  }

}
  main()
  {
  FILE *fn;
  if((fn==fopen("boot.bin","rb"))!=NULL)
  {
      char buffer[512];
      fread(&buffer,512,1,fn);
      printf("\n contents of charbuff\n");
      for(int i=0;i<=512;i++)
      printf("%c",buffer[i]);
      writebootsector(buffer,_floppy1);
      fclose(fn);
      return 1;
  }
  else
  {
     printf("unable to open\n");
  return 0;
  }
}
