#define _FILE_OFFSET_BITS 64
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_mode.h>

#define SHOW_PICTURE "/tmp/drmshowme"
#define plog //printf
//14byte
typedef struct
{
    char    cfType[2];//"BM"(0x4D42)
    int     cfSize;
    int     cfReserved;
    int     cfoffBits;
}__attribute__((packed)) BITMAPFILEHEADER;

//40byte��Ϣͷ
typedef struct
{
    int ciSize;//BITMAPINFOHEADER��ռ���ֽ���
    int  ciWidth;//���
    int  ciHeight;//�߶�
    short ciPlanes;//Ŀ���豸��λƽ������ֵΪ1
    short ciBitCount;//ÿ�����ص�λ��, 1, 4, 8, 16, 24 or 32
    /*
    * ѹ��˵��,
    * 0 BI_RGB����ѹ����
    * 1 BI_RLE8, for bit8 bitmap only
     * 2 BI_RLE4, for bit4 bitmap only
     * 3 BI_BITFIELDS, bit field, for bit16 or bit32 bitmap
     * 4 BI_JPEG, bitmap include jpeg, for printer only
     * 5 BI_PNG, bitmap include png, for printer only
     */
    int ciCompress;
    int ciSizeImage;//���ֽڱ�ʾ��ͼ���С�������ݱ�����4�ı���
    char ciXPelsPerMeter[4];//Ŀ���豸��ˮƽ������/��
    char ciYPelsPerMeter[4];//Ŀ���豸�Ĵ�ֱ������/��
    int ciClrUsed; //λͼʹ�õ�ɫ�����ɫ��
    int ciClrImportant; //
}__attribute__((packed)) BITMAPINFOHEADER;

typedef struct
{
    unsigned char blue;
    unsigned char green;
    unsigned char red;
    unsigned char reserved;
}__attribute__((packed)) PIXEL;//��ɫģʽRGB

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
    *����16bit RGB565 -> 24bit RGB888 ��ת��
    * 16bit RGB656 R4 R3 R2 R1 R0 G5 G4 G3 G2 G1 G0 B4 B3 B2 B1 B0
    * 24ibt RGB888 R4 R3 R2 R1 R0 0 0 0 G5 G4 G3 G2 G1 G0 0 0 B4 B3 B2 B1 B0 0 0 0
    * 24ibt RGB888 R4 R3 R2 R1 R0 R2 R1 R0 G5 G4 G3 G2 G1 G0 G1 G0 B4 B3 B2 B1 B0 B2 B1 B0
    * ˵�����ڶ��е� 24bit RGB888
����Ϊת����δ���в��������ݣ��ھ����ϻ�����ʧ
    * �����е� 24bit RGB888
����Ϊ�����������������ݣ��Ե�λ������������
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
    * 24bit RGB888 -> 16bit RGB565 ��ת��
    * 24ibt RGB888 R7 R6 R5 R4 R3 R2 R1 R0 G7 G6 G5 G4 G3 G2 G1 G0 B7 B6 B5 B4 B3 B2 B1 B0
    * 16bit RGB656 R7 R6 R5 R4 R3 G7 G6 G5 G4 G3 G2 B7 B6 B5 B4 B3
    *
����λ����8bit��5bit��6bit��ȡԭ8bit�ĸ�λ������������ѹ���
���ȴ��ʧ�˾��ȡ�
    */
    unsigned int r = rgb24[0], g = rgb24[1], b = rgb24[2];

    return (unsigned short)(((b >> 3) << 11) |((g >> 2) << 5) |(r >> 3));
}

drmModeConnector* FindConnector(int fd)
{
    drmModeRes *resources = drmModeGetResources(fd); // drmModeRes描述了计算机所有的显卡信息：connector，encoder，crtc，modes等。
    if (!resources)
    {
        return NULL;
    }

    drmModeConnector* conn = NULL;
    int i = 0;
    for (i = 0; i < resources->count_connectors; i++)
    {
        conn = drmModeGetConnector(fd, resources->connectors[i]);
        if (conn != NULL)
        {
            //找到处于连接状态的Connector。
            if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0)
            {
                for (i = 0; i < conn->count_modes; i++) {
                    drmModeModeInfo *mode = &conn->modes[i];
                    plog("[hardy] conn mode:%s %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
                        mode->name, mode->vrefresh,
                        mode->hdisplay, mode->hsync_start, mode->hsync_end, mode->htotal, mode->hskew,
                        mode->vdisplay, mode->vsync_start, mode->vsync_end, mode->vtotal, mode->vscan,
                        mode->clock, mode->flags, mode->type);
                }
                break;
            }
            else
            {
                drmModeFreeConnector(conn);
            }
        }
    }

    drmModeFreeResources(resources);
    return conn;
}


//查找与Connector匹配的Crtc
int FindCrtcId(int fd, drmModeConnector *conn)
{
    drmModeEncoder *encoder = NULL;
    drmModeRes *resources = drmModeGetResources(fd);
    int crtc_id = 0;
    int i = 0;

    if (!resources)
    {
        fprintf(stderr, "drmModeGetResources failed\n");
        return -1;
    }

    for (i = 0; i < resources->count_encoders; i++) {
        encoder = drmModeGetEncoder(fd, resources->encoders[i]);
        if (!encoder) {
            continue;
        }

        if(encoder->encoder_id == conn->encoder_id) {
            plog("[hardy] encoder:%d %d %d %d %d\n",
                encoder->encoder_id, encoder->encoder_type, encoder->crtc_id,
                encoder->possible_crtcs, encoder->possible_clones);
            break;
        }
    }

    if (encoder) drmModeFreeEncoder(encoder);

    crtc_id = encoder->crtc_id;
    drmModeFreeResources(resources);

    return crtc_id;
}

drmModeCrtc *FindCrtc(int fd, int crtcid)
{
    drmModeCrtcPtr pCrtc = NULL;
    drmModeRes *resources = drmModeGetResources(fd);
    int crtc_id = 0;
    int i = 0;

    if (!resources)
    {
        fprintf(stderr, "drmModeGetResources failed\n");
        return NULL;
    }

    for (i = 0; i < resources-> count_crtcs; i++) {
        pCrtc = drmModeGetCrtc(fd, resources->crtcs[i]);
        if (!pCrtc) {
            continue;
        }

        if(crtcid == pCrtc->crtc_id) {
            drmModeModeInfo *mode = &pCrtc->mode;
            plog("[hardy] crtc:%d %d %d %d %d %d %d\n",
                pCrtc->crtc_id, pCrtc->buffer_id, pCrtc->mode_valid,
                pCrtc->x, pCrtc->y,pCrtc->width, pCrtc->height);
            plog("[hardy] crtc mode:%s %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
                mode->name, mode->vrefresh,
                mode->hdisplay, mode->hsync_start, mode->hsync_end, mode->htotal, mode->hskew,
                mode->vdisplay, mode->vsync_start, mode->vsync_end, mode->vtotal, mode->vscan,
                mode->clock, mode->flags, mode->type);
            break;
        }
    }

    drmModeFreeResources(resources);

    return pCrtc;
}

//绘制一张全色的图
void SetColor(unsigned char *dest, int stride, int w, int h)
{
    struct color {
        unsigned r, g, b;
    };

    struct color ccs[] = {
        { 255, 0, 0 },
        { 0, 255, 0 },
        { 0, 0, 255 },
        { 255, 255, 0 },
        { 0, 255, 255 },
        { 255, 0, 255 }
    };

    static int i = 0;

    unsigned int j, k, off;
    unsigned int r = 255;
    unsigned int g = 1;
    unsigned int b = 1;

    for (j = 0; j < h; ++j)
    {
        i = (j % 50) % 6;
        for (k = 0; k < w; ++k)
        {
            off = stride * j + k * 4;
            *(uint32_t*)&(dest[off]) = (ccs[i].r << 16) | (ccs[i].g << 8) | ccs[i].b;
        }
    }

    i++;

    printf("draw picture\n");
}

int drawBmp(const char *bmp, unsigned char *dest, int stride, int drmw, int drmh)
{
    FILE *fin = (FILE*)fopen(bmp, "rb");
    BITMAPFILEHEADER fhead;
    BITMAPINFOHEADER bminfo;
    unsigned char* tmpbuf = NULL;
    unsigned short* ptr16;
    unsigned int* ptr32;
    unsigned char* ptr8;
    int bpl;
    int x, y;
    int ret;

    int src_xs, src_xe, src_ys, src_ye;
    int dst_xs, dst_xe, dst_ys,dst_ye;

    if(fin == NULL)
    {
        printf("can not find %s\n",bmp);
        return -1;
    }

    // read bmp header
     ret = fread(&fhead, 1, sizeof(BITMAPFILEHEADER), fin);
     ret = fread(&bminfo, 1, sizeof(BITMAPINFOHEADER), fin);

    plog("Image info: size %d*%d, bpp %d, compression %d, offset %d\n",
        bminfo.ciWidth, bminfo.ciHeight, bminfo.ciBitCount, bminfo.ciCompress, fhead.cfoffBits);
    if(bminfo.ciBitCount!=24 && bminfo.ciBitCount!=16)
    {
        printf("not supported bmp file\n");
        fclose(fin);
        return -2;
    }

    // allocate buffer
    tmpbuf = malloc(bminfo.ciWidth*bminfo.ciHeight*bminfo.ciBitCount/8 );
    if(tmpbuf == NULL)
    {
        printf("malloc faild\n");
        fclose(fin);
        return -3;
    }

    fseek(fin, fhead.cfoffBits, SEEK_SET);
    ret = fread(tmpbuf, 1, bminfo.ciWidth*bminfo.ciHeight*bminfo.ciBitCount/8, fin);
    fclose(fin);

    if (drmw <= bminfo.ciWidth) {
        src_xs = (bminfo.ciWidth-drmw)/2;
        src_xe = src_xs+drmw;

        dst_xs =0, dst_xe = drmw;
    } else {
        src_xs = 0; src_xe = bminfo.ciWidth;

        dst_xs = (drmw -bminfo.ciWidth)/2;
        dst_xe = dst_xs+bminfo.ciWidth;
    }

    if (drmh <= bminfo.ciHeight) {
        src_ys = (bminfo.ciHeight-drmh)/2;
        src_ye = src_ys+drmh;

        dst_ys =0, dst_ye=drmh;
    } else {
        src_ys = 0; src_ye = bminfo.ciHeight;

        dst_ys = (drmh -bminfo.ciHeight)/2;
        dst_ye = dst_ys+bminfo.ciHeight;
    }

    // copy image into framebuffer
    ptr8 = (unsigned char*) dest;
    bpl = bminfo.ciWidth*bminfo.ciBitCount/8;

    if (24 == bminfo.ciBitCount )
    {
        for(y=src_ys;y<src_ye;y++)
        {
            // the first pixel of bmp line
            int src_ls = y*bpl;
            // the first pixel of drm line
            int dst_ls = (dst_ye-(y-src_ys)-1)* drmw;

            for(x=src_xs;x<src_xe;x++)
            {
                int src_pix = src_ls+x*3;
                int dst_pix = (dst_ls+dst_xs + (x-src_xs))<<2;

                *(int*)(ptr8+dst_pix) = 0xFF000000 | (tmpbuf[src_pix+2]<<16) | (tmpbuf[src_pix+1]<<8) |tmpbuf[src_pix];
            }
        }
    }
    else if (16 == bminfo.ciBitCount  && 3 == bminfo.ciCompress)
    {
        for(y=src_ys;y<src_ye;y++)
        {
            for(x=src_xs;x<src_xe;x++)
            {
                rgb565_2_rgb24(ptr8+((dst_ye-y-1)* drmw + x-src_xs+dst_xs)*4, *(short*)(tmpbuf+y*bpl+x*2));
                ptr8[((dst_ye-y-1)* drmw + x-src_xs+dst_xs)*4+3] = 255;
            }
        }
    }

    free(tmpbuf);

    return 0;
}

void storeImage(unsigned char* buf, drmModeFBPtr drmfb, char *name)
{
    FILE *fin = (FILE*)fopen(name, "w");
    BITMAPFILEHEADER fhead;
    BITMAPINFOHEADER bminfo;
    PIXEL rgb_quad[3];//�����ɫ��
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

    plog("drmfb:[%d %d] bpp:%d\n", drmfb->width, drmfb->height, drmfb->bpp);

    fhead.cfType[0] = 'B', fhead.cfType[1] = 'M';
    bminfo.ciSize = sizeof(BITMAPINFOHEADER);
    bminfo.ciWidth = drmfb->width;
    bminfo.ciHeight = drmfb->height;
    bminfo.ciPlanes = 1;

    // write image data
    ptr8 = (unsigned char*) buf;
    switch(drmfb->bpp)
    {
    case 32:
        bminfo.ciBitCount = 24;
        bminfo.ciSizeImage = drmfb->width * drmfb->height * 3;

        bminfo.ciCompress = 0;
        fhead.cfoffBits = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
        fhead.cfSize = fhead.cfoffBits + drmfb->width * drmfb->height * 3;
        fseek(fin, fhead.cfoffBits, SEEK_SET);

        for (y=0; y<drmfb->height; y++)
        {
            for(x=0; x<drmfb->width; x++)
            {
                fwrite(ptr8 + ((drmfb->height-y-1)* drmfb->width + x)*4, 1, 3, fin);
            }
        }
        break;

    case 24:
        break;

    case 16:
#if 1        //
        bminfo.ciBitCount = 16;
        bminfo.ciCompress = 3; //BI_BITFIELDS;//λ���ŷ�ʽ�����ݵ�ɫ������֪��RGBռbit��
        fhead.cfoffBits = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER) + sizeof(rgb_quad);

        bminfo.ciSizeImage = drmfb->width * drmfb->height * 2;
        fhead.cfSize = fhead.cfoffBits + bminfo.ciSizeImage;

        //RGB565��ʽ����
        *(int*)rgb_quad = RGB565_MASK_RED;
        *(int*)(rgb_quad+1) = RGB565_MASK_GREEN;
        *(int*)(rgb_quad+2) = RGB565_MASK_BLUE;
        fseek(fin, sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER), SEEK_SET);
        fwrite(&rgb_quad, 1, sizeof(rgb_quad), fin);

        fseek(fin, fhead.cfoffBits, SEEK_SET);
        for (y=0; y<drmfb->height; y++)
        {
            fwrite(ptr8 + (drmfb->height-y-1)* drmfb->width*2, 2, drmfb->width, fin);
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

int main(int argc, char *argv[])
{
    int ret, fd;
    char bmpfile[256] = {0};
    char tmp[256] = {0};
    FILE *fp = NULL;

    int len;

    if (argc < 2) {
        printf("demo:\n");
        printf("     drmtest a.bmp\n\n");
        printf("     change picture -- \"echo b.bmp > "SHOW_PICTURE"\"\n\n");
        return 0;
    } else {
        strcpy(bmpfile, argv[1]);
        printf("[hardy] picture:%s\n", bmpfile);
    }

    fd = open("/dev/dri/card0", O_RDWR);
    if (fd < 0)
    {
        /* Probably permissions error */
        fprintf(stderr, "couldn't open %s, skipping\n", "");
        return -1;
    }

    drmSetMaster(fd);

    drmModeConnectorPtr connector = FindConnector(fd);

    int crtcid = 0;
    crtcid = FindCrtcId(fd, connector);

    drmModeCrtc *crtc = FindCrtc(fd, crtcid);

    if(access(bmpfile, F_OK) != 0) {
        drmModeFBPtr drmfb = drmModeGetFB(fd, crtc->buffer_id);

        struct drm_mode_map_dumb mreq; //请求映射缓存到内存。
        mreq.handle = drmfb->handle;

        ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
        if (ret)
        {
            printf("map dumb failed!\n");
            return -1;
        }

        unsigned char* buf = mmap(0, drmfb->pitch*drmfb->height,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
        if (buf == MAP_FAILED)
        {
            printf("mmap failed!\n");
            return -1;
        }

        storeImage(buf, drmfb, bmpfile);
        drmModeFreeFB(drmfb);
        goto exit;
    }

    int width = connector->modes[0].hdisplay;
    int height = connector->modes[0].vdisplay;

    struct drm_mode_create_dumb creq;
    memset(&creq, 0, sizeof(creq));

    creq.width = width;
    creq.height = height;
    creq.bpp = 32;
    creq.flags = 0;

    ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
    if (ret)
    {
        printf("create dumb failed!\n");
    }

    uint32_t framebuffer = -1;
    uint32_t stride = creq.pitch;

    //使用缓存的handel创建一个FB，返回fb的id：framebuffer。
#if 1
    ret = drmModeAddFB(fd, width, height, 24, 32, creq.pitch, creq.handle, &framebuffer);
#else
    unsigned int handles[4], pitches[4], offsets[4];
    handles[0] = creq.handle;
    pitches[0] = creq.pitch;
    offsets[0] = 0;
    ret = drmModeAddFB2(fd, width, height, DRM_FORMAT_ARGB8888, handles, pitches, offsets,
            &framebuffer, 0);
#endif
    if (ret)
    {
        printf("failed to create fb\n");
        return -1;
    }

    drmModeSetCrtc(fd, crtcid, framebuffer, 0, 0,
        &connector->connector_id, 1, &connector->modes[0]);

    struct drm_mode_map_dumb mreq; //请求映射缓存到内存。
    mreq.handle = creq.handle;

    ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
    if (ret)
    {
        printf("map dumb failed!\n");
        return -1;
    }

    // 猜测：创建的缓存位于显存上，在使用之前先使用drm_mode_map_dumb将其映射到内存空间。
    // 但是映射后缓存位于内核内存空间，还需要一次mmap才能被程序使用。
    //unsigned char* buf = mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
    unsigned char* buf = mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
    if (buf == MAP_FAILED)
    {
        printf("mmap failed!\n");
        return -1;
    }

    memset(buf, 0, creq.size);

#if 0
    int cc = 0;
    while (cc < 3)
    {
        SetColor(buf, stride, width, height);
        drmModePageFlip(fd, crtcid, framebuffer, DRM_MODE_PAGE_FLIP_EVENT, 0);
        cc++;
        while (1) ;
        usleep(100*1000);
    }
#else
    if(access(bmpfile, F_OK) == 0) {
        drawBmp(bmpfile, buf, stride, width, height);
    }

    while (1) {
//        drmModePageFlip(fd, crtcid, framebuffer, DRM_MODE_PAGE_FLIP_EVENT, 0);
        usleep(40*1000);

        fp = fopen(SHOW_PICTURE,"rb");
        if (NULL == fp) continue;

        fgets(tmp, sizeof(tmp), fp);
        fclose(fp);
        while ((len=strlen(tmp)) > 0) {
            char *pc = tmp+len-1;

            if (' ' == *pc || '\r' == *pc || '\n' == *pc) {
                *pc = 0;
            } else {
                break;
            }
        }
        system("rm -f "SHOW_PICTURE);

        if (0 == strcmp("-1", tmp)) {
            break;
        }

        if (0 != strcmp(tmp, bmpfile) ) {
            if (0 == drawBmp(tmp, buf, stride, width, height)) {
                strcpy(bmpfile, tmp);
                //drmModePageFlip(fd, crtcid, framebuffer, DRM_MODE_PAGE_FLIP_EVENT, 0);
            }
        }
    }
#endif

    munmap(buf, creq.size);
exit:
    if (NULL != crtc) drmModeFreeCrtc(crtc);
    if (NULL != connector) drmModeFreeConnector(connector);
    close(fd);
    exit(0);
}

