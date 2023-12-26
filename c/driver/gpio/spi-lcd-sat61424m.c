#define LOG_TAG "lcd_sanboen"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <kernel/elog.h>
#include <sys/poll.h>
#include <kernel/module.h>
#include <kernel/io.h>
#include <kernel/lib/fdt_api.h>
#include <kernel/lib/libfdt/libfdt.h>
#include <cpu_func.h>
#include <hcuapi/sysdata.h>
#include <kernel/drivers/hc_clk_gate.h>
#include <sys/ioctl.h>
#if !defined(CONFIG_DISABLE_MOUNTPOINT)
#  include <sys/mount.h>
#endif
#include <hcuapi/gpio.h>

#include <errno.h>
#include <nuttx/fs/fs.h>
#include <kernel/completion.h>
#include <kernel/notify.h>
#include <hcuapi/sys-blocking-notify.h>
#include <hcuapi/pinpad.h>
#include <kernel/delay.h>

#include "../lcd/lcd_main.h"

#define printf(...)

#define SCLK(x) gpio_set_output(PINPAD_B02,x)                   // Serial Clock Input
#define SDIN(x) gpio_set_output(PINPAD_L24,x)                   // Serial Data Input

#define RES(x)  gpio_set_output(PINPAD_L18,x)                   // Reset
#define CS(x)   gpio_set_output(PINPAD_L19,x)                   // Chip Select

#define HPW (80)
#define HBP (100)
#define HFP (12)
#define VPW (3)
#define VBP (2)
#define VFP (15)

#define FPS (60)
#define HDA (320)
#define VDA (960)

#define FLAG_DELAY            0xFE
#define FLAG_END_OF_TABLE     0xFF   // END OF REGISTERS MARKER

unsigned char sanboen_INIT_CMD[] =
{
//    0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x13,
  //  0xEF, 1, 0x08,
    0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x10,
    0xC0, 2, 0x77, 0x00,
    /*
    * porch control
      byte0: VBP
      byte1: VFP
    */
    0xC1, 2, 0x11, 0x02,
    /*
    * Inversion selection & Frame Rate Control
    * RTNI[4:0]:minimum number of pclk in each line, PCLK=512+(RTNI[4:0]x16)  640
    */
    0xC2, 2, 0x37, 0x00,
    /*
    * RGB control
    */
    0xC3, 3, 0x80, HBP, VBP,
    /*
    * PTSA[9:0]: Partial display start line address
    * PTEA[9:0]: Partial display end line address
    */
    0xC5, 4, VPW+VBP, 0x00, (VPW+VBP+VDA) & 0xFF, (VPW+VBP+VDA) >> 8,
    /*
    * D2 SS:To selection x-direction
    */
    //0xC7, 1, 04,
    0xCC, 1, 0x18,

    0xB0, 16, 0x40, 0x14, 0x59, 0x10, 0x12, 0x08, 0x03, 0x09, 0x05, 0x1E, 0x05, 0x14, 0x10, 0x68, 0x33, 0x15,

    0xB1, 16, 0x40, 0x08, 0x53, 0x09, 0x11, 0x09, 0x02, 0x07, 0x09, 0x1A, 0x04, 0x12, 0x12, 0x64, 0x29, 0x29,
    0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x11,

    0xB0, 1, 0x6D,

    0xB1, 1, 0x1D,

    0xB2, 1, 0x87,

    0xB3, 1, 0x80,
    0xB5, 1, 0x49,
    0xB7, 1, 0x85,
    0xB8, 1, 0x20,
    0xC1, 1, 0x78,
    0xC2, 1, 0x78,
    0xD0, 1, 0x88,

    0xE0, 6, 0x00, 0x00, 0x02, 0x00, 0x00, 0x0C,

    0xE1, 11,  0x02, 0x8C, 0x00, 0x00, 0x03, 0x8C, 0x00, 0x00, 0x00, 0x33, 0x33,

    0xE2, 13, 0x33, 0x33, 0x33, 0x33, 0xC9, 0x3C, 0x00, 0x00, 0xCA, 0x3C, 0x00, 0x00, 0x00,

    0xE3, 4, 0x00, 0x00, 0x33, 0x33,

    0xE4, 2, 0x44, 0x44,

    0xE5, 16, 0x05, 0xCD, 0x82, 0x82, 0x01, 0xC9, 0x82, 0x82, 0x07, 0xCF, 0x82, 0x82, 0x03, 0xCB, 0x82, 0x82,

    0xE6, 4, 0x00, 0x00, 0x33, 0x33,

    0xE7, 2, 0x44, 0x44,

    0xE8, 16, 0x06, 0xCE, 0x82, 0x82, 0x02, 0xCA, 0x82, 0x82, 0x08, 0xD0, 0x82, 0x82, 0x04, 0xCC, 0x82, 0x82,

    0xEB, 7, 0x00, 0x01, 0xE4, 0xE4, 0x88, 0x00, 0x40,

    0xED, 16, 0xFF, 0xF0, 0x07, 0x65, 0x4F, 0xFC, 0xC2, 0x2F, 0xF2, 0x2C, 0xCF, 0xF4, 0x56, 0x70, 0x0F, 0xFF,

    0xEF, 6, 0x08, 0x08, 0x08, 0x45, 0x3F, 0x54,

    0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x00,

    // bist
#if 0
    0xff,5,0x77,0x01,0x00,0x00,0x12,
    0xD1,14,0x81,0x15,0x03,0xC0,0x0C,0x01,0x30,0x01,0xE0,0x80,0x01,0xE0,0x03,0xC0,
    0xD2, 1, 0x08,
#endif

    /*
    * Display data access control
    * D3: BGR
    */
    //0x36, 1, 0x10,
    0x3A, 1, 0x70, //RGB888

    0x11, 0, 0x00,
    FLAG_DELAY, FLAG_DELAY, 120,

    0x29,0, 0x00,
    FLAG_DELAY, FLAG_DELAY, 20,
    FLAG_END_OF_TABLE, FLAG_END_OF_TABLE, FLAG_END_OF_TABLE
};

static void uDelay(unsigned char l)
{
  usleep(l);
}


static void mDelay(int n)
{
  msleep(n);
}


static void WriteComm(unsigned char Data)
{
  unsigned char i;

  CS(0);

  // FIRST BIT
  SCLK(0);
  SDIN(0);
  SCLK(1);

  for (i=0; i<8; i++)
  {
    SCLK(0);
    SDIN((Data&0x80)>>7);
    Data = Data << 1;
    //uDelay(1);
    SCLK(1);
    //uDelay(1);
  }
  CS(1);
}


static void WriteData(unsigned char Data)
{
  unsigned char i;

  CS(0);


  // FIRST BIT
  SCLK(0);
  SDIN(1);
  SCLK(1);

  for (i=0; i<8; i++)
  {
    SCLK(0);
    SDIN((Data&0x80)>>7);
    Data = Data << 1;
    //uDelay(1);
    SCLK(1);
    //uDelay(1);
  }
  CS(1);
}

static int lcd_init(void)
{
  unsigned char *cmd = sanboen_INIT_CMD;
  printf("--------------- init begin  ---------------\n");

  gpio_configure(23, 0x01);
  gpio_set_output(23, 1);

  gpio_configure(PINPAD_B03, 0x01);
  gpio_set_output(PINPAD_B03, 0);

  gpio_configure(PINPAD_B02, 0x01);
  gpio_set_output(PINPAD_B02, 1);

  gpio_configure(PINPAD_L24, 0x01);
  gpio_set_output(PINPAD_L24, 1);

  gpio_configure(PINPAD_L18, 0x01);
  gpio_set_output(PINPAD_L18, 1);

  gpio_configure(PINPAD_L19, 0x01);
  gpio_set_output(PINPAD_L19, 1);


  RES(1);

  mDelay(1);

  RES(0);

  mDelay(10);

  RES(1);

  mDelay(10);

    while (FLAG_END_OF_TABLE != (*(cmd+1)) ) {
      if (FLAG_DELAY == (*(cmd+1)) ) {
          cmd += 2;
          usleep((*cmd) * 1000);
          cmd++;
      } else {
          int len = *(cmd+1);

          //printf("cmd: 0x%02x\n", *cmd);
          WriteComm(*cmd);
          if (0 == len) {
              cmd += 3;
          } else {
              cmd += 2;
              for (int i=0; i<len; i++) {
                  WriteData(*cmd);
                  cmd ++;
              }
          }
      }
    }

  printf("--------------- init End  ---------------\n");

}

static struct lcd_map_list lcdsbn_map = {
    .map = {
        .lcd_init = lcd_init,
        .name = "lcd-sanboen",
    }
};

static int sanboen_probe(void)
{
	int default_off = 0;
    int np = fdt_node_probe_by_path("/hcrtos/lcd_sanboen");

    if(np < 0){
        log_e("no node lcd_sanboen\n");
        return 0;
    }

	fdt_get_property_u_32_index(np, "default-off", 	0, &default_off);
	if(default_off ==0)
		lcd_init();

	lcdsbn_map.map.default_off_val = default_off;

	lcd_map_register(&lcdsbn_map);
}

module_driver(hc_lcdsbn_driver,sanboen_probe,NULL,1)

