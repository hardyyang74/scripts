
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <string.h>

char BIT_MASK[] = {0, 0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff};

#define TRANSP_V (0xff&BIT_MASK[vinfo->transp.length])
#define RED_V (0xff&BIT_MASK[vinfo->red.length])
#define GREEN_V (0xff&BIT_MASK[vinfo->green.length])
#define BLUE_V (0xff&BIT_MASK[vinfo->blue.length])

#define TRANSP_VAL (TRANSP_V<<vinfo->transp.offset)
#define RED_VAL (RED_V<<vinfo->red.offset)
#define GREEN_VAL (GREEN_V<<vinfo->green.offset)
#define BLUE_VAL (BLUE_V<<vinfo->blue.offset)

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
        red = k*RED_V/scalenum;
        green =k*GREEN_V/scalenum;
        blue = k*BLUE_V/scalenum;

        int color = TRANSP_VAL
            + (red << vinfo->red.offset) + (green << vinfo->green.offset) + (blue<<vinfo->blue.offset);

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
    color = TRANSP_VAL | RED_VAL;
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
    color = TRANSP_VAL | GREEN_VAL;
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
    color = TRANSP_VAL | BLUE_VAL;
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
    color = TRANSP_VAL
        | (RED_V<<vinfo->red.offset)
        | (GREEN_V<<vinfo->green.offset)
        | ((BLUE_V>>1)<<vinfo->blue.offset);
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
    color = TRANSP_VAL|RED_VAL|GREEN_VAL|BLUE_VAL;
    for (y=0; y<2*linewidth; y++) {
        location = y *finfo->line_length;
        for(x=0;x<2*linewidth;x++) {
            PIXEL_COLOR(location+x*pixelen, color);
        }
    }

    // right bottom
    color = TRANSP_VAL;
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
        color = TRANSP_VAL | ((RED_V*y/vinfo->yres)<<vinfo->red.offset);
        for(x=0;x<vinfo->xres/4;x++) {
            PIXEL_COLOR(location, color);
            location += pixelen;
        }

        color = TRANSP_VAL | ((GREEN_V*y/vinfo->yres)<<vinfo->green.offset);
        for(;x<vinfo->xres*2/4;x++) {
            PIXEL_COLOR(location, color);
            location += pixelen;
        }

        color = TRANSP_VAL | ((BLUE_V*y/vinfo->yres)<<vinfo->blue.offset);
        for(;x<vinfo->xres*3/4;x++) {
            PIXEL_COLOR(location, color);
            location += pixelen;
        }

        color = TRANSP_VAL | ((RED_V*y/vinfo->yres)<<vinfo->red.offset)
            | ((GREEN_V*y/vinfo->yres)<<vinfo->green.offset)
            | ((BLUE_V*y/vinfo->yres)<<vinfo->blue.offset);
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
            r = RED_V-RED_V*(x+1)/vinfo->xres;
            b = GREEN_V*(x+1)/vinfo->xres;
            g = BLUE_V*(y+1)/vinfo->yres;

            //r = (r & 0x100) ? ~r : r;
            //g = (g & 0x100) ? ~g : g;
            //b = (b & 0x100) ? ~b : b;
            //printf("x:%d r:%d\n", x, r);

            PIXEL_COLOR(location, TRANSP_VAL | (r<<vinfo->red.offset) | (g<<vinfo->green.offset) | (b<<vinfo->blue.offset));
            location += pixelen;
        }
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
    } else if (0 == strcmp("5", argv[1])) { // white
        memset(fbp, 0xff, finfo.smem_len );
    } else { // draw image
        renderImage(&vinfo,bmpname,fbp);
    }

    munmap(fbp, finfo.smem_len);

    close (fp);

    return 0;

}


