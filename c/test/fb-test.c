
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/ioctl.h>

char BIT_MASK[] = {0, 0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff};

#define TRGB_V(c) (0xff&BIT_MASK[vinfo->c.length])
#define TRGB_VAL(c) (TRGB_V(c)<<vinfo->c.offset)
#define RGB_2_FBRGB(c,v) (((v>>(8-vinfo->c.length))<<vinfo->c.offset))

#define PIXEL_COLOR(location, color) \
do \
{ \
    if (2 == pixelen) { \
        *(short*)(fbp + (location)) = (color); \
    } else { \
        *(int*)(fbp + (location)) = (color); \
    } \
} while(0)

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
    unsigned int  ciWidth;//宽度
    unsigned int  ciHeight;//高度
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

void clearScreen(struct fb_fix_screeninfo *finfo, struct fb_var_screeninfo *vinfo, char* fbp)
{
    unsigned int x = 0, y = 0;

    char pixelen = vinfo->bits_per_pixel/8;

    int black = TRGB_VAL(transp);
    for (x=0; x<vinfo->xres; x++) {
        PIXEL_COLOR(x*pixelen, black);
    }
    for (y=1; y<vinfo->yres; y++) {
        memcpy(fbp+y*finfo->line_length, fbp, finfo->line_length);
    }
}

void renderImage(struct fb_fix_screeninfo *finfo, struct fb_var_screeninfo *vinfo, char *name, void* fbp)
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

    unsigned long location = 0;
    char pixelen = vinfo->bits_per_pixel/8;

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

    copy_w = (vinfo->xres <= bminfo.ciWidth) ? vinfo->xres : bminfo.ciWidth;
    copy_h = (vinfo->yres <= bminfo.ciHeight) ? vinfo->yres : bminfo.ciHeight;

    // copy image into framebuffer
    ptr8 = (unsigned char*) fbp;
    bpl = bminfo.ciWidth*bminfo.ciBitCount/8;

    if (24 == bminfo.ciBitCount )
    {
        for(y=0;y<copy_h;y++)
        {
            unsigned long l_src = y*bpl;
            location = (vinfo->yres-y-1) * finfo->line_length;
            for(x=0;x<copy_w;x++)
            {
                PIXEL_COLOR(location,
                    TRGB_VAL(transp)|RGB_2_FBRGB(red,tmpbuf[l_src+2])|RGB_2_FBRGB(green,tmpbuf[l_src+1])|RGB_2_FBRGB(blue,tmpbuf[l_src]));

                location += pixelen;
                l_src += 3;
            }
        }
    }
    else if (16 == bminfo.ciBitCount  &&  3 == bminfo.ciCompress)
    {
        for(y=0;y<copy_h;y++)
        {
            for(x=0;x<copy_w;x++)
            {
                rgb565_2_rgb24(ptr8+((vinfo->yres-y-1)* vinfo->xres + x)*4, *(short*)(tmpbuf+y*bpl+x*2));
                ptr8[((vinfo->yres-y-1)* vinfo->xres + x)*4+3] = 255;
            }
        }
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
    unsigned int x, y;
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
    unsigned int x = 0, y = 0,k;
    unsigned char red,green,blue;
    const unsigned int scalenum = 8;

    char pixelen = vinfo->bits_per_pixel/8;

    for(k=0;k<scalenum;k++)
    {
        int color = TRGB_VAL(transp)
            + RGB_2_FBRGB(red, 255*k/scalenum) + RGB_2_FBRGB(green, 255*k/scalenum) + RGB_2_FBRGB(blue, 255*k/scalenum);

        for(x=vinfo->xres*k/scalenum;x<vinfo->xres*(k+1)/scalenum;x++)
        {
            PIXEL_COLOR(x * pixelen, color);
        }
    }

    for(y=1;y<vinfo->yres;y++)
    {
        memcpy(fbp+y*finfo->line_length, fbp, finfo->line_length);
    }
}

void drawColorBar(struct fb_fix_screeninfo *finfo, struct fb_var_screeninfo *vinfo, char* fbp)
{
    int color = 0;
    unsigned int x = 0, y = 0, sy = 0;
    unsigned int linewidth = 5;
    unsigned long location = 0;

    char pixelen = vinfo->bits_per_pixel/8;

    //red
    color = TRGB_VAL(transp) | TRGB_VAL(red);
    location = sy = 0;
    for(x=0;x<vinfo->xres;x++)
    {
        PIXEL_COLOR(location, color);
        location += pixelen;
    }
    for(y = 1;y<vinfo->yres/3;y++)
    {
        memcpy(fbp+y*finfo->line_length, fbp+sy*finfo->line_length, finfo->line_length);
    }

    // g
    color = TRGB_VAL(transp) | TRGB_VAL(green);
    location = y * finfo->line_length;
    sy = y;
    for(x=0;x<vinfo->xres;x++)
    {
        PIXEL_COLOR(location, color);
        location += pixelen;
    }
    for(y++;y<vinfo->yres*2/3;y++)
    {
        memcpy(fbp+y*finfo->line_length, fbp+sy*finfo->line_length, finfo->line_length);
    }

    // b
    color = TRGB_VAL(transp) | TRGB_VAL(blue);
    location = y * finfo->line_length;
    sy = y;
    for(x=0;x<vinfo->xres;x++)
    {
        PIXEL_COLOR(location, color);
        location += pixelen;
    }
    for(y++;y<vinfo->yres-linewidth;y++)
    {
        memcpy(fbp+y*finfo->line_length, fbp+sy*finfo->line_length, finfo->line_length);
    }

    // white rect
    color = TRGB_VAL(transp) | TRGB_VAL(red) | TRGB_VAL(green) | RGB_2_FBRGB(blue, 128);
    for (y=0; y<vinfo->yres; y++) {
        location = y *finfo->line_length;

        if ((y < linewidth) || (y > (vinfo->yres-linewidth-1)) ) {
            for(x=0;x<vinfo->xres;x++) {
                PIXEL_COLOR(location+x*pixelen, color);
            }
        } else {
            for(x=0;x<linewidth;x++) {
                PIXEL_COLOR(location+x*pixelen, color);
            }
            for(x=vinfo->xres-linewidth;x<vinfo->xres;x++) {
                PIXEL_COLOR(location+x*pixelen, color);
            }
        }
    }

    // left top
    color = TRGB_VAL(transp)|TRGB_VAL(red)|TRGB_VAL(green)|TRGB_VAL(blue);
    for (y=0; y<2*linewidth; y++) {
        location = y *finfo->line_length;
        for(x=0;x<2*linewidth;x++) {
            PIXEL_COLOR(location+x*pixelen, color);
        }
    }

    // right bottom
    color = TRGB_VAL(transp);
    for (y=vinfo->yres-2*linewidth; y<vinfo->yres; y++) {
        location = y *finfo->line_length;
        for(x=vinfo->xres-2*linewidth;x<vinfo->xres;x++) {
            PIXEL_COLOR(location+x*pixelen, color);
        }
    }
}

void drawGradualColor(struct fb_fix_screeninfo *finfo, struct fb_var_screeninfo *vinfo, char* fbp)
{
    unsigned int x = 0, y = 0, k;
    unsigned int color = 0;
    unsigned long location = 0;
    unsigned int dotnum = vinfo->xres*vinfo->yres;

    char pixelen = vinfo->bits_per_pixel/8;

    location = 0;
    for(y=0;y<vinfo->yres;y++) {
        color = TRGB_VAL(transp) | RGB_2_FBRGB(red, 255*y/vinfo->yres);
        for(x=0;x<vinfo->xres/4;x++) {
            PIXEL_COLOR(location, color);
            location += pixelen;
        }

        color = TRGB_VAL(transp) | RGB_2_FBRGB(green, 255*y/vinfo->yres);
        for(;x<vinfo->xres*2/4;x++) {
            PIXEL_COLOR(location, color);
            location += pixelen;
        }

        color = TRGB_VAL(transp) | RGB_2_FBRGB(blue, 255*y/vinfo->yres);
        for(;x<vinfo->xres*3/4;x++) {
            PIXEL_COLOR(location, color);
            location += pixelen;
        }

        color = TRGB_VAL(transp) | RGB_2_FBRGB(red, 255*y/vinfo->yres)
            | RGB_2_FBRGB(green, 255*y/vinfo->yres)
            | RGB_2_FBRGB(blue, 255*y/vinfo->yres);
        for(;x<vinfo->xres;x++) {
            PIXEL_COLOR(location, color);
            location += pixelen;
        }
    }
}

void drawAllColor(struct fb_fix_screeninfo *finfo, struct fb_var_screeninfo *vinfo, char* fbp)
{
    unsigned int x = 0, y = 0;
    unsigned int r, g=0, b=0;
    unsigned long location = 0;
    char xmin = (vinfo->xres<vinfo->yres);

    char pixelen = vinfo->bits_per_pixel/8;

    location = 0;
    for(y=0;y<vinfo->yres;y++) {
        for(x=0;x<vinfo->xres;x++) {
            r = RGB_2_FBRGB(red, 255-255*(x+1)/vinfo->xres);
            g = RGB_2_FBRGB(green, 255*(y+1)/vinfo->yres);
            b = RGB_2_FBRGB(blue, 255*(x+1)/vinfo->xres);

            PIXEL_COLOR(location, TRGB_VAL(transp) | r | g | b);
            location += pixelen;
        }
    }
}

void drawPercent(struct fb_fix_screeninfo *finfo, struct fb_var_screeninfo *vinfo, char* fbp, int percent, int reverse)
{
    unsigned int x = 0, y = 0;
    unsigned int c = RGB_2_FBRGB(transp, 255)|RGB_2_FBRGB(red, 128)|RGB_2_FBRGB(green, 128)|RGB_2_FBRGB(blue, 128);
    unsigned long location = 0;
    char xmin = (vinfo->xres<vinfo->yres);

    char pixelen = vinfo->bits_per_pixel/8;

    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    if (xmin) {
        char linelen = 100; // unit: pixel
        char *linebuf = (char*)malloc(pixelen*linelen);
        const int gapW=20;

        // draw line
        for (x=0;x<linelen;x++) {
            if (2 == pixelen) {
                *(short*)(linebuf+x*pixelen) = c;
            } else {
                *(int*)(linebuf+x*pixelen) = c;
            }
        }

        // copy line
        int progressEnd = (vinfo->yres-(gapW<<1)) * percent/100 + gapW;
        for(y=gapW;y<progressEnd;y++) {
            if (reverse) {
                location = (vinfo->yres-y)*finfo->line_length + (finfo->line_length-linelen*pixelen)/2;
            } else {
                location = y*finfo->line_length + (finfo->line_length-linelen*pixelen)/2;
            }
            memcpy(fbp+location, linebuf, pixelen*linelen);
        }

        free(linebuf);
    } else {
        const int gapW=20, progressW=100;
        int startY=(vinfo->yres-progressW)>>1, endY=(vinfo->yres+progressW)>>1;

        int linelen = (vinfo->xres-(gapW<<1)) * percent/100; // unit: pixel
        char *linebuf = (char*)malloc(pixelen*linelen);

        // draw line
        for (x=0;x<linelen;x++) {
            if (2 == pixelen) {
                *(short*)(linebuf+x*pixelen) = c;
            } else {
                *(int*)(linebuf+x*pixelen) = c;
            }
        }

        // copy line
        for(y=startY;y<endY;y++) {
            int startX=gapW, endX=startX+linelen;

            if (reverse) {
                location = y*finfo->line_length + (vinfo->xres-gapW-linelen)*pixelen;
            } else {
                location = y*finfo->line_length + startX*pixelen;
            }
            memcpy(fbp+location, linebuf, pixelen*linelen);
        }

        free(linebuf);
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

    if (argc >= 3 )
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
            "       ex3, draw color to frame buffer: fbtest 0~6 /dev/fb0\n"
            "            0 -- clear screen\n"
            "            1 -- draw grayscale\n"
            "            2 -- draw color bar\n"
            "            3 -- draw radual color\n"
            "            4 -- all color\n"
            "            5 -- white\n"
            "            6 -- percent\n"
            "                 para3 is percent\n"
            "                 para4: direction is reverse\n");
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

    printf("------------xres:%d,yres:%d,bits_per_pixel:%d memlen:%d\n",vinfo.xres,vinfo.yres,vinfo.bits_per_pixel,finfo.smem_len);
    printf("------------R:offset:%d,length:%d,msb_right:%d\n",vinfo.red.offset,vinfo.red.length,vinfo.red.msb_right);
    printf("------------G:offset:%d,length:%d,msb_right:%d\n",vinfo.green.offset,vinfo.green.length,vinfo.green.msb_right);
    printf("------------B:offset:%d,length:%d,msb_right:%d\n",vinfo.blue.offset,vinfo.blue.length,vinfo.blue.msb_right);

    fbp =(char *) mmap (0, finfo.smem_len, PROT_READ | PROT_WRITE,MAP_SHARED, fp,0);
    if ((int) fbp == -1)
    {
        printf ("Error: failed to map framebuffer device tomemory.\n");
        return 4;
    }

    printf("------------fbp:0x%x smem_start:0x%x mmio_start:0x%x\n", fbp, finfo.smem_start, finfo.mmio_start);

    if (NULL != strstr(argv[1], "/dev/fb") ){ // save screen into image
        storeImage(&vinfo,bmpname,fbp + finfo.line_length*vinfo.yoffset);
    } else if (0 == strcmp("0", argv[1])) { // clean fb
        memset(fbp, 0, finfo.smem_len);
    } else if (0 == strcmp("1", argv[1])) { // draw grayscale
        drawGrayScale(&finfo, &vinfo, fbp + finfo.line_length*vinfo.yoffset);
    } else if (0 == strcmp("2", argv[1])) { // draw color bar
        drawColorBar(&finfo, &vinfo, fbp + finfo.line_length*vinfo.yoffset);
    } else if (0 == strcmp("3", argv[1])) { // draw radual color
        drawGradualColor(&finfo, &vinfo, fbp + finfo.line_length*vinfo.yoffset);
    } else if (0 == strcmp("4", argv[1])) { // draw radual color
        drawAllColor(&finfo, &vinfo, fbp + finfo.line_length*vinfo.yoffset);
    } else if (0 == strcmp("5", argv[1])) { // white
        memset(fbp, 0xff, finfo.smem_len );
    } else if (0 == strcmp("6", argv[1])) { // percent
        int reverse=0, percent=100;
        if (argc >= 4) percent = atoi(argv[3]);
        if (argc >= 5) reverse = atoi(argv[4]);

        //ioctl(fp, FBIO_WAITFORVSYNC, NULL);
        //vinfo.yoffset = (0==vinfo.yoffset) ? vinfo.yres : 0;
        clearScreen(&finfo, &vinfo, fbp + finfo.line_length*vinfo.yoffset);
        drawPercent(&finfo, &vinfo, fbp + finfo.line_length*vinfo.yoffset, percent, reverse);
        //ioctl(fp, FBIOPAN_DISPLAY, &vinfo);
    } else { // draw image
        renderImage(&finfo, &vinfo,bmpname,fbp);
    }

    munmap(fbp, finfo.smem_len);

    close (fp);

    return 0;

}


