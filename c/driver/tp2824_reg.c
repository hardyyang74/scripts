#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <asm/types.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include "tp2824b.h"

extern struct i2c_client *i2cClient44;

// tx_type : 0: AHD; 1: TVI; 2: CVI; else: not support
#define TX_AHD 0
#define TX_TVI 1
#define TX_CVI 2

#define RTY_CNT 8

unsigned char videoinfmt = 0xaa;
unsigned char videofmt[4]={0xff, 0xff, 0xff, 0xff};

unsigned char dev_id[2]={0xff, 0xff};


void tp2824b_channel_ctl(unsigned char ch) // ch: 0~4, 0/1/2/3 for each ch; 4 for all ch
{
	i2c_smbus_write_byte_data(i2cClient44, 0x40, ch);
	i2c_smbus_write_byte_data(i2cClient44, 0x06, 0x80); //reset
}


void tp2824b_1080p_25_init(int tx_type)
{
	if (tx_type == TX_AHD){
	}
	else if (tx_type == TX_TVI){
	}
	else if (tx_type == TX_CVI){
	}
}


void tp2824b_1080p_30_init(int tx_type)
{
	if (tx_type == TX_AHD){
	}
	else if (tx_type == TX_TVI){
	}
	else if (tx_type == TX_CVI){
	}
}


void tp2824b_720p_25_init(int tx_type)
{
    printk("dev_id:0x%x tx_type:%d\n", dev_id[1], tx_type);
    if ((dev_id[1] == 0x27) || (dev_id[1] == 0x29) ){ //ver. c
        if (tx_type == TX_AHD){
            int i,val;
            //tp28xx_byte_write(chip, 0x45, 0xd4); //DDR mode
            if (dev_id[1] == 0x27) {
                i2c_smbus_write_byte_data(i2cClient44, 0x45, 0xc9); //SDR mode
            } else { // tp2829
                i2c_smbus_write_byte_data(i2cClient44, 0x45, 0x09); //SDR mode
            }

            for(i = 0; i < 6; i++ )
            {
                i2c_smbus_write_byte_data(i2cClient44, 0x44, 0x47);
                i2c_smbus_write_byte_data(i2cClient44, 0x42, 0x0C);
                i2c_smbus_write_byte_data(i2cClient44, 0x44, 0x07);
                i2c_smbus_write_byte_data(i2cClient44, 0x42, 0x00);
                msleep(1);
                val = i2c_smbus_read_byte_data(i2cClient44, 0x01);
                if(0x08 != val) break;
            }

            i2c_smbus_write_byte_data(i2cClient44, 0x02, 0xCE);
            i2c_smbus_write_byte_data(i2cClient44, 0x03, 0x0d);
            i2c_smbus_write_byte_data(i2cClient44, 0x04, 0x1b);
            i2c_smbus_write_byte_data(i2cClient44, 0x05, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x06, 0x32);
            i2c_smbus_write_byte_data(i2cClient44, 0x07, 0xC0);
            i2c_smbus_write_byte_data(i2cClient44, 0x08, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x09, 0x24);
            i2c_smbus_write_byte_data(i2cClient44, 0x0a, 0x48);
            i2c_smbus_write_byte_data(i2cClient44, 0x0B, 0xC0);
            i2c_smbus_write_byte_data(i2cClient44, 0x0C, 0x03);
            i2c_smbus_write_byte_data(i2cClient44, 0x0D, 0x71);
            i2c_smbus_write_byte_data(i2cClient44, 0x0e, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x0f, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x15, 0x13);
            i2c_smbus_write_byte_data(i2cClient44, 0x16, 0x16);
            i2c_smbus_write_byte_data(i2cClient44, 0x17, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x18, 0x19);
            i2c_smbus_write_byte_data(i2cClient44, 0x19, 0xD0);
            i2c_smbus_write_byte_data(i2cClient44, 0x1A, 0x25);
            i2c_smbus_write_byte_data(i2cClient44, 0x1b, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x1C, 0x07);
            i2c_smbus_write_byte_data(i2cClient44, 0x1D, 0xBC);
            i2c_smbus_write_byte_data(i2cClient44, 0x1e, 0x60);
            i2c_smbus_write_byte_data(i2cClient44, 0x1f, 0x06);
            i2c_smbus_write_byte_data(i2cClient44, 0x20, 0x40);
            i2c_smbus_write_byte_data(i2cClient44, 0x21, 0x46);
            i2c_smbus_write_byte_data(i2cClient44, 0x22, 0x36);
            i2c_smbus_write_byte_data(i2cClient44, 0x23, 0x3c);
            i2c_smbus_write_byte_data(i2cClient44, 0x24, 0x04);
            i2c_smbus_write_byte_data(i2cClient44, 0x25, 0xFe);
            i2c_smbus_write_byte_data(i2cClient44, 0x26, 0x01);
            i2c_smbus_write_byte_data(i2cClient44, 0x27, 0x2d);
            i2c_smbus_write_byte_data(i2cClient44, 0x28, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x29, 0x48);
            i2c_smbus_write_byte_data(i2cClient44, 0x2a, 0x30);
            i2c_smbus_write_byte_data(i2cClient44, 0x2B, 0x60);
            i2c_smbus_write_byte_data(i2cClient44, 0x2C, 0x3A);
            i2c_smbus_write_byte_data(i2cClient44, 0x2D, 0x5a);
            i2c_smbus_write_byte_data(i2cClient44, 0x2E, 0x40);
            i2c_smbus_write_byte_data(i2cClient44, 0x2f, 0x06);
            i2c_smbus_write_byte_data(i2cClient44, 0x30, 0x9e);
            i2c_smbus_write_byte_data(i2cClient44, 0x31, 0x20);
            i2c_smbus_write_byte_data(i2cClient44, 0x32, 0x10);
            i2c_smbus_write_byte_data(i2cClient44, 0x33, 0x90);
            i2c_smbus_write_byte_data(i2cClient44, 0x35, 0x25);
            i2c_smbus_write_byte_data(i2cClient44, 0x36, 0xca);
            i2c_smbus_write_byte_data(i2cClient44, 0x37, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x38, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x39, 0x18);
            i2c_smbus_write_byte_data(i2cClient44, 0x3A, 0x32);
            i2c_smbus_write_byte_data(i2cClient44, 0x3b, 0x26);
            i2c_smbus_write_byte_data(i2cClient44, 0x3c, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x3d, 0x60);
            i2c_smbus_write_byte_data(i2cClient44, 0x3e, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x3f, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x4D, 0x0F);
            i2c_smbus_write_byte_data(i2cClient44, 0x4E, 0x0F);
            i2c_smbus_write_byte_data(i2cClient44, 0x50, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x51, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x52, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x53, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0xF1, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0xF2, 0x77);
            i2c_smbus_write_byte_data(i2cClient44, 0xF3, 0x77);
            if (dev_id[1] == 0x29) {
                i2c_smbus_write_byte_data(i2cClient44, 0xF4, 0x20);
            }
            i2c_smbus_write_byte_data(i2cClient44, 0xF5, 0x0F);
            i2c_smbus_write_byte_data(i2cClient44, 0xF6, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0xF7, 0x11);
            i2c_smbus_write_byte_data(i2cClient44, 0xF8, 0x22);
            i2c_smbus_write_byte_data(i2cClient44, 0xF9, 0x33);
            i2c_smbus_write_byte_data(i2cClient44, 0xFA, 0x99);
            i2c_smbus_write_byte_data(i2cClient44, 0xFB, 0x99);
        }	else if (tx_type == TX_TVI){
            //			i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x04);
            i2c_smbus_write_byte_data(i2cClient44, 0x02, 0xCA);
            i2c_smbus_write_byte_data(i2cClient44, 0x07, 0xC0);
            i2c_smbus_write_byte_data(i2cClient44, 0x0B, 0xC0);
            i2c_smbus_write_byte_data(i2cClient44, 0x0C, 0x63);
            i2c_smbus_write_byte_data(i2cClient44, 0x0D, 0x10);
            i2c_smbus_write_byte_data(i2cClient44, 0x16, 0x15);
            i2c_smbus_write_byte_data(i2cClient44, 0x17, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x18, 0x18);
            i2c_smbus_write_byte_data(i2cClient44, 0x19, 0xD0);
            i2c_smbus_write_byte_data(i2cClient44, 0x1A, 0x25);
            i2c_smbus_write_byte_data(i2cClient44, 0x1C, 0x07);
            i2c_smbus_write_byte_data(i2cClient44, 0x1D, 0xC2);
            i2c_smbus_write_byte_data(i2cClient44, 0x20, 0x60);
            i2c_smbus_write_byte_data(i2cClient44, 0x21, 0x86);
            i2c_smbus_write_byte_data(i2cClient44, 0x25, 0xFF);
            i2c_smbus_write_byte_data(i2cClient44, 0x26, 0x02);
            i2c_smbus_write_byte_data(i2cClient44, 0x2B, 0x60);
            i2c_smbus_write_byte_data(i2cClient44, 0x2C, 0x0A);
            i2c_smbus_write_byte_data(i2cClient44, 0x2D, 0x30);
            i2c_smbus_write_byte_data(i2cClient44, 0x2E, 0x70);
            i2c_smbus_write_byte_data(i2cClient44, 0x30, 0x48);
            i2c_smbus_write_byte_data(i2cClient44, 0x31, 0xBA);
            i2c_smbus_write_byte_data(i2cClient44, 0x32, 0x2E);
            i2c_smbus_write_byte_data(i2cClient44, 0x33, 0x90);
            i2c_smbus_write_byte_data(i2cClient44, 0x35, 0x25);
            i2c_smbus_write_byte_data(i2cClient44, 0x39, 0x88);
            i2c_smbus_write_byte_data(i2cClient44, 0x3A, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x4D, 0x0F);
            i2c_smbus_write_byte_data(i2cClient44, 0x4E, 0x0F);
            i2c_smbus_write_byte_data(i2cClient44, 0xF5, 0x0F);
            i2c_smbus_write_byte_data(i2cClient44, 0xFA, 0x99);
            i2c_smbus_write_byte_data(i2cClient44, 0xFB, 0x99);
        }        else if (tx_type == TX_CVI){
        }
    } else if (dev_id[1] == 0x31 ) {
        if (tx_type == TX_AHD){
            i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x04);
            i2c_smbus_write_byte_data(i2cClient44, 0x3b, 0x20);
            i2c_smbus_write_byte_data(i2cClient44, 0x3d, 0xe0);
            i2c_smbus_write_byte_data(i2cClient44, 0x3d, 0x60);
            i2c_smbus_write_byte_data(i2cClient44, 0x3b, 0x25);
            i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x40);
            i2c_smbus_write_byte_data(i2cClient44, 0x7a, 0x20);
            i2c_smbus_write_byte_data(i2cClient44, 0x3c, 0x20);
            i2c_smbus_write_byte_data(i2cClient44, 0x3c, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x7a, 0x25);
            i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x04);

            i2c_smbus_write_byte_data(i2cClient44, 0x42, 0x80);
            i2c_smbus_write_byte_data(i2cClient44, 0x44, 0x17);
            i2c_smbus_write_byte_data(i2cClient44, 0x43, 0x12);
            i2c_smbus_write_byte_data(i2cClient44, 0x45, 0x09);

            i2c_smbus_write_byte_data(i2cClient44, 0x02, 0xCE);
            i2c_smbus_write_byte_data(i2cClient44, 0x07, 0xC0);
            i2c_smbus_write_byte_data(i2cClient44, 0x0B, 0xC0);
            i2c_smbus_write_byte_data(i2cClient44, 0x0C, 0x03);
            i2c_smbus_write_byte_data(i2cClient44, 0x0D, 0x71);
            i2c_smbus_write_byte_data(i2cClient44, 0x15, 0x13);
            i2c_smbus_write_byte_data(i2cClient44, 0x16, 0x16);
            i2c_smbus_write_byte_data(i2cClient44, 0x17, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x18, 0x19);
            i2c_smbus_write_byte_data(i2cClient44, 0x19, 0xD0);
            i2c_smbus_write_byte_data(i2cClient44, 0x1A, 0x25);
            i2c_smbus_write_byte_data(i2cClient44, 0x1b, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x1C, 0x07);
            i2c_smbus_write_byte_data(i2cClient44, 0x1D, 0xBC);

            i2c_smbus_write_byte_data(i2cClient44, 0x20, 0x40);
            i2c_smbus_write_byte_data(i2cClient44, 0x21, 0x46);
            i2c_smbus_write_byte_data(i2cClient44, 0x22, 0x36);
            i2c_smbus_write_byte_data(i2cClient44, 0x23, 0x3c);
            i2c_smbus_write_byte_data(i2cClient44, 0x24, 0x04);
            i2c_smbus_write_byte_data(i2cClient44, 0x25, 0xFe);
            i2c_smbus_write_byte_data(i2cClient44, 0x26, 0x01);
            i2c_smbus_write_byte_data(i2cClient44, 0x27, 0x2d);
            i2c_smbus_write_byte_data(i2cClient44, 0x28, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x29, 0x48);
            i2c_smbus_write_byte_data(i2cClient44, 0x2a, 0x30);
            i2c_smbus_write_byte_data(i2cClient44, 0x2B, 0x60);
            i2c_smbus_write_byte_data(i2cClient44, 0x2C, 0x3A);
            i2c_smbus_write_byte_data(i2cClient44, 0x2D, 0x5a);
            i2c_smbus_write_byte_data(i2cClient44, 0x2E, 0x40);

            i2c_smbus_write_byte_data(i2cClient44, 0x30, 0x9e);
            i2c_smbus_write_byte_data(i2cClient44, 0x31, 0x20);
            i2c_smbus_write_byte_data(i2cClient44, 0x32, 0x10);
            i2c_smbus_write_byte_data(i2cClient44, 0x33, 0x90);
            i2c_smbus_write_byte_data(i2cClient44, 0x35, 0x25);

            i2c_smbus_write_byte_data(i2cClient44, 0x38, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x39, 0x18);
            i2c_smbus_write_byte_data(i2cClient44, 0x3A, 0x32);
            i2c_smbus_write_byte_data(i2cClient44, 0x3b, 0x25);

            i2c_smbus_write_byte_data(i2cClient44, 0x4D, 0x0F);
            i2c_smbus_write_byte_data(i2cClient44, 0x4E, 0x0F);
            i2c_smbus_write_byte_data(i2cClient44, 0x50, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x51, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x52, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x53, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0xF1, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0xF2, 0x77);
            i2c_smbus_write_byte_data(i2cClient44, 0xF3, 0x77);

            i2c_smbus_write_byte_data(i2cClient44, 0xF5, 0xfF);
            i2c_smbus_write_byte_data(i2cClient44, 0xF6, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0xF7, 0x11);
            i2c_smbus_write_byte_data(i2cClient44, 0xF8, 0x22);
            i2c_smbus_write_byte_data(i2cClient44, 0xF9, 0x33);
            i2c_smbus_write_byte_data(i2cClient44, 0xFA, 0x99);
            i2c_smbus_write_byte_data(i2cClient44, 0xFB, 0x99);
        }
    } else { // ver 2824B
        if (tx_type == TX_AHD){
            // i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x04);
            i2c_smbus_write_byte_data(i2cClient44, 0x02, 0xCA);
            i2c_smbus_write_byte_data(i2cClient44, 0x07, 0x80);
            i2c_smbus_write_byte_data(i2cClient44, 0x0B, 0xC0);
            i2c_smbus_write_byte_data(i2cClient44, 0x0C, 0x03);
            i2c_smbus_write_byte_data(i2cClient44, 0x0D, 0x10);
            i2c_smbus_write_byte_data(i2cClient44, 0x16, 0x16);
            i2c_smbus_write_byte_data(i2cClient44, 0x17, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x18, 0x19);
            i2c_smbus_write_byte_data(i2cClient44, 0x19, 0xD0);
            i2c_smbus_write_byte_data(i2cClient44, 0x1A, 0x25);
            i2c_smbus_write_byte_data(i2cClient44, 0x1C, 0x07);
            i2c_smbus_write_byte_data(i2cClient44, 0x1D, 0xBC);
            i2c_smbus_write_byte_data(i2cClient44, 0x20, 0x60);
            i2c_smbus_write_byte_data(i2cClient44, 0x21, 0x84);
            i2c_smbus_write_byte_data(i2cClient44, 0x25, 0xF0);
            i2c_smbus_write_byte_data(i2cClient44, 0x26, 0x02);
            i2c_smbus_write_byte_data(i2cClient44, 0x2B, 0x58);
            i2c_smbus_write_byte_data(i2cClient44, 0x2C, 0x0A);
            i2c_smbus_write_byte_data(i2cClient44, 0x2D, 0x48);
            i2c_smbus_write_byte_data(i2cClient44, 0x2E, 0x5E);
            i2c_smbus_write_byte_data(i2cClient44, 0x30, 0x27);
            i2c_smbus_write_byte_data(i2cClient44, 0x31, 0x88);
            i2c_smbus_write_byte_data(i2cClient44, 0x32, 0x04);
            i2c_smbus_write_byte_data(i2cClient44, 0x33, 0x23);
            i2c_smbus_write_byte_data(i2cClient44, 0x35, 0x25);
            i2c_smbus_write_byte_data(i2cClient44, 0x39, 0x88);
            i2c_smbus_write_byte_data(i2cClient44, 0x3A, 0x70);
            i2c_smbus_write_byte_data(i2cClient44, 0x4D, 0x0F);
            i2c_smbus_write_byte_data(i2cClient44, 0x4E, 0x0F);
            i2c_smbus_write_byte_data(i2cClient44, 0xF5, 0x0F);
            i2c_smbus_write_byte_data(i2cClient44, 0xFA, 0xAA);
            i2c_smbus_write_byte_data(i2cClient44, 0xFB, 0xAA);
        }    else if (tx_type == TX_TVI){
            //			i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x04);
            i2c_smbus_write_byte_data(i2cClient44, 0x02, 0xCA);
            i2c_smbus_write_byte_data(i2cClient44, 0x07, 0xC0);
            i2c_smbus_write_byte_data(i2cClient44, 0x0B, 0xC0);
            i2c_smbus_write_byte_data(i2cClient44, 0x0C, 0x63);
            i2c_smbus_write_byte_data(i2cClient44, 0x0D, 0x10);
            i2c_smbus_write_byte_data(i2cClient44, 0x16, 0x15);
            i2c_smbus_write_byte_data(i2cClient44, 0x17, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x18, 0x18);
            i2c_smbus_write_byte_data(i2cClient44, 0x19, 0xD0);
            i2c_smbus_write_byte_data(i2cClient44, 0x1A, 0x25);
            i2c_smbus_write_byte_data(i2cClient44, 0x1C, 0x07);
            i2c_smbus_write_byte_data(i2cClient44, 0x1D, 0xC2);
            i2c_smbus_write_byte_data(i2cClient44, 0x20, 0x60);
            i2c_smbus_write_byte_data(i2cClient44, 0x21, 0x86);
            i2c_smbus_write_byte_data(i2cClient44, 0x25, 0xFF);
            i2c_smbus_write_byte_data(i2cClient44, 0x26, 0x02);
            i2c_smbus_write_byte_data(i2cClient44, 0x2B, 0x70);
            i2c_smbus_write_byte_data(i2cClient44, 0x2C, 0x0A);
            i2c_smbus_write_byte_data(i2cClient44, 0x2D, 0x30);
            i2c_smbus_write_byte_data(i2cClient44, 0x2E, 0x70);
            i2c_smbus_write_byte_data(i2cClient44, 0x30, 0x48);
            i2c_smbus_write_byte_data(i2cClient44, 0x31, 0xBA);
            i2c_smbus_write_byte_data(i2cClient44, 0x32, 0x2E);
            i2c_smbus_write_byte_data(i2cClient44, 0x33, 0x90);
            i2c_smbus_write_byte_data(i2cClient44, 0x35, 0x25);
            i2c_smbus_write_byte_data(i2cClient44, 0x39, 0x88);
            i2c_smbus_write_byte_data(i2cClient44, 0x3A, 0x00);
            i2c_smbus_write_byte_data(i2cClient44, 0x4D, 0x0F);
            i2c_smbus_write_byte_data(i2cClient44, 0x4E, 0x0F);
            i2c_smbus_write_byte_data(i2cClient44, 0xF5, 0x0F);
            i2c_smbus_write_byte_data(i2cClient44, 0xFA, 0x99);
            i2c_smbus_write_byte_data(i2cClient44, 0xFB, 0x99);
        }    else if (tx_type == TX_CVI){
        }
    }
}

void tp2824b_720p_50_init(int tx_type)
{
	if (tx_type == TX_AHD){
	}
	else if (tx_type == TX_TVI){
//		i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x04);
		i2c_smbus_write_byte_data(i2cClient44, 0x02, 0xca);
		i2c_smbus_write_byte_data(i2cClient44, 0x07, 0xc0);
		i2c_smbus_write_byte_data(i2cClient44, 0x0b, 0xc0);
		i2c_smbus_write_byte_data(i2cClient44, 0x0c, 0x63);
		i2c_smbus_write_byte_data(i2cClient44, 0x15, 0x13);
		i2c_smbus_write_byte_data(i2cClient44, 0x16, 0x15);
		i2c_smbus_write_byte_data(i2cClient44, 0x17, 0x00);
		i2c_smbus_write_byte_data(i2cClient44, 0x18, 0x18);
		i2c_smbus_write_byte_data(i2cClient44, 0x19, 0xd0);
		i2c_smbus_write_byte_data(i2cClient44, 0x1a, 0x25);
		i2c_smbus_write_byte_data(i2cClient44, 0x1c, 0x07);
		i2c_smbus_write_byte_data(i2cClient44, 0x1d, 0xc2);
		i2c_smbus_write_byte_data(i2cClient44, 0x24, 0x00);
		i2c_smbus_write_byte_data(i2cClient44, 0x2b, 0x70);
		i2c_smbus_write_byte_data(i2cClient44, 0x35, 0x25);
		i2c_smbus_write_byte_data(i2cClient44, 0x39, 0x8c);

		i2c_smbus_write_byte_data(i2cClient44, 0x42, 0x00);
		i2c_smbus_write_byte_data(i2cClient44, 0x43, 0x12);
		i2c_smbus_write_byte_data(i2cClient44, 0x44, 0x07);
		i2c_smbus_write_byte_data(i2cClient44, 0x45, 0x49);
		i2c_smbus_write_byte_data(i2cClient44, 0x4d, 0x0f);
		i2c_smbus_write_byte_data(i2cClient44, 0x4e, 0x0f);
		i2c_smbus_write_byte_data(i2cClient44, 0x4f, 0x0f);

		i2c_smbus_write_byte_data(i2cClient44, 0x7e, 0x0f);

		i2c_smbus_write_byte_data(i2cClient44, 0xf5, 0x00);
		i2c_smbus_write_byte_data(i2cClient44, 0xfa, 0x88);
		i2c_smbus_write_byte_data(i2cClient44, 0xfb, 0x88);
	}
	else if (tx_type == TX_CVI){
	}
}


void tp2824b_720p_30_init(int tx_type)
{
	if (dev_id[1] == 0x27){ //ver. c
		if (tx_type == TX_AHD){
//			i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x04);
			i2c_smbus_write_byte_data(i2cClient44, 0x02, 0xCE);
			i2c_smbus_write_byte_data(i2cClient44, 0x07, 0xC0);
			i2c_smbus_write_byte_data(i2cClient44, 0x0B, 0xC0);
			i2c_smbus_write_byte_data(i2cClient44, 0x0C, 0x03);
			i2c_smbus_write_byte_data(i2cClient44, 0x0D, 0x70);
			i2c_smbus_write_byte_data(i2cClient44, 0x16, 0x16);
			i2c_smbus_write_byte_data(i2cClient44, 0x1C, 0x06);
			i2c_smbus_write_byte_data(i2cClient44, 0x1D, 0x72);
			i2c_smbus_write_byte_data(i2cClient44, 0x20, 0x40);
			i2c_smbus_write_byte_data(i2cClient44, 0x21, 0x46);
			i2c_smbus_write_byte_data(i2cClient44, 0x22, 0x36);
			i2c_smbus_write_byte_data(i2cClient44, 0x25, 0xFE);
			i2c_smbus_write_byte_data(i2cClient44, 0x26, 0x01);
			i2c_smbus_write_byte_data(i2cClient44, 0x2B, 0x60);
			i2c_smbus_write_byte_data(i2cClient44, 0x2C, 0x1A);
			i2c_smbus_write_byte_data(i2cClient44, 0x2D, 0x5A);
			i2c_smbus_write_byte_data(i2cClient44, 0x2E, 0x40);
			i2c_smbus_write_byte_data(i2cClient44, 0x30, 0x9D);
			i2c_smbus_write_byte_data(i2cClient44, 0x31, 0xCA);
			i2c_smbus_write_byte_data(i2cClient44, 0x32, 0x01);
			i2c_smbus_write_byte_data(i2cClient44, 0x33, 0xD0);
			i2c_smbus_write_byte_data(i2cClient44, 0x35, 0x25);
			i2c_smbus_write_byte_data(i2cClient44, 0x38, 0x00);
			i2c_smbus_write_byte_data(i2cClient44, 0x39, 0x18);
			i2c_smbus_write_byte_data(i2cClient44, 0x3A, 0x32);
			i2c_smbus_write_byte_data(i2cClient44, 0x4D, 0x0F);
			i2c_smbus_write_byte_data(i2cClient44, 0x4E, 0x0F);
			i2c_smbus_write_byte_data(i2cClient44, 0xF5, 0x0f);
			i2c_smbus_write_byte_data(i2cClient44, 0xFA, 0xAA);
			i2c_smbus_write_byte_data(i2cClient44, 0xFB, 0xAA);
		}
		else if (tx_type == TX_TVI){
		}
		else if (tx_type == TX_CVI){
		}
	}
	else{
		if (tx_type == TX_AHD){
		}
		else if (tx_type == TX_TVI){
//			i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x04);
			i2c_smbus_write_byte_data(i2cClient44, 0x02, 0x02);
			i2c_smbus_write_byte_data(i2cClient44, 0x07, 0xc0);
			i2c_smbus_write_byte_data(i2cClient44, 0x0b, 0xc0);
			i2c_smbus_write_byte_data(i2cClient44, 0x0c, 0x63);
			i2c_smbus_write_byte_data(i2cClient44, 0x15, 0x13);
			i2c_smbus_write_byte_data(i2cClient44, 0x16, 0x15);
			i2c_smbus_write_byte_data(i2cClient44, 0x17, 0x00);
			i2c_smbus_write_byte_data(i2cClient44, 0x18, 0x19);
			i2c_smbus_write_byte_data(i2cClient44, 0x19, 0xd0);
			i2c_smbus_write_byte_data(i2cClient44, 0x1a, 0x25);
			i2c_smbus_write_byte_data(i2cClient44, 0x1c, 0x06);
			i2c_smbus_write_byte_data(i2cClient44, 0x1d, 0x72);
			i2c_smbus_write_byte_data(i2cClient44, 0x2b, 0x70);
			i2c_smbus_write_byte_data(i2cClient44, 0x35, 0x25);
			i2c_smbus_write_byte_data(i2cClient44, 0x39, 0x88);

			i2c_smbus_write_byte_data(i2cClient44, 0x42, 0x00);
			i2c_smbus_write_byte_data(i2cClient44, 0x43, 0x12);
			i2c_smbus_write_byte_data(i2cClient44, 0x44, 0x07);
			i2c_smbus_write_byte_data(i2cClient44, 0x45, 0x49);
			i2c_smbus_write_byte_data(i2cClient44, 0x4d, 0x0f);
			i2c_smbus_write_byte_data(i2cClient44, 0x4e, 0x0f);
			i2c_smbus_write_byte_data(i2cClient44, 0x4f, 0x0f);

			i2c_smbus_write_byte_data(i2cClient44, 0x7e, 0x0f);

			i2c_smbus_write_byte_data(i2cClient44, 0xf5, 0x0f);
			i2c_smbus_write_byte_data(i2cClient44, 0xfa, 0x99);
			i2c_smbus_write_byte_data(i2cClient44, 0xfb, 0x99);
		}
		else if (tx_type == TX_CVI){
		}
	}
}


void tp2824b_720p_60_init(int tx_type)
{
	if (tx_type == TX_AHD){
	}
	else if (tx_type == TX_TVI){
	}
	else if (tx_type == TX_CVI){
	}
}

void tp2824c_NTSC_960_480_init(void)
{
	printk("dev_id:0x%x\n", dev_id[1]);
	if (dev_id[1] == 0x27){ //ver. c
//	i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x04);
		i2c_smbus_write_byte_data(i2cClient44, 0x02, 0xCF);
		i2c_smbus_write_byte_data(i2cClient44, 0x07, 0xC0);
		i2c_smbus_write_byte_data(i2cClient44, 0x0B, 0xC0);
		i2c_smbus_write_byte_data(i2cClient44, 0x0C, 0x43);
		i2c_smbus_write_byte_data(i2cClient44, 0x0D, 0x10);
		i2c_smbus_write_byte_data(i2cClient44, 0x16, 0x4B);
		i2c_smbus_write_byte_data(i2cClient44, 0x17, 0xBC);
		i2c_smbus_write_byte_data(i2cClient44, 0x18, 0x12);
		i2c_smbus_write_byte_data(i2cClient44, 0x19, 0xF0);
		i2c_smbus_write_byte_data(i2cClient44, 0x1A, 0x07);
		i2c_smbus_write_byte_data(i2cClient44, 0x1C, 0x09);
		i2c_smbus_write_byte_data(i2cClient44, 0x1D, 0x38);
		i2c_smbus_write_byte_data(i2cClient44, 0x20, 0xA0);
		i2c_smbus_write_byte_data(i2cClient44, 0x21, 0x84);
		i2c_smbus_write_byte_data(i2cClient44, 0x25, 0x25);
		i2c_smbus_write_byte_data(i2cClient44, 0x26, 0x12);
		i2c_smbus_write_byte_data(i2cClient44, 0x2B, 0x50);
		i2c_smbus_write_byte_data(i2cClient44, 0x2C, 0x2A);
		i2c_smbus_write_byte_data(i2cClient44, 0x2D, 0x68);
		i2c_smbus_write_byte_data(i2cClient44, 0x2E, 0x5E);
		i2c_smbus_write_byte_data(i2cClient44, 0x30, 0x62);
		i2c_smbus_write_byte_data(i2cClient44, 0x31, 0xBB);
		i2c_smbus_write_byte_data(i2cClient44, 0x32, 0x96);
		i2c_smbus_write_byte_data(i2cClient44, 0x33, 0xC0);
		i2c_smbus_write_byte_data(i2cClient44, 0x35, 0x65);
		i2c_smbus_write_byte_data(i2cClient44, 0x39, 0x84);
		i2c_smbus_write_byte_data(i2cClient44, 0x3A, 0x32);
		i2c_smbus_write_byte_data(i2cClient44, 0x4D, 0x0F);
		i2c_smbus_write_byte_data(i2cClient44, 0x4E, 0x0F);
		i2c_smbus_write_byte_data(i2cClient44, 0xF5, 0x0F);
		i2c_smbus_write_byte_data(i2cClient44, 0xFA, 0xCB);
		i2c_smbus_write_byte_data(i2cClient44, 0xFB, 0xED);

		i2c_smbus_write_byte_data(i2cClient44, 0xF1, 0x01);
		i2c_smbus_write_byte_data(i2cClient44, 0x3B, 0x26);
	}
	else
	{
	//	i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x04);
		i2c_smbus_write_byte_data(i2cClient44, 0x02, 0xCF);
		i2c_smbus_write_byte_data(i2cClient44, 0x07, 0xC0);
		i2c_smbus_write_byte_data(i2cClient44, 0x0B, 0xC0);
		i2c_smbus_write_byte_data(i2cClient44, 0x0C, 0x43);
		i2c_smbus_write_byte_data(i2cClient44, 0x0D, 0x10);
		i2c_smbus_write_byte_data(i2cClient44, 0x16, 0x4B);
		i2c_smbus_write_byte_data(i2cClient44, 0x17, 0xBC);
		i2c_smbus_write_byte_data(i2cClient44, 0x18, 0x12);
		i2c_smbus_write_byte_data(i2cClient44, 0x19, 0xF0);
		i2c_smbus_write_byte_data(i2cClient44, 0x1A, 0x07);
		i2c_smbus_write_byte_data(i2cClient44, 0x1C, 0x09);
		i2c_smbus_write_byte_data(i2cClient44, 0x1D, 0x38);
		i2c_smbus_write_byte_data(i2cClient44, 0x20, 0xA0);
		i2c_smbus_write_byte_data(i2cClient44, 0x21, 0x21);
		i2c_smbus_write_byte_data(i2cClient44, 0x25, 0x25);
		i2c_smbus_write_byte_data(i2cClient44, 0x26, 0x12);
		i2c_smbus_write_byte_data(i2cClient44, 0x2B, 0x50);
		i2c_smbus_write_byte_data(i2cClient44, 0x2C, 0x2A);
		i2c_smbus_write_byte_data(i2cClient44, 0x2D, 0x68);
		i2c_smbus_write_byte_data(i2cClient44, 0x2E, 0x5E);
		i2c_smbus_write_byte_data(i2cClient44, 0x30, 0x62);
		i2c_smbus_write_byte_data(i2cClient44, 0x31, 0xBB);
		i2c_smbus_write_byte_data(i2cClient44, 0x32, 0x96);
		i2c_smbus_write_byte_data(i2cClient44, 0x33, 0xC0);
		i2c_smbus_write_byte_data(i2cClient44, 0x35, 0x65);
		i2c_smbus_write_byte_data(i2cClient44, 0x39, 0x84);
		i2c_smbus_write_byte_data(i2cClient44, 0x3A, 0x70);
		i2c_smbus_write_byte_data(i2cClient44, 0x4D, 0x0F);
		i2c_smbus_write_byte_data(i2cClient44, 0x4E, 0x0F);
		i2c_smbus_write_byte_data(i2cClient44, 0xF5, 0x0F);
		i2c_smbus_write_byte_data(i2cClient44, 0xFA, 0xCB);
		i2c_smbus_write_byte_data(i2cClient44, 0xFB, 0xED);
	}
}

#if  0
unsigned char tbl_tp2824c_NTSC_raster[] = {
		// Start address 0x15, Size = 9B
		0x13, 0x4e, 0xbc, 0x15, 0xf0, 0x07, 0x00, 0x09, 0x38
};

static void tp2824c_write_table(unsigned char addr, unsigned char *tbl_ptr, unsigned char tbl_cnt)
{
    unsigned char i = 0;
    for(i = 0; i < tbl_cnt; i ++)
    {
        i2c_smbus_write_byte_data(i2cClient44, (addr + i), *(tbl_ptr + i));
    }
}

static void tp2824c_set_work_mode_NTSC()
{
    // Start address 0x15, Size = 9B
    tp2824c_write_table(0x15, tbl_tp2824c_NTSC_raster, 9);
}

static void tp2824c_reset_default()
{
    unsigned int tmp;
    //tp28xx_byte_write(chip, 0x26, 0x04);
    i2c_smbus_write_byte_data(i2cClient44, 0x07, 0xc0);
    i2c_smbus_write_byte_data(i2cClient44, 0x0b, 0xc0);
    i2c_smbus_write_byte_data(i2cClient44, 0x21, 0x84);
    i2c_smbus_write_byte_data(i2cClient44, 0x38, 0x00);
    i2c_smbus_write_byte_data(i2cClient44, 0x39, 0x1C);
    i2c_smbus_write_byte_data(i2cClient44, 0x3a, 0x32);
    i2c_smbus_write_byte_data(i2cClient44, 0x3B, 0x26);

    tmp = i2c_smbus_read_byte_data(i2cClient44, 0x26);
    tmp &= 0xfe;
    i2c_smbus_write_byte_data(i2cClient44, 0x26, tmp);

    tmp = i2c_smbus_read_byte_data(i2cClient44, 0x06);
    tmp &= 0xfb;
    i2c_smbus_write_byte_data(i2cClient44, 0x06, tmp);
}

static void tp2824c_output()
{
    unsigned int tmp;

    i2c_smbus_write_byte_data(i2cClient44, 0xF5, 0x00);
    i2c_smbus_write_byte_data(i2cClient44, 0xF1, 0x00);
    i2c_smbus_write_byte_data(i2cClient44, 0x4D, 0x0f);
    i2c_smbus_write_byte_data(i2cClient44, 0x4E, 0x0f);
    i2c_smbus_write_byte_data(i2cClient44, 0x4f, 0x03);


        i2c_smbus_write_byte_data(i2cClient44, 0xFA, 0x88);
        i2c_smbus_write_byte_data(i2cClient44, 0xFB, 0x88);
        i2c_smbus_write_byte_data(i2cClient44, 0xF6, 0x00);
        i2c_smbus_write_byte_data(i2cClient44, 0xF7, 0x11);
        i2c_smbus_write_byte_data(i2cClient44, 0xF8, 0x22);
        i2c_smbus_write_byte_data(i2cClient44, 0xF9, 0x33);
        i2c_smbus_write_byte_data(i2cClient44, 0x50, 0x00); //
        i2c_smbus_write_byte_data(i2cClient44, 0x51, 0x00); //
        i2c_smbus_write_byte_data(i2cClient44, 0x52, 0x00); //
        i2c_smbus_write_byte_data(i2cClient44, 0x53, 0x00); //
        i2c_smbus_write_byte_data(i2cClient44, 0xF3, 0x00);
        i2c_smbus_write_byte_data(i2cClient44, 0xF2, 0x00);

            tmp = i2c_smbus_read_byte_data(i2cClient44, 0xFA);
            tmp &= 0x88;
            tmp |= 0x11;
            i2c_smbus_write_byte_data(i2cClient44, 0xFA, tmp);
            tmp = i2c_smbus_read_byte_data(i2cClient44, 0xFB);
            tmp &= 0x88;
            tmp |= 0x11;
            i2c_smbus_write_byte_data(i2cClient44, 0xFB, tmp);
}

void tp2824c_NTSC_960_480_init(void)
{
    unsigned char tmp;

    tp2824c_reset_default();

    tp2824c_output();

     i2c_smbus_write_byte_data(i2cClient44, 0x35, 0x25);
     //set work mode NTSC 9B
     tp2824c_set_work_mode_NTSC();


    tmp = i2c_smbus_read_byte_data(i2cClient44, 0x02);
    tmp &=0xF8;
    tmp |=0x07;
    i2c_smbus_write_byte_data(i2cClient44, 0x02, tmp);

    tmp = i2c_smbus_read_byte_data(i2cClient44, 0xf5);
    tmp |= 0x0f;
    i2c_smbus_write_byte_data(i2cClient44, 0xf5, tmp);

    //data set
    i2c_smbus_write_byte_data(i2cClient44, 0x07, 0xc0);
    i2c_smbus_write_byte_data(i2cClient44, 0x0b, 0xc0);
    i2c_smbus_write_byte_data(i2cClient44, 0x0c, 0x13);
    i2c_smbus_write_byte_data(i2cClient44, 0x0d, 0x50);

    i2c_smbus_write_byte_data(i2cClient44, 0x20, 0x40);
    i2c_smbus_write_byte_data(i2cClient44, 0x21, 0x84);
    i2c_smbus_write_byte_data(i2cClient44, 0x22, 0x36);
    i2c_smbus_write_byte_data(i2cClient44, 0x23, 0x3c);

    i2c_smbus_write_byte_data(i2cClient44, 0x25, 0xff);
    i2c_smbus_write_byte_data(i2cClient44, 0x26, 0x05);
    i2c_smbus_write_byte_data(i2cClient44, 0x27, 0x2d);
    i2c_smbus_write_byte_data(i2cClient44, 0x28, 0x00);

    i2c_smbus_write_byte_data(i2cClient44, 0x2b, 0x70);
    i2c_smbus_write_byte_data(i2cClient44, 0x2c, 0x2a);
    i2c_smbus_write_byte_data(i2cClient44, 0x2d, 0x68);
    i2c_smbus_write_byte_data(i2cClient44, 0x2e, 0x57);

    i2c_smbus_write_byte_data(i2cClient44, 0x30, 0x62);
    i2c_smbus_write_byte_data(i2cClient44, 0x31, 0xbb);
    i2c_smbus_write_byte_data(i2cClient44, 0x32, 0x96);
    i2c_smbus_write_byte_data(i2cClient44, 0x33, 0xc0);
    //i2c_smbus_write_byte_data(i2cClient44, 0x35, 0x25);
    i2c_smbus_write_byte_data(i2cClient44, 0x38, 0x00);
    i2c_smbus_write_byte_data(i2cClient44, 0x39, 0x04);
    i2c_smbus_write_byte_data(i2cClient44, 0x3a, 0x32);
    i2c_smbus_write_byte_data(i2cClient44, 0x3B, 0x26);

    i2c_smbus_write_byte_data(i2cClient44, 0x18, 0x12);

    i2c_smbus_write_byte_data(i2cClient44, 0x13, 0x00);
    tmp = i2c_smbus_read_byte_data(i2cClient44, 0x14);
    tmp &= 0x9f;
    i2c_smbus_write_byte_data(i2cClient44, 0x14, tmp);

    //set sysclock
    //i2c_smbus_write_byte_data(i2cClient44, 0xFA, 0xCB);
    //i2c_smbus_write_byte_data(i2cClient44, 0xFB, 0xED);
}
#endif

void tp2824b_PAL_960_576_init(void)
{
//	i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x04);
	i2c_smbus_write_byte_data(i2cClient44, 0x02, 0xCF);
	i2c_smbus_write_byte_data(i2cClient44, 0x07, 0xC0);
	i2c_smbus_write_byte_data(i2cClient44, 0x0B, 0xC0);
	i2c_smbus_write_byte_data(i2cClient44, 0x0C, 0x43);
	i2c_smbus_write_byte_data(i2cClient44, 0x0D, 0x11);
	i2c_smbus_write_byte_data(i2cClient44, 0x16, 0x5F);
	i2c_smbus_write_byte_data(i2cClient44, 0x17, 0xBC);
	i2c_smbus_write_byte_data(i2cClient44, 0x18, 0x17);
	i2c_smbus_write_byte_data(i2cClient44, 0x19, 0x20);
	i2c_smbus_write_byte_data(i2cClient44, 0x1A, 0x17);
	i2c_smbus_write_byte_data(i2cClient44, 0x1C, 0x09);
	i2c_smbus_write_byte_data(i2cClient44, 0x1D, 0x48);
	i2c_smbus_write_byte_data(i2cClient44, 0x20, 0xB0);
	i2c_smbus_write_byte_data(i2cClient44, 0x21, 0x86);
	i2c_smbus_write_byte_data(i2cClient44, 0x25, 0xFF);
	i2c_smbus_write_byte_data(i2cClient44, 0x26, 0x02);
	i2c_smbus_write_byte_data(i2cClient44, 0x2B, 0x70);
	i2c_smbus_write_byte_data(i2cClient44, 0x2C, 0x2A);
	i2c_smbus_write_byte_data(i2cClient44, 0x2D, 0x60);
	i2c_smbus_write_byte_data(i2cClient44, 0x2E, 0x5E);
	i2c_smbus_write_byte_data(i2cClient44, 0x30, 0x7A);
	i2c_smbus_write_byte_data(i2cClient44, 0x31, 0x4A);
	i2c_smbus_write_byte_data(i2cClient44, 0x32, 0x4D);
	i2c_smbus_write_byte_data(i2cClient44, 0x33, 0xF0);
	i2c_smbus_write_byte_data(i2cClient44, 0x35, 0x65);
	i2c_smbus_write_byte_data(i2cClient44, 0x39, 0x84);
	i2c_smbus_write_byte_data(i2cClient44, 0x3A, 0x70);
	i2c_smbus_write_byte_data(i2cClient44, 0x4D, 0x0F);
	i2c_smbus_write_byte_data(i2cClient44, 0x4E, 0x0F);
	i2c_smbus_write_byte_data(i2cClient44, 0xF5, 0x0F);
	i2c_smbus_write_byte_data(i2cClient44, 0xFA, 0xCB);
	i2c_smbus_write_byte_data(i2cClient44, 0xFB, 0xED);
}

//INPUT CAMERA RESOLUTION
//CAMERA_NTSC(0) / CAMERA_PAL(1)
//CAMERA_960H_NTSC(2) / CAMERA_960H_PAL(3)
//CAMERA_720_NTSC(4) / CAMERA_720_PAL(5)
//CAMERA_1080_NTSC(6) / CAMERA_1080_PAL(7)
//CAMERA_NTSC_960H(8) / CAMERA_PAL_960H(9)
//unknow 0xff


void tp2824b_def_color_set(int color){
	if (color)
		i2c_smbus_write_byte_data(i2cClient44, 0x2A, 0x34); // blue screen
	else
		i2c_smbus_write_byte_data(i2cClient44, 0x2A, 0x30); // black screen
}


void tp2824b_def_pq_set(void){
		i2c_smbus_write_byte_data(i2cClient44, 0x10, 0x78); // brightness
		i2c_smbus_write_byte_data(i2cClient44, 0x11, 0x40); // contrast
		i2c_smbus_write_byte_data(i2cClient44, 0x12, 0x40); // UVgain
		i2c_smbus_write_byte_data(i2cClient44, 0x13, 0x00); // hue
		i2c_smbus_write_byte_data(i2cClient44, 0x14, 0x0f); // sharpness
}


void tp2824b_video_set_brightness_multi_ch(tp2824_video_adjust *v_adj_multi)
{
 	u8 i, reg_val;

	/*
	* 0~254  ->  0~127
	*/
	for (i = 0; i<4; i++){
		if (v_adj_multi[i].ch <= 3){
			printk("[hardy] ch:%d brightness:%d\n", v_adj_multi[i].ch, v_adj_multi[i].value);
			i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x00+v_adj_multi[i].ch);
			reg_val = i2c_smbus_read_byte_data(i2cClient44, 0x10);
			reg_val &= 0x80;
			reg_val |= (v_adj_multi[i].value>>1);
			i2c_smbus_write_byte_data(i2cClient44, 0x10, reg_val);
		}
	}
	i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x04);
}


void tp2824b_video_set_contrast_multi_ch(tp2824_video_adjust *v_adj_multi)
{
 	unsigned char i, reg_val;

	/*
	* 0~254  ->  0~127
	*/
	for (i = 0; i<4; i++){
		if (v_adj_multi[i].ch <= 3){
			i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x00+v_adj_multi[i].ch);
			reg_val = i2c_smbus_read_byte_data(i2cClient44, 0x11);
			reg_val &= 0x80;
			reg_val |= (v_adj_multi[i].value>>1);
			i2c_smbus_write_byte_data(i2cClient44, 0x11, reg_val);
		}
	}
	i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x04);
}


void tp2824b_video_set_sharpness_multi_ch(tp2824_video_adjust *v_adj_multi)
{
 	unsigned char i, reg_val;

	for (i = 0; i<4; i++){
		if (v_adj_multi[i].ch <= 3){
			i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x00+v_adj_multi[i].ch);
			reg_val = i2c_smbus_read_byte_data(i2cClient44, 0x14);
			reg_val &= 0xe0;
			reg_val |= (v_adj_multi[i].value>>3);
			i2c_smbus_write_byte_data(i2cClient44, 0x14, reg_val);
		}
	}
	i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x04);
}


void tp2824b_video_set_hue_multi_ch(tp2824_video_adjust *v_adj_multi)
{
 	unsigned char i, reg_val;

	for (i = 0; i<4; i++){
		if (v_adj_multi[i].ch <= 3){
			i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x00+v_adj_multi[i].ch);
			reg_val = v_adj_multi[i].value;
			i2c_smbus_write_byte_data(i2cClient44, 0x13, reg_val);
		}
	}
	i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x04);
}


void tp2824b_video_set_uvgain_multi_ch(tp2824_video_adjust *v_adj_multi)
{
 	unsigned char i, reg_val;

	for (i = 0; i<4; i++){
		if (v_adj_multi[i].ch <= 3){
			i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x00+v_adj_multi[i].ch);
			reg_val = i2c_smbus_read_byte_data(i2cClient44, 0x12);
			reg_val &= 0x80;
			reg_val |= (v_adj_multi[i].value>>1);
			i2c_smbus_write_byte_data(i2cClient44, 0x12, reg_val);
		}
	}
	i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x04);
}


unsigned char tp2824b_video_get_fmt(void)
{
	int i, j, k, tmp, mode;

	if (videoinfmt != 0xaa)
		return videoinfmt;

	tmp = i2c_smbus_read_byte_data(i2cClient44, 0xF5);
	printk("0xf5 = 0x%02x\n", tmp);
	tmp = i2c_smbus_read_byte_data(i2cClient44, 0x35);
	printk("0x35 = 0x%02x\n", tmp);

	i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x04); // wite all, read ch(0+i)

	j = 0;
	while(i2c_smbus_read_byte_data(i2cClient44, 0x01)&0x80){
		printk("VDLOSS t=%d\n", j);

		if (j>10)
			break;
		msleep(10);
		j++;
	}

	k = 0;

//		tmp = i2c_smbus_read_byte_data(i2cClient44, 0x01);
//		printk("LOCK = 0x%02x\n", tmp);

	do{
		tmp = i2c_smbus_read_byte_data(i2cClient44, 0x01);
		printk("LOCK = 0x%02x\n", tmp);

		mode = (i2c_smbus_read_byte_data(i2cClient44, 0x03)&0x07);
		switch(mode) {
			case 0: //720P_60
			case 1: //720P_50
			default:
				videofmt[0] = 0xff;
				break;

			case 2: //1080P_30
				videofmt[0] = 6;
				if(tmp&0x04){	//tvi
					tp2824b_1080p_30_init(TX_TVI);
				}
				else{			//ahd
					tp2824b_1080p_30_init(TX_AHD);
				}
				break;
			case 3: //1080P_25
				videofmt[0] = 7;
				if(tmp&0x04){	//tvi
					tp2824b_1080p_25_init(TX_TVI);
				}
				else{			//ahd
					tp2824b_1080p_25_init(TX_AHD);
				}
				break;

			case 4: //720P_30
				videofmt[0] = 4;
				if(tmp&0x04){	//tvi
					tp2824b_720p_30_init(TX_TVI);
				}
				else{			//ahd
					tp2824b_720p_30_init(TX_AHD);
				}
				break;

			case 5: //720P_25
				videofmt[0] = 5;
				if(tmp&0x04){	//tvi
					tp2824b_720p_25_init(TX_TVI);
				}
				else{			//ahd
					tp2824b_720p_25_init(TX_AHD);
				}
				break;

			case 6: //SD
				if(tmp&0x04){
			 		videofmt[0] = 3; //960H_PAL
					tp2824b_PAL_960_576_init();
				}
				else{
					videofmt[0] = 2; //960H_NTSC
					tp2824c_NTSC_960_480_init();
				}
				break;
		}

		printk("[T%d] videofmt  = %d\n",k ,videofmt[0]);
		msleep(500);
		k++;
	}while ((0x70 != (tmp&0x70)) &&(k<RTY_CNT));

	for(i=0; i<4; i++){
		i2c_smbus_write_byte_data(i2cClient44, 0x40, (0x04+i)); // wite all, read ch(0+i)
		tmp = i2c_smbus_read_byte_data(i2cClient44, 0x01);
		printk("Ch[%d] Reg 0x01 = 0x%02x\n", i, tmp);
	}

	if(videofmt[0] != 0xff)
		videoinfmt = videofmt[0];
	else
		videoinfmt = 0xff;

	return videoinfmt;
}


void tp2824_get_version(void)
{
	i2c_smbus_write_byte_data(i2cClient44, 0x40, 0x04);	//DPAGE = 0; APAGE = 0; Write ALL ch; Read ch0
	dev_id[0] = i2c_smbus_read_byte_data(i2cClient44, 0xFE);
	dev_id[1] = i2c_smbus_read_byte_data(i2cClient44, 0xFF);
}

