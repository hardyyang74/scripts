
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <string.h>

typedef struct _BMP_HEADER
{
    char    cfType[2];//文件类型，"BM"(0x4D42)
    unsigned int file_size;
    unsigned int reserved1;
    unsigned int data_offset;
    unsigned int header_size;
    unsigned int width;
    unsigned int height;
    unsigned short planes;
    unsigned short bpp;
    unsigned int compression;
    char ciSizeImage[4];//用字节表示的图像大小，该数据必须是4的倍数
    char reserved[20];
} __attribute__((packed)) BMP_HEADER;

#if 0
//14byte文件头
typedef struct
{
    char    cfType[2];//文件类型，"BM"(0x4D42)
    int     cfSize;//文件大小（字节）
    int     cfReserved;//保留，值为0
    int     cfoffBits;//数据区相对于文件头的偏移量（字节）
}__attribute__((packed)) BITMAPFILEHEADER;
//__attribute__((packed))的作用是告诉编译器取消结构在编译过程中的优化对齐

//40byte信息头
typedef struct
{
    char ciSize[4];//BITMAPFILEHEADER所占的字节数
    int  ciWidth;//宽度
    int  ciHeight;//高度
    char ciPlanes[2];//目标设备的位平面数，值为1
    short ciBitCount;//每个像素的位数
    char ciCompress[4];//压缩说明
    char ciSizeImage[4];//用字节表示的图像大小，该数据必须是4的倍数
    char ciXPelsPerMeter[4];//目标设备的水平像素数/米
    char ciYPelsPerMeter[4];//目标设备的垂直像素数/米
    char ciClrUsed[4]; //位图使用调色板的颜色数
    char ciClrImportant[4]; //指定重要的颜色数，当该域的值等于颜色数时（或者等于0时），表示所有颜色都一样重要
}__attribute__((packed)) BITMAPINFOHEADER;

typedef struct
{
    unsigned char blue;
    unsigned char green;
    unsigned char red;
    unsigned char reserved;
}__attribute__((packed)) PIXEL;//颜色模式RGB
#endif

#define RGB565_MASK_RED        0xF800
#define RGB565_MASK_GREEN                         0x07E0
#define RGB565_MASK_BLUE                         0x001F

void rgb565_2_rgb24(char *rgb24, short rgb565)
{
    /*
    *　　16bit RGB565 -> 24bit RGB888 的转换
    * 16bit RGB656 R4 R3 R2 R1 R0 G5 G4 G3 G2 G1 G0 B4 B3 B2 B1 B0
    * 24ibt RGB888 R4 R3 R2 R1 R0 0 0 0 G5 G4 G3 G2 G1 G0 0 0 B4 B3 B2 B1 B0 0 0 0
    * 24ibt RGB888 R4 R3 R2 R1 R0 R2 R1 R0 G5 G4 G3 G2 G1 G0 G1 G0 B4 B3 B2 B1 B0 B2 B1 B0
    * 说明：第二行的 24bit RGB888 数据为转换后，未进行补偿的数据，在精度上会有损失
    * 第三行的 24bit RGB888 数据为经过量化补偿的数据，对低位做了量化补偿
    */

    //extract RGB
    rgb24[2] = (rgb565 & RGB565_MASK_RED) >> 11;
    rgb24[1] = (rgb565 & RGB565_MASK_GREEN) >> 5;
    rgb24[0] = (rgb565 & RGB565_MASK_BLUE);

    //amplify the image
    rgb24[2] = (rgb24[2] << 3) | (rgb24[2] & 3);
    rgb24[1] = (rgb24[1] << 2) | (rgb24[1] & 2);
    rgb24[0] = (rgb24[0] << 3) | (rgb24[0] & 3);
}

unsigned short rgb_24_2_565(const unsigned char *rgb24)
{
    /*
    * 24bit RGB888 -> 16bit RGB565 的转换
    * 24ibt RGB888 R7 R6 R5 R4 R3 R2 R1 R0 G7 G6 G5 G4 G3 G2 G1 G0 B7 B6 B5 B4 B3 B2 B1 B0
    * 16bit RGB656 R7 R6 R5 R4 R3 G7 G6 G5 G4 G3 G2 B7 B6 B5 B4 B3
    * 量化位数从8bit到5bit或6bit，取原8bit的高位，量化上做了压缩，却损失了精度。
    */
    unsigned int r = rgb24[0], g = rgb24[1], b = rgb24[2];

    return (unsigned short)(((b >> 3) << 11) |((g >> 2) << 5) |(r >> 3));
}

void renderImage(struct fb_var_screeninfo *var, char *name, void* fb)
{
    FILE *fin = (FILE*)fopen(name, "rb");
    BMP_HEADER bmp;
    unsigned char* tmpbuf = NULL;
    unsigned short* ptr16;
    unsigned int* ptr32;
    unsigned char* ptr8;
    int bpl;
    int x, y;
    int copy_w, copy_h;
    int ret;

    if(fin == NULL)
    {
        printf("can not find %s\n",name);
        return;
    }

    // read bmp header
     ret = fread(&bmp, 1, sizeof(BMP_HEADER), fin);
    printf("Image info: size %d*%d, bpp %d, compression %d, offset %d\n", bmp.width, bmp.height, bmp.bpp, bmp.compression, bmp.data_offset);
    if((bmp.bpp!=24) || bmp.compression)
    {
        printf("not supported bmp file\n");
        fclose(fin);
        return;
    }

    // allocate buffer
    tmpbuf = malloc(bmp.width*bmp.height*bmp.bpp/8 );
    if(tmpbuf == NULL)
    {
        printf("malloc faild\n");
        fclose(fin);
        return;
    }

    fseek(fin, bmp.data_offset, SEEK_SET);
    ret = fread(tmpbuf, 1, bmp.width*bmp.height*bmp.bpp/8+1024, fin);
    fclose(fin);

    copy_w = (var->xres <= bmp.width) ? var->xres : bmp.width;
    copy_h = (var->yres <= bmp.height) ? var->yres : bmp.height;

    // copy image into framebuffer
    ptr8 = (unsigned char*) fb;
    bpl = bmp.width*bmp.bpp/8;
    switch(var->bits_per_pixel)
    {
    case 32:
        for(y=0;y<copy_h;y++)
        {
            for(x=0;x<copy_w;x++)
            {
                ptr8[((var->yres-y-1)* var->xres + x)*4] = tmpbuf[y*bpl+x*3];
                ptr8[((var->yres-y-1)* var->xres + x)*4+1] = tmpbuf[y*bpl+x*3+1];
                ptr8[((var->yres-y-1)* var->xres + x)*4+2] = tmpbuf[y*bpl+x*3+2];
                ptr8[((var->yres-y-1)* var->xres + x)*4+3] = 255;
            }
        }

        break;

    case 24:
        for(y=0;y<copy_h;y++)
        {
            for(x=0;x<copy_w;x++)
            {
                    ptr8[((var->yres-y-1) * var->xres + x)*4] = tmpbuf[y*bpl+x*3];
                    ptr8[((var->yres-y-1) * var->xres + x)*4+1] = tmpbuf[y*bpl+x*3+1];
                    ptr8[((var->yres-y-1) * var->xres + x)*4+2] = tmpbuf[y*bpl+x*3+2];
            }
        }
        break;

    case 16:
        for(y=0;y<copy_h;y++)
        {
            for(x=0;x<copy_w;x++)
            {
                *(short*)(ptr8+((var->yres-y-1) * var->xres + x)*2) = rgb_24_2_565(tmpbuf + y*bpl+x*3);
            }
        }
        break;

    default:
        break;

    }
}

void storeImage(struct fb_var_screeninfo *var, char *name, void* fb)
{
    FILE *fin = (FILE*)fopen(name, "w");
    BMP_HEADER bmp;
    unsigned char* ptr8;
    int bpl;
    int x, y;
    int ret;
    char rgb24[3];

    if(fin == NULL)
    {
        printf("can not find %s\n",name);
        return;
    }

    memset((void*)&bmp, 0 , sizeof(BMP_HEADER));
    bmp.cfType[0] = 'B', bmp.cfType[1] = 'M';
    bmp.file_size = sizeof(BMP_HEADER) + var->xres * var->yres * 3;
    bmp.data_offset = sizeof(BMP_HEADER);
    bmp.header_size = 40;
    bmp.width = var->xres;
    bmp.height = var->yres;
    bmp.bpp = 24;
    bmp.compression = 0;
    bmp.ciSizeImage[4] = var->xres * var->yres * 3;

    // read bmp header
    printf("Image info: size %d*%d, bpp %d, compression %d, offset %d\n", bmp.width, bmp.height, bmp.bpp, bmp.compression, bmp.data_offset);
    fwrite(&bmp, 1, sizeof(BMP_HEADER), fin);

    // write image data
    ptr8 = (unsigned char*) fb;
    switch(var->bits_per_pixel)
    {
    case 32:
        for (y=0; y<var->yres; y++)
        {
            for(x=0; x<var->xres; x++)
            {
                fwrite(ptr8 + ((var->yres-y-1)* var->xres + x)*4, 1, 3, fin);
            }
        }
        break;

    case 24:
        break;

    case 16:
        for (y=0; y<var->yres; y++)
        {
            for(x=0; x<var->xres; x++)
            {
                rgb565_2_rgb24(rgb24, *(short*)(ptr8 + ((var->yres-y-1)* var->xres + x)*2));
                fwrite(rgb24, 1, sizeof(rgb24), fin);
            }
        }
        break;

    default:
        break;
    }

    fflush(fin);
    fclose(fin);
}

int main (int argc, char *argv[])
{
    int fp=0;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    long screensize=0;
    char *fbp = 0;

    int x = 0, y = 0,k;
    unsigned char red,green,blue;
    long location = 0;

    char devfb[128] = "/dev/fb0";
    char bmpname[128] = "/media/usb1p1/fb.bmp";

    if (3 == argc)
    {
        if (NULL != strstr(argv[1], "/dev/fb") )
        {
            strcpy(devfb, argv[1]);
            strcpy(bmpname, argv[2]);
        }
        else
        {
            strcpy(bmpname, argv[1]);
            strcpy(devfb, argv[2]);
        }
    }
    else
    {
        printf("    usage: fbtest source dest\n"
            "       ex1, capture framebuffer: fbtest /dev/fb0 /media/usb1p1/fb.bmp\n"
            "       ex2, draw bmp to frame buffer: fbtest /media/usb1p1/fb.bmp /dev/fb0\n");
        return 1;
    }

    fp = open (devfb,O_RDWR);

    if (fp < 0)
    {
        printf("Error : Can not open framebuffer device/n");
        return 1;
    }

    if(ioctl(fp,FBIOGET_FSCREENINFO,&finfo))
    {
        printf("Error reading fixed information/n");
        return 2;
    }

    if(ioctl(fp,FBIOGET_VSCREENINFO,&vinfo))
    {
        printf("Error reading variable information/n");
        return 3;
    }

    printf("------------xres:%d,yres:%d,bits_per_pixel:%d\n",vinfo.xres,vinfo.yres,vinfo.bits_per_pixel);
    printf("------------R:offset:%d,length:%d,msb_right:%d\n",vinfo.red.offset,vinfo.red.length,vinfo.red.msb_right);
    printf("------------G:offset:%d,length:%d,msb_right:%d\n",vinfo.green.offset,vinfo.green.length,vinfo.green.msb_right);
    printf("------------B:offset:%d,length:%d,msb_right:%d\n",vinfo.blue.offset,vinfo.blue.length,vinfo.blue.msb_right);

    screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel /8;
    fbp =(char *) mmap (0, screensize, PROT_READ | PROT_WRITE,MAP_SHARED, fp,0);
    if ((int) fbp == -1)
    {
        printf ("Error: failed to map framebuffer device tomemory./n");
        return 4;
    }

#if 1
#if 1
    if (NULL != strstr(argv[1], "/dev/fb") ){
        storeImage(&vinfo,bmpname,fbp);
    } else {
        renderImage(&vinfo,bmpname,fbp);
    }
#else
    for(k=0;k<10;k++)
    {

        for(x=vinfo.xres*k/10;x<vinfo.xres*(k+1)/10;x++)
        {

            for(y=0;y<vinfo.yres;y++)
            {
                location = x * (vinfo.bits_per_pixel / 8) + y *finfo.line_length;

                red = k*255/10;
                green =k*255/10;
                blue = k*255/10;

                red = red>>3;
                green = green>>2;
                blue = blue>>3;

                *((unsigned short *)(fbp + location)) = (red << 11) + (green << 5) + (blue << 0);

                //*(fbp + location + 1) = 0xe0;

            }

        }
    }
#endif

#else


        //red
        for(y=0;y<vinfo.yres/3;y++)
        {

            for(x=0;x<vinfo.xres;x++)
            {
                location = x * (vinfo.bits_per_pixel / 8) + y *finfo.line_length;

                *(fbp + location) = 0xf8;

                *(fbp + location + 1) = 0;

                //*(fbp + location + 2) = 0;

                //*(fbp + location + 3) = 0;
            }

        }

        //b
        for(y=vinfo.yres/3;y<vinfo.yres/3*2;y++)
        {

            for(x=0;x<vinfo.xres;x++)
            {

                location = x * (vinfo.bits_per_pixel / 8) + y *finfo.line_length;

                *(fbp + location) = 0;

                *(fbp + location + 1) = 0x1f;

                //*(fbp + location + 2) = 0;

                //*(fbp + location + 3) = 0;
            }

        }


        //g
        for(y=vinfo.yres/3*2;y<vinfo.yres;y++)
        {

            for(x=0;x<vinfo.xres;x++)
            {

                location = x * (vinfo.bits_per_pixel / 8) + y *finfo.line_length;

                *(fbp + location) = 0x7;

                *(fbp + location + 1) = 0xe0;

                //*(fbp + location + 2) = 255;

                //*(fbp + location + 3) = 0;
            }

        }
#endif

    munmap(fbp, screensize);

    close (fp);

    return 0;

}


