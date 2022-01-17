#define _FILE_OFFSET_BITS 64
#include <errno.h>
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

#if SYSWRAPPER
#include "syswrapper.h"
#endif

#define plog //printf
#define SHOW_PICTURE "/tmp/drmshowme"
#define BMP_W 1280
#define BMP_H 720

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

struct buffer_object {
    uint32_t width, height, size;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t handle;
    uint32_t fb_id;
    uint8_t *fb;
};

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

drmModeConnector* FindConnector(int fd, drmModeRes *resources)
{
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

    return conn;
}

static drmModeCrtcPtr drmFoundCrtc(int fd, drmModeResPtr res,
            drmModeConnector * conn, int *crtc_index)
{
  int i;
  int crtc_id;
  drmModeEncoder *enc;
  drmModeCrtc *crtc;
  uint32_t crtcs_for_connector = 0;

  crtc_id = -1;
  for (i = 0; i < res->count_encoders; i++) {
    enc = drmModeGetEncoder (fd, res->encoders[i]);
    if (enc) {
      if (enc->encoder_id == conn->encoder_id) {
        crtc_id = enc->crtc_id;
        drmModeFreeEncoder (enc);
        break;
      }
      drmModeFreeEncoder (enc);
    }
  }

  /* If no active crtc was found, pick the first possible crtc */
  if (crtc_id == -1) {
    for (i = 0; i < conn->count_encoders; i++) {
      enc = drmModeGetEncoder (fd, conn->encoders[i]);
      crtcs_for_connector |= enc->possible_crtcs;
      drmModeFreeEncoder (enc);
    }

    if (crtcs_for_connector != 0)
      crtc_id = res->crtcs[ffs (crtcs_for_connector) - 1];
  }

  if (crtc_id == -1)
    return NULL;

  for (i = 0; i < res->count_crtcs; i++) {
    crtc = drmModeGetCrtc (fd, res->crtcs[i]);
    if (crtc) {
      if (crtc_id == crtc->crtc_id) {
        if (crtc_index)
          *crtc_index= i;
        return crtc;
      }
      drmModeFreeCrtc (crtc);
    }
  }

  return NULL;
}

drmModeCrtcPtr FindCrtc(int fd, int crtcid)
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

static uint32_t get_property_id(int fd, drmModeObjectPropertiesPtr props, const char *name)
{
    drmModePropertyPtr prop = NULL;
    uint32_t j, id = 0;

    for (uint32_t j = 0; j < props->count_props; j++) {
        prop = drmModeGetProperty(fd, props->props[j]);
        if (prop) {
            if (!strcmp(prop->name, name)) {
                id = prop->prop_id;
                plog("[hardy] name:%s id:%d\n", prop->name, prop->prop_id);
            }
            drmModeFreeProperty(prop);
        }

        if (id) break;
    }

    return id;
}

void adjsutZpos(int fd, uint32_t planeid, int zpos)
{
    drmModeObjectPropertiesPtr props;

    props = drmModeObjectGetProperties(fd, planeid, DRM_MODE_OBJECT_PLANE);

    uint32_t property_zpos = get_property_id(fd, props, "zpos");
    if (property_zpos) {
        drmModeAtomicReq *req;

        req = drmModeAtomicAlloc();
        drmModeAtomicAddProperty(req, planeid, property_zpos, zpos);
        drmModeAtomicCommit(fd, req, 0, NULL);
        drmModeAtomicFree(req);
    }

    drmModeFreeObjectProperties(props);
}

void print_plane(int fd, drmModePlanePtr p)
{
    drmModeObjectPropertiesPtr props = NULL;
    drmModePropertyPtr prop = NULL;

    props = drmModeObjectGetProperties(fd, p->plane_id,DRM_MODE_OBJECT_PLANE);
    if (!props) {
        plog("[hardy] get DRM_MODE_OBJECT_PLANE fail\n");
        return;
    }

    plog("[hardy] plane id:%d", p->plane_id);
    for (uint32_t j = 0; j < props->count_props; j++) {
        prop = drmModeGetProperty(fd, props->props[j]);
        if (prop) {
            if (!strcmp(prop->name, "NAME")) {
                plog(" %s:%s", prop->name, prop->enums[0].name);
            } else {
                plog(" name:%s", prop->name);
            }
            drmModeFreeProperty(prop);
        }
    }
    plog("\n");
}

drmModePlanePtr FindPlaneByName(int fd, const char *name)
{
    int i, j;
    drmModePlanePtr plane = NULL;
    drmModePlaneResPtr pr = drmModeGetPlaneResources(fd);
    drmModeObjectPropertiesPtr props;
    drmModePropertyPtr prop;
    uint32_t found_plane = 0;

    if (!pr) {
        printf("drmModeGetPlaneResources faild!\n");
        return NULL;
    }

    for (i=0; i<pr->count_planes; i++) {
        plane = drmModeGetPlane(fd, pr->planes[i]);
        if (!plane) {
            continue;
        }

        plog("[hardy] plane id:%d crtc_id:%d fb_id=%d possible_crtcs:%d\n",
            plane->plane_id, plane->crtc_id, plane->fb_id, plane->possible_crtcs);

        //if (plane->crtc_id == crtcid) break;

        props = drmModeObjectGetProperties(fd, plane->plane_id,DRM_MODE_OBJECT_PLANE);
        if (!props) {
            printf("failed to found props plane[%d] %s\n",
                   plane->plane_id, strerror(errno));
            drmModeFreePlane(plane);
            plane = NULL;
            continue;
        }

        for (j = 0; j < props->count_props; j++) {
            prop = drmModeGetProperty(fd, props->props[j]);
            if (!strcmp(prop->name, "NAME")) {
                plog(" %s:%s acquire name:%s\n", prop->name, prop->enums[0].name, name);
            }
            plog(" %s\n", prop->name);
            if (!strcmp(prop->name, "NAME")
                && !strcmp(name, prop->enums[0].name)) {
                drmModeFreeProperty(prop);
                found_plane = 1;
                break;
            }
            drmModeFreeProperty(prop);
        }

        drmModeFreeObjectProperties(props);
        if (found_plane) break;
        drmModeFreePlane(plane);
        plane = NULL;
    }

    if (pr) drmModeFreePlaneResources(pr);

    return plane;
}

drmModePlanePtr FindPlaneByCrtc(int fd, uint32_t crtcid)
{
    int i, j;
    drmModePlanePtr plane = NULL;
    drmModePlaneResPtr pr = drmModeGetPlaneResources(fd);
    drmModeObjectPropertiesPtr props;
    drmModePropertyPtr prop;
    uint32_t found_plane = 0;

    if (!pr) {
        printf("drmModeGetPlaneResources faild!\n");
        return NULL;
    }

    for (i=0; i<pr->count_planes; i++) {
        plane = drmModeGetPlane(fd, pr->planes[i]);
        if (!plane) {
            continue;
        }

        plog("[hardy] plane id:%d crtc_id:%d fb_id=%d possible_crtcs:%d\n",
            plane->plane_id, plane->crtc_id, plane->fb_id, plane->possible_crtcs);

        if (plane->crtc_id == crtcid) break;

        drmModeFreePlane(plane);
        plane = NULL;
    }

    if (pr) drmModeFreePlaneResources(pr);

    return plane;
}

static int create_fb(int fd, struct buffer_object *bo)
{
    int ret;
    struct drm_mode_create_dumb creq;

    memset(&creq, 0, sizeof(creq));

    creq.width = bo->width;
    creq.height = bo->height;
    creq.bpp = 32;
    creq.flags = 0;

    ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
    if (ret)
    {
        printf("create dumb failed!\n");
        return -1;
    }

    bo->pitch = creq.pitch;
    bo->bpp = creq.bpp;
    bo->size = creq.size;
    bo->handle = creq.handle;

    //使用缓存的handel创建一个FB，返回fb的id：framebuffer。
#if 0
    ret = drmModeAddFB(fd, bo->width, bo->height, 24, 32, creq.pitch, creq.handle, &bo->fb_id);
#else
    unsigned int handles[4], pitches[4], offsets[4];
    handles[0] = creq.handle;
    pitches[0] = creq.pitch;
    offsets[0] = 0;
    ret = drmModeAddFB2(fd, bo->width, bo->height, DRM_FORMAT_ARGB8888, handles, pitches, offsets,
            &bo->fb_id, 0);
#endif
    if (ret)
    {
        printf("failed to create fb\n");
        return -1;
    }

    plog("create_fb bufid:%d size:%d bpp:%d\n", bo->fb_id, bo->size, bo->bpp);

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
    bo->fb = mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
    if (bo->fb == MAP_FAILED)
    {
        printf("mmap failed!\n");
        return -1;
    }

    return 0;
}

int fbid2buf(int fd, struct buffer_object *bo)
{
    int ret = 0;

    drmModeFBPtr drmfb = drmModeGetFB(fd, bo->fb_id);

    if (NULL == drmfb) {
        printf("drmModeGetFB failed!\n");
        return -1;
    }

    struct drm_mode_map_dumb mreq; //请求映射缓存到内存。
    mreq.handle = drmfb->handle;

    ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
    if (ret)
    {
        printf("map dumb failed!\n");
        return -1;
    }

    bo->width = drmfb->width;
    bo->height = drmfb->height;
    bo->pitch = drmfb->pitch;
    bo->bpp = drmfb->bpp;
    bo->handle = drmfb->handle;
    bo->size = drmfb->width*drmfb->height;
    bo->fb = mmap(0, drmfb->pitch*drmfb->height,
        PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);

    drmModeFreeFB(drmfb);

    return 0;
}

static void destroy_fb(int fd, struct buffer_object *bo)
{
    struct drm_mode_destroy_dumb destroy = {bo->handle};

    drmModeRmFB(fd, bo->fb_id);
    munmap(bo->fb, bo->size);
    drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
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

    //plog("Image info: size %d*%d, bpp %d, compression %d, offset %d\n",
        //bminfo.ciWidth, bminfo.ciHeight, bminfo.ciBitCount, bminfo.ciCompress, fhead.cfoffBits);
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

void storeImage(struct buffer_object *bo, char *name)
{
    unsigned char* buf = bo->fb;
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

    plog("drmfb:[%d %d] bpp:%d\n", bo->width, bo->height, bo->bpp);

    fhead.cfType[0] = 'B', fhead.cfType[1] = 'M';
    bminfo.ciSize = sizeof(BITMAPINFOHEADER);
    bminfo.ciWidth = bo->width;
    bminfo.ciHeight = bo->height;
    bminfo.ciPlanes = 1;

    // write image data
    ptr8 = (unsigned char*) buf;
    switch(bo->bpp)
    {
    case 32:
        bminfo.ciBitCount = 24;
        bminfo.ciSizeImage = bo->width * bo->height * 3;

        bminfo.ciCompress = 0;
        fhead.cfoffBits = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
        fhead.cfSize = fhead.cfoffBits + bo->width * bo->height * 3;
        fseek(fin, fhead.cfoffBits, SEEK_SET);

        for (y=0; y<bo->height; y++)
        {
            for(x=0; x<bo->width; x++)
            {
                //fwrite(ptr8 + ((drmfb->height-y-1)* drmfb->width + x)*4, 1, 3, fin);
                fwrite(ptr8 + ((bo->height-y-1)* bo->width + x)*4+2, 1, 1, fin);
                fwrite(ptr8 + ((bo->height-y-1)* bo->width + x)*4+1, 1, 1, fin);
                fwrite(ptr8 + ((bo->height-y-1)* bo->width + x)*4+0, 1, 1, fin);
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

        bminfo.ciSizeImage = bo->width * bo->height * 2;
        fhead.cfSize = fhead.cfoffBits + bminfo.ciSizeImage;

        //RGB565��ʽ����
        *(int*)rgb_quad = RGB565_MASK_RED;
        *(int*)(rgb_quad+1) = RGB565_MASK_GREEN;
        *(int*)(rgb_quad+2) = RGB565_MASK_BLUE;
        fseek(fin, sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER), SEEK_SET);
        fwrite(&rgb_quad, 1, sizeof(rgb_quad), fin);

        fseek(fin, fhead.cfoffBits, SEEK_SET);
        for (y=0; y<bo->height; y++)
        {
            fwrite(ptr8 + (bo->height-y-1)* bo->width*2, 2, bo->width, fin);
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
    drmModeRes *resources = NULL;

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

    //fd = open("/dev/dri/card0", O_RDWR);
    fd = drmOpen("rockchip", NULL);
    if (fd < 0)
    {
        /* Probably permissions error */
        fprintf(stderr, "couldn't open %s, skipping\n", "");
        return -1;
    }

    resources = drmModeGetResources(fd);
    if (!resources)
    {
        fprintf(stderr, "get res fail\n");
        drmClose(fd);
        return -2;
    }

    drmSetMaster(fd);
    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);

    drmModeConnectorPtr connector = FindConnector(fd, resources);
    drmModeCrtc *crtc = drmFoundCrtc(fd, resources, connector, NULL);
    int crtcid = crtc->crtc_id;

    plog("hardy: connid:%d type:%d typeid:%d encodeid:%d crtcid:%d buf_id:%d\n",
        connector->connector_id, connector->connector_type, connector->connector_type_id, connector->encoder_id, crtcid, crtc->buffer_id);

    struct buffer_object bo = {};

    if(access(bmpfile, F_OK) != 0) {
        bo.fb_id = crtc->buffer_id;
        fbid2buf(fd, &bo);
        plog("connector:%d crtcid:%d bufid:%d\n", connector->connector_id, crtcid, crtc->buffer_id);
        storeImage(&bo, bmpfile);
        goto exit;
    }

    memset(&bo, 0, sizeof(struct buffer_object));
    bo.width = BMP_W; // size of image
    bo.height = BMP_H;
    if (connector->modes[0].hdisplay > BMP_W )
        bo.width =connector->modes[0].hdisplay;
    if (connector->modes[0].vdisplay > BMP_H)
        bo.height =connector->modes[0].vdisplay;
    create_fb(fd, &bo);

    memcpy(&crtc->mode, &connector->modes[0], sizeof(connector->modes[0]));
    drmModeSetCrtc(fd, crtcid, bo.fb_id, 0, 0,
        &connector->connector_id, 1, &connector->modes[0]);

    // primary plane
    //drmModePlanePtr plane = FindPlaneByCrtc(fd, crtcid);
    drmModePlanePtr plane = NULL;
#if SYSWRAPPER
    plane = getVideoPlane();
#else
    if(access("/usr/bin/weston", F_OK) == 0) {
        if (DRM_MODE_CONNECTOR_HDMIA == connector->connector_type
            && 1 == connector->connector_type_id) {
            plane = FindPlaneByName(fd, "Esmart0-win0");
        } else {
            plane = FindPlaneByName(fd, "Smart0-win0");
        }
        plog("vp0 plane id:%d\n", plane->plane_id);
    } else {
        plane = FindPlaneByName(fd, "Esmart0-win0");
        plog("vp2 plane id:%d\n", plane->plane_id);
    }
#endif

    int x=0, y=0;
    if (connector->modes[0].hdisplay > BMP_W) {
        x = (connector->modes[0].hdisplay - BMP_W) / 2;
    }
    if (connector->modes[0].vdisplay > BMP_H) {
        y = (connector->modes[0].vdisplay - BMP_H) / 2;
    }
    plog("display dst[%d %d %d %d] src[%d %d %d %d]\n",
        0, 0, connector->modes[0].hdisplay, connector->modes[0].vdisplay,
        x, y, BMP_W, BMP_H);
    drmModeSetPlane(fd, plane->plane_id, crtcid, bo.fb_id, 0,
        0, 0, connector->modes[0].hdisplay, connector->modes[0].vdisplay,
        x<<16, y<<16, BMP_W<<16, BMP_H<<16);
    drmModeFreePlane(plane);

    memset(bo.fb, 0, bo.size);

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
        drawBmp(bmpfile, bo.fb, bo.pitch, bo.width, bo.height);
    }

    while (1) {
        //drmModePageFlip(fd, crtcid, bo.fb_id, DRM_MODE_PAGE_FLIP_EVENT, 0);
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
            if (0 == drawBmp(tmp, bo.fb, bo.pitch, bo.width, bo.height)) {
                strcpy(bmpfile, tmp);
            }
        }
    }
#endif

    destroy_fb(fd, &bo);
exit:
    if (NULL != crtc) drmModeFreeCrtc(crtc);
    if (NULL != connector) drmModeFreeConnector(connector);
    if (NULL != resources) drmModeFreeResources(resources);
    drmClose(fd);
    exit(0);
}

