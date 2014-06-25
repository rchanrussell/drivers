
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  FILE* fp = NULL;
  int i = 0;
  int len = argc-1;
  unsigned char buf[16];/* max of display */
/*
  unsigned char buf[] = {0x55,0x44,0x33,0x22,0x11};
*/
  if (argc > 17) {
    printf("Too many arguments! 16 inputs max\n");
    return 0;
  }
  for(i=1; i< argc;i++){
    buf[i-1] = strtol(argv[i],NULL,16);
  }

  fp = fopen("/dev/i2c","w");
  printf("Attempt %d bytes .. write\n",fwrite(&buf,len,1,fp));
  fclose(fp);
  return 0; 
}
