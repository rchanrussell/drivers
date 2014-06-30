
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
  FILE* fp = NULL;
  int fd = 0;
  int i = 0;
  int bytes = 0;
  int len = argc-1;
  unsigned char ctrl = 0;
  unsigned char buf[131];/* max of device read/write page 128B + config + two addr bytes */
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
  /* detect if using reading from device */
  ctrl = buf[0];

  /* write actual data */
  fp = fopen("/dev/i2c","w");
  bytes = fwrite(&buf,len,1,fp);
  fflush(fp);
  printf("Wrote %d bytes .. write\n",len);
  fclose(fp);

  /* write again but stop after address - resets SEEPROM pointer to
   * start of last write
   */
  buf[0] = 0x03; /* ctrl = start,stop*/
  buf[1] = 0xA0; /* slvAddr for write */
  buf[2] = 0x00; /* device mem high addr */
  buf[3] = 0x00; /* device mem low addr */
  fp = fopen("/dev/i2c","w");
  bytes = fwrite(&buf,4,1,fp);
  fflush(fp);
  /* write twice to ensure enter the correct addr in the EEPROM */
  bytes = fwrite(&buf,4,1,fp);
  printf("Wrote %d bytes .. write\n",4);
  fclose(fp);

  /* configure for read */
  buf[0] = 0xA1; /* slvAddr, read */
  /* change 4 to 3 if using single byte addressable I2C SEEPROM */
  buf[1] = len-4; 

  /* this will overwrite buf with the data, starting at pos 0 */
  fd = open("/dev/i2c",O_RDONLY);
  bytes = read(fd,&buf,4);
  close(fd);
  printf("Read %d bytes \n", bytes);

  printf("Read Data: \n");
    for(i=0; i<bytes; i++) {
      printf("0x%x ",buf[i]);
    }
  printf("\n");

  return 0; 
}
