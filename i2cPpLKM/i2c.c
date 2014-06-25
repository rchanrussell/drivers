/*
 * Use parallel port, 3 pins, as I2C interface
 * One pin is SCL, one is SDA out, other SDA in
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk */
#include <linux/slab.h> /* kmalloc */
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <linux/ioport.h>
#include <linux/delay.h>

#include <asm/system.h> /* cli, flags */
#include <asm/uaccess.h>
#include <asm/io.h> /* inb, outb */

#define DRIVER_AUTHOR "Robert Chan Russell"
#define DRIVER_DESC "Utilize parallel port as I2C interface"

#define ADDR_PARABASE 0x378  /* Data Byte - out */
#define ADDR_I2CIN (ADDR_PARABASE + 1) /* Status Byte - in */
#define ADDR_I2CPWR (ADDR_PARABASE + 2)
#define I2C_SCL  0x1  /* pin2 */
#define I2C_SDAO 0x2  /* pin3 */
#define I2C_SDAI 0x20 /* pin12 */

/* license info */
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

/* func defs */
static int init_i2c(void);
static void cleanup_i2c(void);
static int open_i2c(struct inode *, struct file *);
static int close_i2c(struct inode *, struct file *);
static ssize_t read_i2c(struct file *, char *, size_t, loff_t *);
static ssize_t write_i2c(struct file *, const char *, size_t, loff_t *);
static int  i2c_out(char);
static void i2c_reset(void);
static void i2c_start(void);
static void i2c_stop(void);
static char i2c_in(void);
static void i2c_setSDA(unsigned char val);
static unsigned char i2c_getSDA(void);
static void i2c_setSCL(unsigned char val);
static void i2c_pulseSCL(void);
static void i2c_sendACK(unsigned char val);
static void read_i2cOBAT(void *i2c_buf, void *i2c_rd); 

/* structs */
static int Major;
static struct file_operations fops = {
  .read = read_i2c,
  .release = close_i2c,
  .open = open_i2c,
  .write = write_i2c
};

static int i2c_in_use = 0;
static int port = 0;
static unsigned char ignoreACKCHK = 0; // for devices who do not respond

static void i2c_setSDA(unsigned char val){
  unsigned char p = inb(ADDR_PARABASE);
  /* change only bit 2, pin3 */
  if (val==0x0) {
    p |= I2C_SDAO;
  } else {
    p &= ~I2C_SDAO;
  }
  outb(p, ADDR_PARABASE);
}

static unsigned char i2c_getSDA() {
  unsigned char p = inb(ADDR_I2CIN);
  unsigned char r = 0;
  if ((p & I2C_SDAI) == I2C_SDAI) {
    r = 0x0;
  } else {
    r = 0x1;
  }
  return r;
}

static void i2c_setSCL(unsigned char val){
  unsigned char p = inb(ADDR_PARABASE);
  /* change only bit 1, pin2 */
  if (val==0x0) {
    p |= I2C_SCL;
  } else {
    p &= ~I2C_SCL; 
  }
  outb(p, ADDR_PARABASE);
}

static void i2c_pulseSCL() {
  i2c_setSCL(0x0);
  i2c_setSCL(0x1);
  i2c_setSCL(0x0);
}

static void i2c_sendACK(unsigned char val) {
  /* ACK 0, NACK 1*/
  i2c_setSDA(val);
  i2c_setSCL(0x0);
  msleep(1);
  i2c_setSCL(0x1);
  msleep(1);
  i2c_setSCL(0x0);
}

static int i2c_out(char c) {
  int n;
  char d;
  char ack_nack;
/*
printk("<1>i2c_out %x\n",c);
*/
  d = c;
  for (n=0; n<8; n++) {
    if ((d & 0x80) == 0x80) {
      i2c_setSDA(0x1);
    } else {
      i2c_setSDA(0x0);
    }
    d = (d << 1); /* bitshift to MSB */
    i2c_pulseSCL();
  }
  /* give device time to respond */
  msleep(10);
  /* set SCL H
   * this should clock in ACK_NACK bit from device
   */
  i2c_setSCL(0x1);
  /* device sends 1 for NACK, 0 for ACK
   * due to inverted HW expect 1 for ACK 0 for NACK
   */
  ack_nack = i2c_getSDA();
  /* SCL should be H */
  /* to receive ACK/NACK do SCL L then SCL H */

  if (ack_nack == 0x1) { /* received NACK */
    if (ignoreACKCHK == 0x0) {
      i2c_pulseSCL();
    }
    ack_nack = i2c_getSDA();
printk("<1>i2c: ack_nack after pulse 0x%x\n",ack_nack);
  }
  
  /* set SCL L */
  i2c_setSCL(0x0);

  /* device sent NACK */
  if (ack_nack == 0x01) {
    return -1;
  }
  return 0;
}

static char i2c_in() {
  int n;
  char read = 0;
//  msleep(10);
  for (n=0; n<8; n++) {
    /* read bit12 only */
    read = (read << 1); /* prepare for next bit */
    if (i2c_getSDA() == 0x1) {
      read |= 0x1;
    } else {
      read &= 0xFE;
    }
    msleep(1);
    i2c_pulseSCL();
  }

printk("<1>i2c_in read %x\n",read);
  /* calling function should process
   * returned value then decide to send
   * ACK or NACK
   */
  return read;
}

static void i2c_reset(void) {
  int pulses;
printk("<1>i2c_reset\n");
  /* 9 clock pulses */
  for (pulses = 0; pulses < 9; pulses++) {
    i2c_pulseSCL();
  }
}

static void i2c_start(void) {
  i2c_setSCL(0x1);
  i2c_setSDA(0x1);
  i2c_setSDA(0x0);
  i2c_setSCL(0x0);
}

static void i2c_stop(void) {
  i2c_setSDA(0x0);
  i2c_setSCL(0x0);
  i2c_setSCL(0x1);
  i2c_setSDA(0x1);
}

static void initLCD (void) {
  /* from LCD datasheet
   * Newhaven Display International
   * NHD-C0216CiZ-FSW-FBW-3V3
   */
  i2c_start();
  i2c_out(0x7c); /* device address */
  i2c_out(0x00);
  i2c_out(0x38);
  msleep(10);
  i2c_out(0x39);
  msleep(10);
  i2c_out(0x14);
  i2c_out(0x78);
  i2c_out(0x5E);
  i2c_out(0x6D);
  i2c_out(0x0C);
  i2c_out(0x01);
  i2c_out(0x06);
  msleep(10);
  i2c_stop();
}

static int open_i2c(struct inode *inode, struct file *file) {
  if (i2c_in_use) {
    return -EBUSY;
  }
  i2c_in_use++;
  return 0;
}

static int close_i2c(struct inode *inode, struct file *file) {
  if (i2c_in_use) {
    i2c_in_use--;
  }
  return 0;
}

static ssize_t read_i2c(struct file *file, char *buf, size_t len, loff_t *ofs) {
  printk("<1>read_i2c -reads performed using write command\n");
  printk("<1>read_i2c -set bit3 of control byte, then addr then bytes to read\n"); 
  return len;
}

static void read_i2cOBAT(void *i2c_buf, void *i2c_rd) {
  unsigned char readNum=0;
  unsigned char byteRead=0;
  unsigned char slvAddr = 0;
  char *pB = i2c_buf;
  char *pR = i2c_rd;
  int l=0;

  /* cfg for read */
  pB++; /* i2c_buf[1] */
  slvAddr = *pB;
  pB++; /* i2c_buf[2] */
  /* for 1MBit device, page size 128 bytes */
  if(*pB > 128) {
    readNum = 128;
  } else {
    readNum = *pB;
  }
printk("<1>i2c_write: reading %d bytes\n",readNum);
  msleep(10);
  /* loop for num bytes */
  for (l=0; l<readNum; l++) {
    /* send cfg */
    if(i2c_out(slvAddr) < 0) {
      printk("<1>i2c: NACK received sending 0x%x\n",*pB);
    }

    /* read one B */
    byteRead = i2c_in();
    /* send NACK */
    i2c_sendACK(0x1);/* nack */
    /* send stop */
    i2c_stop();
    msleep(10);
    i2c_start();
    *pR = byteRead;
    pR++;
  } /* readNum loop */
}

static ssize_t write_i2c(struct file *file, const char *buf, size_t len, loff_t *ofs) {
  char i2c_ctrl;
  char i2c_lcd;
  unsigned char readNum=0;
/* uncomment when have timing fixed
  unsigned char byteRead=0;
*/
  int l =0;
  size_t retVal = len;
  /* buffer writing to device */
  char i2c_buf[128]; /* page size */
  char i2c_rd[128];/*diff addr required, another write/read reqd for more */

  if (len == 0) {return 0;}

  if(copy_from_user(&i2c_buf,buf,len) != 0x0) {
    return -EFAULT;
  }
  i2c_ctrl = i2c_buf[0];

  /* first byte - control byte
   * b0 - send start
   * b1 - send stop (last operation)
   * b2 - reset
   * b3 - read
   * b4 - initLCD (specific - for testing only)
   * b5 - write to LCD
   * Order Processed:
   * Reset, start, <data>, stop
   */
    if (i2c_ctrl & 0x4) {
      i2c_reset();
    }
    if (i2c_ctrl & 0x1) {
      i2c_start();
    }
    if (i2c_ctrl & 0x10) {
      ignoreACKCHK = 0x1;
      /* don't send start/stop, this handled here and initLCD */
      initLCD();
    }
    /* prep LCD for data bytes - needs to be sent every time
     * with start/stop
     */
    if (i2c_ctrl & 0x20) {
      ignoreACKCHK = 0x1;
      i2c_lcd = 0x7c;
      i2c_out(i2c_lcd);
      i2c_lcd = 0x80;
      i2c_out(i2c_lcd);
      i2c_out(i2c_lcd);
      i2c_lcd = 0x40;
      i2c_out(i2c_lcd);
    }

    if (i2c_ctrl & 0x08){
      if (len != 3) {
        printk("<1>i2c_write error - 3 bytes, ctrl,slaveAddr,numBytes\n");
      } else {
/* removed due to timing issue i cannot resolve
 * cannot seem to send more than one byte, regardless
 * of ACK timing - works on Arduino with some timing tweaks
 * but cannot replicate here
 * send one byte at a time instead
        // cfg for read 
        if(i2c_out(i2c_buf[1]) < 0) {
            printk("<1>i2c: NACK received sending 0x%x\n",i2c_buf[1]);
        }
        // for 1MBit device, page size 128 bytes 
        if(i2c_buf[2] > 128) {
          readNum = 128;
        } else {
          readNum = i2c_buf[2];
        }
  printk("<1>i2c_write: reading %d bytes\n",readNum);
        msleep(10);
        for (l=0; l<readNum; l++) {
  	  if (l == (readNum-1)) {
            byteRead = i2c_in();
  	  i2c_sendACK(0x1);
          } else {
            byteRead = i2c_in();
  	    msleep(1);
  	    i2c_sendACK(0x0);
  	    msleep(10);
          }
          i2c_rd[l] = byteRead;
        }
*/
        // for 1MBit device, page size 128 bytes 
        if(i2c_buf[2] > 128) {
          readNum = 128;
        } else {
          readNum = i2c_buf[2];
        }
        retVal=readNum;
        read_i2cOBAT(&i2c_buf, &i2c_rd);

      } /* ctrl + slaveAddr + datasize */
    } else {
    /* send passed data, bypassing ctrl byte */
printk("<1>i2c_write: writng %d bytes\n",len-1);
      for (l=1; l<len; l++) {
        if(i2c_out(i2c_buf[l]) < 0) {
          printk("<1>i2c: NACK received sending 0x%x\n",i2c_buf[l]);
        }
      }
    }
    if (i2c_ctrl & 0x2) {
    i2c_stop();
    }
  ignoreACKCHK = 0x0; // disable ignore of device check
  return retVal;
}

static int init_i2c(void) {
  //int result;
  printk(KERN_ALERT "i2c init\n");

  Major = register_chrdev(0, "i2c", &fops);//character device - virtual
  if (Major < 0) {
    printk(KERN_ALERT "rand: registration of device failed: %d\n",Major);
    return Major;
  }

  /* Register port */
/* bypass since port is in use 
  port = check_region(ADDR_PARABASE, 1);
  if (port) {
    printk("<1>i2c: cannot reserve 0x%x\n", ADDR_PARABASE);
    result = port;
    cleanup_i2c();
    return result;
  }
  request_region(ADDR_PARABASE, 1, "i2c");
*/
  printk("<1>i2c: assigned major number %d\n", Major);

  return 0;
}

static void cleanup_i2c(void) {
  /* unregister device */
  unregister_chrdev(Major, "i2c");
  if (!port) {
    release_region(ADDR_PARABASE, 1);
  }
  printk("<1>i2c: removing i2c module\n");
}

module_init(init_i2c);
module_exit(cleanup_i2c);
