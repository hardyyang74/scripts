
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <string.h>

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
    int ciSize;//BITMAPINFOHEADER所占的字节数
    int  ciWidth;//宽度
    int  ciHeight;//高度
    short ciPlanes;//目标设备的位平面数，值为1
    short ciBitCount;//每个像素的位数, 1, 4, 8, 16, 24 or 32
    /*
    * 压缩说明,
    * 0 BI_RGB（不压缩）
    * 1 BI_RLE8, for bit8 bitmap only
     * 2 BI_RLE4, for bit4 bitmap only
     * 3 BI_BITFIELDS, bit field, for bit16 or bit32 bitmap
     * 4 BI_JPEG, bitmap include jpeg, for printer only
     * 5 BI_PNG, bitmap include png, for printer only
     */
    int ciCompress;
    int ciSizeImage;//用字节表示的图像大小，该数据必须是4的倍数
    char ciXPelsPerMeter[4];//目标设备的水平像素数/米
    char ciYPelsPerMeter[4];//目标设备的垂直像素数/米
    int ciClrUsed; //位图使用调色板的颜色数
    int ciClrImportant; //指定重要的颜色数，当该域的值等于颜色数时（或者等于0时），表示所有颜色都一样重要
}__attribute__((packed)) BITMAPINFOHEADER;

typedef struct
{
    unsigned char blue;
    unsigned char green;
    unsigned char red;
    unsigned char reserved;
}__attribute__((packed)) PIXEL;//颜色模式RGB

/*
-------------------------------------------------
|segment name    | struct                                                      |size                 |
-------------------------------------------------
|file head                | BITMAPFILEHEADER                      |14                   |
|bitmap info          | BITMAPINFOHEADER                     |40                   |
|palette                    | PIXEL                                                      |N/A               |
|raw data                | N/A                                                         | N/A               |
-------------------------------------------------
*/
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
    BITMAPFILEHEADER fhead;
    BITMAPINFOHEADER bminfo;
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
     ret = fread(&fhead, 1, sizeof(BITMAPFILEHEADER), fin);
     ret = fread(&bminfo, 1, sizeof(BITMAPINFOHEADER), fin);
    printf("Image info: size %d*%d, bpp %d, compression %d, offset %d\n", bminfo.ciWidth, bminfo.ciHeight, bminfo.ciBitCount, bminfo.ciCompress, fhead.cfoffBits);
    if(bminfo.ciBitCount!=24  &&  bminfo.ciBitCount!=16)
    {
        printf("not supported bmp file\n");
        fclose(fin);
        return;
    }

    // allocate buffer
    tmpbuf = malloc(bminfo.ciWidth*bminfo.ciHeight*bminfo.ciBitCount/8 );
    if(tmpbuf == NULL)
    {
        printf("malloc faild\n");
        fclose(fin);
        return;
    }

    fseek(fin, fhead.cfoffBits, SEEK_SET);
    ret = fread(tmpbuf, 1, bminfo.ciWidth*bminfo.ciHeight*bminfo.ciBitCount/8, fin);
    fclose(fin);

    copy_w = (var->xres <= bminfo.ciWidth) ? var->xres : bminfo.ciWidth;
    copy_h = (var->yres <= bminfo.ciHeight) ? var->yres : bminfo.ciHeight;

    // copy image into framebuffer
    ptr8 = (unsigned char*) fb;
    bpl = bminfo.ciWidth*bminfo.ciBitCount/8;
    switch(var->bits_per_pixel)
    {
    case 32:
        if (24 == bminfo.ciBitCount )
        {
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
        }
        else if (16 == bminfo.ciBitCount  &&  3 == bminfo.ciCompress)
        {
            for(y=0;y<copy_h;y++)
            {
                for(x=0;x<copy_w;x++)
                {
                    rgb565_2_rgb24(ptr8+((var->yres-y-1)* var->xres + x)*4, *(short*)(tmpbuf+y*bpl+x*2));
                    ptr8[((var->yres-y-1)* var->xres + x)*4+3] = 255;
                }
            }
        }

        break;

    case 24:
        break;

    case 16:
        if (24 == bminfo.ciBitCount)
        {
            for(y=0;y<copy_h;y++)
            {
                for(x=0;x<copy_w;x++)
                {
                    *(short*)(ptr8+((var->yres-y-1) * var->xres + x)*2) = rgb_24_2_565(tmpbuf + y*bpl+x*3);
                }
            }
        }
        else // rgb565
        {
            for(y=0;y<copy_h;y++)
            {
                memcpy(ptr8+(var->yres-y-1) * var->xres *2, tmpbuf + y*bpl, bpl);
            }
        }
        break;

    default:
        break;

    }

    free(tmpbuf);
}

void storeImage(struct fb_var_screeninfo *var, char *name, void* fb)
{
    FILE *fin = (FILE*)fopen(name, "w");
    BITMAPFILEHEADER fhead;
    BITMAPINFOHEADER bminfo;
    PIXEL rgb_quad[3];//定义调色板
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

    memset((void*)&fhead, 0 , sizeof(BITMAPFILEHEADER));
    memset((void*)&bminfo, 0 , sizeof(BITMAPINFOHEADER));

    fhead.cfType[0] = 'B', fhead.cfType[1] = 'M';
    bminfo.ciSize = sizeof(BITMAPINFOHEADER);
    bminfo.ciWidth = var->xres;
    bminfo.ciHeight = var->yres;
    bminfo.ciPlanes = 1;

    // write image data
    ptr8 = (unsigned char*) fb;
    switch(var->bits_per_pixel)
    {
    case 32:
        bminfo.ciBitCount = 24;
        bminfo.ciSizeImage = var->xres * var->yres * 3;

        bminfo.ciCompress = 0;
        fhead.cfoffBits = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
        fhead.cfSize = fhead.cfoffBits + var->xres * var->yres * 3;
        fseek(fin, fhead.cfoffBits, SEEK_SET);

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
#if 1        //
        bminfo.ciBitCount = 16;
        bminfo.ciCompress = 3; //BI_BITFIELDS;//位域存放方式，根据调色板掩码知道RGB占bit数
        fhead.cfoffBits = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER) + sizeof(rgb_quad);

        bminfo.ciSizeImage = var->xres * var->yres * 2;
        fhead.cfSize = fhead.cfoffBits + bminfo.ciSizeImage;

        //RGB565格式掩码
        *(int*)rgb_quad = RGB565_MASK_RED;
        *(int*)(rgb_quad+1) = RGB565_MASK_GREEN;
        *(int*)(rgb_quad+2) = RGB565_MASK_BLUE;
        fseek(fin, sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER), SEEK_SET);
        fwrite(&rgb_quad, 1, sizeof(rgb_quad), fin);

        fseek(fin, fhead.cfoffBits, SEEK_SET);
        for (y=0; y<var->yres; y++)
        {
            fwrite(ptr8 + (var->yres-y-1)* var->xres*2, 2, var->xres, fin);
        }
#else
        // save 24 bpp
        bminfo.ciBitCount = 24;
        bminfo.ciSizeImage = var->xres * var->yres * 3;

        bminfo.ciCompress = 0;
        fhead.cfoffBits = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
        fhead.cfSize = fhead.cfoffBits + var->xres * var->yres * 3;
        fseek(fin, fhead.cfoffBits, SEEK_SET);

        for (y=0; y<var->yres; y++)
        {
            for(x=0; x<var->xres; x++)
            {
                rgb565_2_rgb24(rgb24, *(short*)(ptr8 + ((var->yres-y-1)* var->xres + x)*2));
                fwrite(rgb24, 1, sizeof(rgb24), fin);
            }
        }
#endif
        break;

    default:
        break;
    }

    // save bmp header
    printf("Image info: size %d*%d, bpp %d, compression %d, offset %d\n", bminfo.ciWidth, bminfo.ciHeight, bminfo.ciBitCount, bminfo.ciCompress, fhead.cfoffBits);
    fseek(fin, 0, SEEK_SET);
    fwrite(&fhead, 1, sizeof(BITMAPFILEHEADER), fin);
    fwrite(&bminfo, 1, sizeof(BITMAPINFOHEADER), fin);

    fflush(fin);
    fclose(fin);
}

void drawGrayScale(struct fb_fix_screeninfo *finfo, struct fb_var_screeninfo *vinfo, char* fbp)
{
    int x = 0, y = 0,k;
    unsigned char red,green,blue;
    long location = 0;
    const int scalenum = 8;

    switch (vinfo->bits_per_pixel) {
        default: // 16bit
            for(k=0;k<scalenum;k++)
            {
                red = k*255/scalenum;
                green =k*255/scalenum;
                blue = k*255/scalenum;

                red = red>>3;
                green = green>>2;
                blue = blue>>3;

                for(x=vinfo->xres*k/scalenum;x<vinfo->xres*(k+1)/scalenum;x++)
                {
                    for(y=0;y<vinfo->yres;y++)
                    {
                        location = x * (vinfo->bits_per_pixel / 8) + y *finfo->line_length;

                        *((unsigned short *)(fbp + location)) = (red << 11) + (green << 5) + (blue << 0);
                        //*(fbp + location + 1) = 0xe0;
                    }
                }
            }
            break;

        case 32:
            for(k=0;k<scalenum;k++)
            {
                red = k*255/scalenum;
                green =k*255/scalenum;
                blue = k*255/scalenum;

                int color = (0xff<<vinfo->transp.offset) + (red << 16) + (green << 8) + (blue);

                for(x=vinfo->xres*k/scalenum;x<vinfo->xres*(k+1)/scalenum;x++)
                {
                    location = x << 2;
                    *(int *)(fbp + location) = color;
                }
            }

            for(y=1;y<vinfo->yres;y++)
            {
                memcpy(fbp+y*finfo->line_length, fbp, finfo->line_length);
            }
            break;
    }
}

void drawColorBar(struct fb_fix_screeninfo *finfo, struct fb_var_screeninfo *vinfo, char* fbp)
{
    int x = 0, y = 0;
    int linewidth = 5;
    long location = 0;

    switch (vinfo->bits_per_pixel) {
        default: // 16bit
            //red
            for(y=0;y<vinfo->yres/3;y++)
            {
                for(x=0;x<vinfo->xres;x++)
                {
                    location = x * (vinfo->bits_per_pixel / 8) + y *finfo->line_length;
                    *(fbp + location) = 0xf8;
                    *(fbp + location + 1) = 0;
                }

            }

            //b
            for(y=vinfo->yres/3;y<vinfo->yres/3*2;y++)
            {
                for(x=0;x<vinfo->xres;x++)
                {
                    location = x * (vinfo->bits_per_pixel / 8) + y *finfo->line_length;
                    *(fbp + location) = 0;
                    *(fbp + location + 1) = 0x1f;
                }

            }

            //g
            for(y=vinfo->yres/3*2;y<vinfo->yres;y++)
            {
                for(x=0;x<vinfo->xres;x++)
                {
                    location = x * (vinfo->bits_per_pixel / 8) + y *finfo->line_length;
                    *(fbp + location) = 0x7;
                    *(fbp + location + 1) = 0xe0;
                }

            }
            break;

        case 32:
            // white rect
            for (y=0; y<vinfo->yres; y++) {
                int color = (0xff<<vinfo->transp.offset) | (0xff<<vinfo->red.offset) | (0xff<<vinfo->blue.offset) | (0x80<<vinfo->blue.offset);
                location = y*finfo->line_length;

                if ((y < linewidth) || (y > vinfo->yres-linewidth) ) {
                    for(x=0;x<vinfo->xres;x++) {
                        *(int*)(fbp + location+(x<<2)) = color;
                    }
                } else {
                    for(x=0;x<linewidth;x++) {
                        *(int*)(fbp + location+(x<<2)) = color;
                    }
                    for(x=vinfo->xres-linewidth;x<vinfo->xres;x++) {
                        *(int*)(fbp + location+(x<<2)) = color;
                    }
                }
            }

            //red
            for(x=linewidth;x<vinfo->xres-linewidth;x++)
            {
                location = (x << 2) + linewidth * finfo->line_length;
                *(int*)(fbp + location) = (0xff<<vinfo->transp.offset) | (0xff<<vinfo->red.offset);
            }
            for(y=linewidth;y<vinfo->yres/3;y++)
            {
                memcpy(fbp+y*finfo->line_length, fbp+linewidth * finfo->line_length, finfo->line_length);
            }

            // g
            for(x=linewidth;x<vinfo->xres-linewidth;x++)
            {
                location = (x << 2) + vinfo->yres/3 * finfo->line_length;
                *(int*)(fbp + location) = (0xff<<vinfo->transp.offset) | (0xff<<vinfo->green.offset);
            }
            for(y=vinfo->yres/3+1;y<vinfo->yres/3*2;y++)
            {
                memcpy(fbp+y*finfo->line_length, fbp+vinfo->yres/3 * finfo->line_length, finfo->line_length);
            }

            // b
            for(x=linewidth;x<vinfo->xres-linewidth;x++)
            {
                location = (x << 2) + vinfo->yres/3*2 *finfo->line_length;
                *(int*)(fbp + location) = (0xff<<vinfo->transp.offset) | (0xff<<vinfo->blue.offset);
            }
            for(y=vinfo->yres/3*2+1;y<vinfo->yres-linewidth;y++)
            {
                memcpy(fbp+y*finfo->line_length, fbp+vinfo->yres/3*2 * finfo->line_length, finfo->line_length);
            }
#if 0
            printf("red:0x%08x green:0x%08x blue:0x%08x\n",
                (0xff<<vinfo->transp.offset) | (0xff<<vinfo->red.offset),
                (0xff<<vinfo->transp.offset) | (0xff<<vinfo->green.offset),
                (0xff<<vinfo->transp.offset) | (0xff<<vinfo->blue.offset));
#endif

            // left top
            for (y=0; y<2*linewidth; y++) {
                location = y*finfo->line_length;

                for(x=0;x<2*linewidth;x++) {
                    *(int*)(fbp + location+(x<<2)) = 0xffffffff;
                }
            }
            break;
    }
}

void drawGradualColor(struct fb_fix_screeninfo *finfo, struct fb_var_screeninfo *vinfo, char* fbp)
{
    int x = 0, y = 0, k;
    unsigned char c;
    long location = 0;
    int dotnum = vinfo->xres*vinfo->yres;

    switch (vinfo->bits_per_pixel) {
        default: // 16bit
            break;

        case 32:
            location = 0;
            for(y=0;y<vinfo->yres;y++) {
                c = y*256/vinfo->yres;

                for(x=0;x<vinfo->xres/4;x++) {
                    *(int*)(fbp + location) = (0xff<<vinfo->transp.offset) | (c<<vinfo->red.offset);
                    location +=4 ;
                }
                for(;x<vinfo->xres/4*2;x++) {
                    *(int*)(fbp + location) = (0xff<<vinfo->transp.offset) | (c<<vinfo->green.offset);
                    location +=4 ;
                }
                for(;x<vinfo->xres/4*3;x++) {
                    *(int*)(fbp + location) = (0xff<<vinfo->transp.offset) | (c<<vinfo->blue.offset);
                    location +=4 ;
                }
                for(;x<vinfo->xres;x++) {
                    *(int*)(fbp + location) = (0xff<<vinfo->transp.offset) | (c<<vinfo->red.offset) | (c<<vinfo->green.offset) | (c<<vinfo->blue.offset);
                    location +=4 ;
                }
            }
            break;
    }
}

void drawAllColor(struct fb_fix_screeninfo *finfo, struct fb_var_screeninfo *vinfo, char* fbp)
{
    int x = 0, y = 0;
    int r, g=0, b=0;
    long location = 0;
    char xmin = (vinfo->xres<vinfo->yres);

    switch (vinfo->bits_per_pixel) {
        default: // 16bit
            break;

        case 32:
            location = 0;
#if 0
            for(y=0;y<vinfo->yres;y++) {
                if (xmin) {
                    switch (y/255) {
                        default: // g++
                            g++;
                            break;
                        case 1:
                            b++;
                            break;
                        case 2:
                            g--;
                            break;
                        case 3:
                            b--;
                            break;
                        case 4:
                            g++;
                            b++;
                            break;
                        case 5:
                            g--;
                            b -= 2;
                            break;
                        case 6:
                            g += 2;
                            b--;
                            break;
                    }
                } else {
                    r = (y & 0x100) ? (~y)&0xff:(y&0xff);
                }

                for(x=0;x<vinfo->xres;x++) {
                    if (xmin) {
                        r = (x & 0x100) ? (~x)&0xff:(x&0xff);
                    } else {
                        switch (x/255) {
                            default: // g++
                                g++;
                                break;
                            case 1:
                                b++;
                                break;
                            case 2:
                                g--;
                                break;
                            case 3:
                                b--;
                                break;
                            case 4:
                                g++;
                                b++;
                                break;
                            case 5:
                                g--;
                                b -= 2;
                                break;
                            case 6:
                                g += 2;
                                b--;
                                break;
                        }
                    }
                    //printf("x:%d r:%d\n", x, r);

                    *(int*)(fbp + location) = (0xff<<vinfo->transp.offset) | ((r&0xff)<<vinfo->red.offset) | ((g&0xff)<<vinfo->green.offset) | ((b&0xff)<<vinfo->blue.offset);
                    location += 4;
                }
            }
#else
            for(y=0;y<vinfo->yres;y++) {
                for(x=0;x<vinfo->xres;x++) {
                    r = 255-255*(x+1)/vinfo->xres;
                    b = 255*(x+1)/vinfo->xres;
                    g = 255*(y+1)/vinfo->yres;

                    //r = (r & 0x100) ? ~r : r;
                    //g = (g & 0x100) ? ~g : g;
                    //b = (b & 0x100) ? ~b : b;
                    //printf("x:%d r:%d\n", x, r);

                    *(int*)(fbp + location) = (0xff<<vinfo->transp.offset) | ((r&0xff)<<vinfo->red.offset) | ((g&0xff)<<vinfo->green.offset) | ((b&0xff)<<vinfo->blue.offset);
                    location += 4;
                }
            }
#endif
            break;
    }
}

int main (int argc, char *argv[])
{
    int fp=0;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    char *fbp = 0;

    char devfb[128] = "/dev/fb0";
    char bmpname[128] = "/media/usb1p1/fb.bmp";

    if (3 == argc)
    {
        if (NULL != strstr(argv[1], "/dev/") )
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
            "       ex2, draw bmp to frame buffer: fbtest /media/usb1p1/fb.bmp /dev/fb0\n"
            "       ex3, draw color to frame buffer: fbtest 0~4 /dev/fb0\n"
            "            0 -- clear screen\n"
            "            1 -- draw grayscale\n"
            "            2 -- draw color bar\n"
            "            3 -- draw radual color\n"
            "            4 -- all color\n");
        return 1;
    }

    fp = open (devfb,O_RDWR);

    if (fp < 0)
    {
        printf("Error : Can not open framebuffer device\n");
        return 1;
    }

    if(ioctl(fp,FBIOGET_FSCREENINFO,&finfo))
    {
        printf("Error reading fixed information\n");
        return 2;
    }

    if(ioctl(fp,FBIOGET_VSCREENINFO,&vinfo))
    {
        printf("Error reading variable information\n");
        return 3;
    }

    printf("------------xres:%d,yres:%d,bits_per_pixel:%d\n",vinfo.xres,vinfo.yres,vinfo.bits_per_pixel);
    printf("------------R:offset:%d,length:%d,msb_right:%d\n",vinfo.red.offset,vinfo.red.length,vinfo.red.msb_right);
    printf("------------G:offset:%d,length:%d,msb_right:%d\n",vinfo.green.offset,vinfo.green.length,vinfo.green.msb_right);
    printf("------------B:offset:%d,length:%d,msb_right:%d\n",vinfo.blue.offset,vinfo.blue.length,vinfo.blue.msb_right);

    fbp =(char *) mmap (0, finfo.smem_len, PROT_READ | PROT_WRITE,MAP_SHARED, fp,0);
    if ((int) fbp == -1)
    {
        printf ("Error: failed to map framebuffer device tomemory.\n");
        return 4;
    }

    if (NULL != strstr(argv[1], "/dev/fb") ){ // save screen into image
        storeImage(&vinfo,bmpname,fbp);
    } else if (0 == strcmp("0", argv[1])) { // clean fb
        memset(fbp, 0, finfo.smem_len );
    } else if (0 == strcmp("1", argv[1])) { // draw grayscale
        drawGrayScale(&finfo, &vinfo, fbp);
    } else if (0 == strcmp("2", argv[1])) { // draw color bar
        drawColorBar(&finfo, &vinfo, fbp);
    } else if (0 == strcmp("3", argv[1])) { // draw radual color
        drawGradualColor(&finfo, &vinfo, fbp);
    } else if (0 == strcmp("4", argv[1])) { // draw radual color
        drawAllColor(&finfo, &vinfo, fbp);
    } else { // draw image
        renderImage(&vinfo,bmpname,fbp);
    }

    munmap(fbp, finfo.smem_len);

    close (fp);

    return 0;

}


