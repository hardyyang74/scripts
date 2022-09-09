#define _FILE_OFFSET_BITS 64
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <poll.h>
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

static int BMP_W = 1280;
static int BMP_H = 720;

struct crtc {
    drmModeCrtc *crtc;
    drmModeObjectProperties *props;
    drmModePropertyRes **props_info;
    drmModeModeInfo *mode;
};

struct encoder {
    drmModeEncoder *encoder;
};

struct connector {
    drmModeConnector *connector;
    drmModeObjectProperties *props;
    drmModePropertyRes **props_info;
    char *name;
};

struct fb {
    drmModeFB *fb;
};

struct plane {
    drmModePlane *plane;
    drmModeObjectProperties *props;
    drmModePropertyRes **props_info;
};

struct resources {
    struct crtc *crtcs;
    int count_crtcs;
    struct encoder *encoders;
    int count_encoders;
    struct connector *connectors;
    int count_connectors;
    struct fb *fbs;
    int count_fbs;
    struct plane *planes;
    uint32_t count_planes;
};

struct device {
    int fd;

    struct resources *resources;

    struct {
        unsigned int width;
        unsigned int height;

        unsigned int fb_id;
        struct bo *bo;
        struct bo *cursor_bo;
    } mode;

    int use_atomic;
    drmModeAtomicReq *req;
};

struct property_arg {
    uint32_t obj_id;
    uint32_t obj_type;
    char name[DRM_PROP_NAME_LEN+1];
    uint32_t prop_id;
    uint64_t value;
    bool optional;
};

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
#define RGB565_MASK_GREEN      0x07E0
#define RGB565_MASK_BLUE       0x001F

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

drmModeConnector* FindConnector(struct resources *resources)
{
    if (!resources)
    {
        return NULL;
    }

    drmModeConnector* conn = NULL;
    int i = 0;
    for (i = 0; i < resources->count_connectors; i++)
    {
        conn = resources->connectors[i].connector;
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
        }
    }

    return conn;
}

static drmModeCrtcPtr drmFoundCrtc(struct resources* res,
            drmModeConnector * conn, int *crtc_index)
{
  int i;
  int crtc_id;
  drmModeEncoder *enc;
  drmModeCrtc *crtc;
  uint32_t crtcs_for_connector = 0;

  crtc_id = -1;
  for (i = 0; i < res->count_encoders; i++) {
    enc = res->encoders[i].encoder;
    if (enc) {
      if (enc->encoder_id == conn->encoder_id) {
        crtc_id = enc->crtc_id;
        break;
      }
    }
  }

  /* If no active crtc was found, pick the first possible crtc */
  if (crtc_id == -1) {
    for (i = 0; i < conn->count_encoders; i++) {
      enc = res->encoders[i].encoder;
      crtcs_for_connector |= enc->possible_crtcs;
    }

    if (crtcs_for_connector != 0)
      crtc_id = res->crtcs[ffs (crtcs_for_connector) - 1].crtc->crtc_id;
  }

  if (crtc_id == -1)
    return NULL;

  for (i = 0; i < res->count_crtcs; i++) {
    crtc = res->crtcs[i].crtc;
    if (crtc) {
      if (crtc_id == crtc->crtc_id) {
        if (crtc_index)
          *crtc_index= i;
        return crtc;
      }
    }
  }

  return NULL;
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

drmModePlanePtr FindPlaneByName(struct resources *resources, const char *name)
{
    int i, j;
    drmModePlanePtr plane = NULL;
    drmModeObjectPropertiesPtr props;
    drmModePropertyPtr prop;
    uint32_t found_plane = 0;

    for (i=0; i<resources->count_planes; i++) {
        plane = resources->planes[i].plane;
        if (!plane) {
            continue;
        }

        plog("[hardy] plane id:%d crtc_id:%d fb_id=%d possible_crtcs:%d\n",
            plane->plane_id, plane->crtc_id, plane->fb_id, plane->possible_crtcs);

        //if (plane->crtc_id == crtcid) break;

        props = resources->planes[i].props;
        if (!props) {
            printf("failed to found props plane[%d] %s\n",
                   plane->plane_id, strerror(errno));
            plane = NULL;
            continue;
        }

        for (j = 0; j < props->count_props; j++) {
            prop = resources->planes[i].props_info[j];
            if (!strcmp(prop->name, "NAME")) {
                plog(" %s:%s acquire name:%s\n", prop->name, prop->enums[0].name, name);
            }
            plog(" %s\n", prop->name);
            if (!strcmp(prop->name, "NAME")
                && !strcmp(name, prop->enums[0].name)) {
                found_plane = 1;
                break;
            }
        }

        if (found_plane) break;
        plane = NULL;
    }

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
    bo->width = creq.pitch/(bo->bpp>>3);
    bo->height = bo->size/bo->pitch;

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

    plog("create_fb bufid:%d size:%d bpp:%d pitch:%d\n", bo->fb_id, bo->size, bo->bpp, bo->pitch);

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
        printf("drmModeGetFB failed, fb_id:%d!\n", bo->fb_id);
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

void storeImage2(struct buffer_object *bo, struct buffer_object *ui, char *name)
{
    unsigned char* buf = bo->fb;
    FILE *fin = (FILE*)fopen(name, "w");
    BITMAPFILEHEADER fhead;
    BITMAPINFOHEADER bminfo;
    PIXEL rgb_quad[3];//�����ɫ��
    unsigned char* ptr8;
    unsigned char* ptrui;
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
    ptrui = (unsigned char*)ui->fb;
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
                unsigned char r = *(ptrui + ((bo->height-y-1)* bo->width + x)*4+0);
                unsigned char g = *(ptrui + ((bo->height-y-1)* bo->width + x)*4+1);
                unsigned char b = *(ptrui + ((bo->height-y-1)* bo->width + x)*4+2);

                if ((0!=r) || (0!=g) || (0!=b)) {
                    fwrite(&r, 1, 1, fin);
                    fwrite(&g, 1, 1, fin);
                    fwrite(&b, 1, 1, fin);
                } else {
                    fwrite(ptr8 + ((bo->height-y-1)* bo->width + x)*4+2, 1, 1, fin);
                    fwrite(ptr8 + ((bo->height-y-1)* bo->width + x)*4+1, 1, 1, fin);
                    fwrite(ptr8 + ((bo->height-y-1)* bo->width + x)*4+0, 1, 1, fin);
                }
            }
        }
        break;

    case 24:
        break;

    case 16:
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

drmModePlanePtr getTopPlane(struct resources *res, drmModeConnectorPtr connector)
{
    drmModePlanePtr plane = NULL;

    if(access("/usr/bin/weston", F_OK) == 0) {
        if (DRM_MODE_CONNECTOR_HDMIA == connector->connector_type
            && 1 == connector->connector_type_id) {
            plane = FindPlaneByName(res, "Esmart0-win0");
        } else {
            plane = FindPlaneByName(res, "Smart0-win0");
        }
    } else {
        plane = FindPlaneByName(res, "Esmart0-win0");
    }

    plog("top plane id:%d\n", plane->plane_id);

    return plane;
}

#ifndef SYSWRAPPER
drmModePlanePtr getVideoPlane(struct resources *res, drmModeConnectorPtr connector)
{
    drmModePlanePtr plane = NULL;

    if(access("/usr/bin/weston", F_OK) == 0) {
        if (DRM_MODE_CONNECTOR_HDMIA == connector->connector_type
            && 1 == connector->connector_type_id) {
            plane = FindPlaneByName(res, "Smart0-win0");
        } else {
            plane = FindPlaneByName(res, "Esmart0-win0");
        }
    } else {
        plane = FindPlaneByName(res, "Esmart0-win0");
    }

    plog("video plane id:%d\n", plane->plane_id);

    return plane;
}
#endif

#define free_resource(_res, type, Type)                 \
    do {                                    \
        if (!(_res)->type##s)                       \
            break;                          \
        for (i = 0; i < (int)(_res)->count_##type##s; ++i) {    \
            if (!(_res)->type##s[i].type)               \
                break;                      \
            drmModeFree##Type((_res)->type##s[i].type);     \
        }                               \
        free((_res)->type##s);                      \
    } while (0)

#define free_properties(_res, type)                 \
    do {                                    \
        for (i = 0; i < (int)(_res)->count_##type##s; ++i) {    \
            unsigned int j;                                     \
            for (j = 0; j < res->type##s[i].props->count_props; ++j)\
                drmModeFreeProperty(res->type##s[i].props_info[j]);\
            free(res->type##s[i].props_info);           \
            drmModeFreeObjectProperties(res->type##s[i].props); \
        }                               \
    } while (0)

#define get_resource(_res, __res, type, Type)                   \
    do {                                    \
        for (i = 0; i < (int)(_res)->count_##type##s; ++i) {    \
            uint32_t type##id = (__res)->type##s[i];            \
            (_res)->type##s[i].type =                           \
                drmModeGet##Type(dev->fd, type##id);            \
            if (!(_res)->type##s[i].type)                       \
                fprintf(stderr, "could not get %s %i: %s\n",    \
                    #type, type##id,                            \
                    strerror(errno));           \
        }                               \
    } while (0)

#define get_properties(_res, type, Type)                    \
    do {                                    \
        for (i = 0; i < (int)(_res)->count_##type##s; ++i) {    \
            struct type *obj = &res->type##s[i];            \
            unsigned int j;                     \
            obj->props =                        \
                drmModeObjectGetProperties(dev->fd, obj->type->type##_id, \
                               DRM_MODE_OBJECT_##Type); \
            if (!obj->props) {                  \
                fprintf(stderr,                 \
                    "could not get %s %i properties: %s\n", \
                    #type, obj->type->type##_id,        \
                    strerror(errno));           \
                continue;                   \
            }                           \
            obj->props_info = calloc(obj->props->count_props,   \
                         sizeof(*obj->props_info)); \
            if (!obj->props_info)                   \
                continue;                   \
            for (j = 0; j < obj->props->count_props; ++j)       \
                obj->props_info[j] =                \
                    drmModeGetProperty(dev->fd, obj->props->props[j]); \
        }                               \
    } while (0)

#define find_object(_res, type, Type)                   \
    do {                                    \
        for (i = 0; i < (int)(_res)->count_##type##s; ++i) {    \
            struct type *obj = &(_res)->type##s[i];         \
            if (obj->type->type##_id != p->obj_id)          \
                continue;                   \
            p->obj_type = DRM_MODE_OBJECT_##Type;           \
            obj_type = #Type;                   \
            props = obj->props;                 \
            props_info = obj->props_info;               \
        }                               \
    } while(0)

const char * drmModeGetConnectorTypeName(uint32_t connector_type)
{
    /* Keep the strings in sync with the kernel's drm_connector_enum_list in
     * drm_connector.c. */
    switch (connector_type) {
    case DRM_MODE_CONNECTOR_Unknown:
        return "Unknown";
    case DRM_MODE_CONNECTOR_VGA:
        return "VGA";
    case DRM_MODE_CONNECTOR_DVII:
        return "DVI-I";
    case DRM_MODE_CONNECTOR_DVID:
        return "DVI-D";
    case DRM_MODE_CONNECTOR_DVIA:
        return "DVI-A";
    case DRM_MODE_CONNECTOR_Composite:
        return "Composite";
    case DRM_MODE_CONNECTOR_SVIDEO:
        return "SVIDEO";
    case DRM_MODE_CONNECTOR_LVDS:
        return "LVDS";
    case DRM_MODE_CONNECTOR_Component:
        return "Component";
    case DRM_MODE_CONNECTOR_9PinDIN:
        return "DIN";
    case DRM_MODE_CONNECTOR_DisplayPort:
        return "DP";
    case DRM_MODE_CONNECTOR_HDMIA:
        return "HDMI-A";
    case DRM_MODE_CONNECTOR_HDMIB:
        return "HDMI-B";
    case DRM_MODE_CONNECTOR_TV:
        return "TV";
    case DRM_MODE_CONNECTOR_eDP:
        return "eDP";
    case DRM_MODE_CONNECTOR_VIRTUAL:
        return "Virtual";
    case DRM_MODE_CONNECTOR_DSI:
        return "DSI";
    case DRM_MODE_CONNECTOR_DPI:
        return "DPI";
    case DRM_MODE_CONNECTOR_WRITEBACK:
        return "Writeback";
    default:
        return NULL;
    }
}

static void free_resources(struct resources *res)
{
    int i;

    if (!res)
        return;

    free_properties(res, plane);
    free_resource(res, plane, Plane);

    free_properties(res, connector);
    free_properties(res, crtc);

    for (i = 0; i < res->count_connectors; i++)
        free(res->connectors[i].name);

    free_resource(res, fb, FB);
    free_resource(res, connector, Connector);
    free_resource(res, encoder, Encoder);
    free_resource(res, crtc, Crtc);

    free(res);
}

static struct resources *get_resources(struct device *dev)
{
    drmModeRes *_res;
    drmModePlaneRes *plane_res;
    struct resources *res;
    int i;

    res = calloc(1, sizeof(*res));
    if (res == 0)
        return NULL;

    drmSetClientCap(dev->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

    _res = drmModeGetResources(dev->fd);
    if (!_res) {
        fprintf(stderr, "drmModeGetResources failed: %s\n",
            strerror(errno));
        free(res);
        return NULL;
    }

    res->count_crtcs = _res->count_crtcs;
    res->count_encoders = _res->count_encoders;
    res->count_connectors = _res->count_connectors;
    res->count_fbs = _res->count_fbs;

    res->crtcs = calloc(res->count_crtcs, sizeof(*res->crtcs));
    res->encoders = calloc(res->count_encoders, sizeof(*res->encoders));
    res->connectors = calloc(res->count_connectors, sizeof(*res->connectors));
    res->fbs = calloc(res->count_fbs, sizeof(*res->fbs));

    if (!res->crtcs || !res->encoders || !res->connectors || !res->fbs) {
        drmModeFreeResources(_res);
        goto error;
    }

    get_resource(res, _res, crtc, Crtc);
    get_resource(res, _res, encoder, Encoder);
    get_resource(res, _res, connector, Connector);
    get_resource(res, _res, fb, FB);

    drmModeFreeResources(_res);

    /* Set the name of all connectors based on the type name and the per-type ID. */
    for (i = 0; i < res->count_connectors; i++) {
        struct connector *connector = &res->connectors[i];
        drmModeConnector *conn = connector->connector;
        int num;

        connector->name = malloc(32);
        num = sprintf(connector->name, "%s-%u",
             drmModeGetConnectorTypeName(conn->connector_type),
             conn->connector_type_id);
        if (num < 0)
            goto error;
    }

    get_properties(res, crtc, CRTC);
    get_properties(res, connector, CONNECTOR);

    for (i = 0; i < res->count_crtcs; ++i)
        res->crtcs[i].mode = &res->crtcs[i].crtc->mode;

    plane_res = drmModeGetPlaneResources(dev->fd);
    if (!plane_res) {
        fprintf(stderr, "drmModeGetPlaneResources failed: %s\n",
            strerror(errno));
        return res;
    }

    res->count_planes = plane_res->count_planes;

    res->planes = calloc(res->count_planes, sizeof(*res->planes));
    if (!res->planes) {
        drmModeFreePlaneResources(plane_res);
        goto error;
    }

    get_resource(res, plane_res, plane, Plane);
    drmModeFreePlaneResources(plane_res);
    get_properties(res, plane, PLANE);

    return res;

error:
    free_resources(res);
    return NULL;
}

static bool set_property(struct device *dev, struct property_arg *p)
{
    drmModeObjectProperties *props = NULL;
    drmModePropertyRes **props_info = NULL;
    const char *obj_type;
    int ret;
    int i;

    p->obj_type = 0;
    p->prop_id = 0;

    find_object(dev->resources, crtc, CRTC);
    if (p->obj_type == 0)
        find_object(dev->resources, connector, CONNECTOR);
    if (p->obj_type == 0)
        find_object(dev->resources, plane, PLANE);
    if (p->obj_type == 0) {
        fprintf(stderr, "Object %i not found, can't set property\n",
            p->obj_id);
        return false;
    }

    if (!props) {
        fprintf(stderr, "%s %i has no properties\n",
            obj_type, p->obj_id);
        return false;
    }

    for (i = 0; i < (int)props->count_props; ++i) {
        if (!props_info[i])
            continue;
        if (strcmp(props_info[i]->name, p->name) == 0)
            break;
    }

    if (i == (int)props->count_props) {
        if (!p->optional)
            fprintf(stderr, "%s %i has no %s property\n",
                obj_type, p->obj_id, p->name);
        return false;
    }

    p->prop_id = props->props[i];

    if (!dev->use_atomic)
        ret = drmModeObjectSetProperty(dev->fd, p->obj_id, p->obj_type,
                                       p->prop_id, p->value);
    else
        ret = drmModeAtomicAddProperty(dev->req, p->obj_id, p->prop_id, p->value);

    if (ret < 0)
        fprintf(stderr, "failed to set %s %i property %s to %" PRIu64 ": %s\n",
            obj_type, p->obj_id, p->name, p->value, strerror(errno));

    return true;
}

static void add_property(struct device *dev, uint32_t obj_id,
                   const char *name, uint64_t value)
{
    struct property_arg p;

    p.obj_id = obj_id;
    strcpy(p.name, name);
    p.value = value;

    set_property(dev, &p);
}

static bool add_property_optional(struct device *dev, uint32_t obj_id,
                  const char *name, uint64_t value)
{
    struct property_arg p;

    p.obj_id = obj_id;
    strcpy(p.name, name);
    p.value = value;
    p.optional = true;

    return set_property(dev, &p);
}

static void set_gamma(int fd, unsigned crtc_id)
{
    /* TODO: support 1024-sized LUTs, when the use-case arises */
    int i, ret;

    uint16_t r[1024], g[1024], b[1024];

    for (i = 0; i < 1024; i++) {
        r[i] =
        g[i] =
        b[i] = i<<6;
    }

    ret = drmModeCrtcSetGamma(fd, crtc_id, 1024, r, g, b);
    if (ret)
        fprintf(stderr, "failed to set gamma: %s\n", strerror(errno));
}

int main(int argc, char *argv[])
{
    int ret, fd;
    char bmpfile[256] = {0};
    char tmp[256] = {0};
    FILE *fp = NULL;

    int len;

    struct device dev;
    memset(&dev, 0, sizeof dev);

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
    dev.fd = drmOpen("rockchip", NULL);;
    if (dev.fd < 0)
    {
        /* Probably permissions error */
        fprintf(stderr, "couldn't open %s, skipping\n", "");
        return -1;
    }

    dev.resources = get_resources(&dev);
    if (!dev.resources)
    {
        fprintf(stderr, "get res fail\n");
        drmClose(dev.fd);
        return -2;
    }

    drmSetMaster(dev.fd);
    drmSetClientCap(dev.fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    drmSetClientCap(dev.fd, DRM_CLIENT_CAP_ATOMIC, 1);

    drmModeConnectorPtr connector = FindConnector(dev.resources);
    drmModeCrtc *crtc = drmFoundCrtc(dev.resources, connector, NULL);
    int crtcid = crtc->crtc_id;

    plog("hardy: connid:%d type:%d typeid:%d encodeid:%d crtcid:%d buf_id:%d\n",
        connector->connector_id, connector->connector_type, connector->connector_type_id, connector->encoder_id, crtcid, crtc->buffer_id);

    struct buffer_object bo = {};

    if(access(bmpfile, F_OK) != 0) {
#if SYSWRAPPER
        drmModePlanePtr plane = getVideoPlane();
#else
        drmModePlanePtr plane = getVideoPlane(dev.resources, connector);
#endif
        bo.fb_id = plane->fb_id;
        if (0 == fbid2buf(dev.fd, &bo) ) {
            plog("connector:%d bufid:%d\n", connector->connector_id, bo.fb_id);
        }

        struct buffer_object boui = {};
        boui.fb_id = crtc->buffer_id;
        if ((0 != boui.fb_id) && (0 == fbid2buf(dev.fd, &boui)) ) {
            // overlay ui
            plog("connector:%d crtcid:%d bufid:%d\n", connector->connector_id, crtcid, boui.fb_id);
            storeImage2(&bo, &boui, bmpfile);
        } else {
            storeImage(&bo, bmpfile);
        }
        goto exit;
    } else {
        FILE *fin = (FILE*)fopen(bmpfile, "rb");
        BITMAPFILEHEADER fhead;
        BITMAPINFOHEADER bminfo;

        if(fin != NULL)
        {
            // read bmp header
            ret = fread(&fhead, 1, sizeof(BITMAPFILEHEADER), fin);
            ret = fread(&bminfo, 1, sizeof(BITMAPINFOHEADER), fin);

            BMP_W = bminfo.ciWidth, BMP_H = bminfo.ciHeight;
            fclose(fin);
        }

    }

    memset(&bo, 0, sizeof(struct buffer_object));
    bo.width = BMP_W; // size of image
    bo.height = BMP_H;
    if (connector->modes[0].hdisplay > BMP_W )
        bo.width = connector->modes[0].hdisplay;
    if (connector->modes[0].vdisplay > BMP_H)
        bo.height = connector->modes[0].vdisplay;
    create_fb(dev.fd, &bo);

    memcpy(&crtc->mode, &connector->modes[0], sizeof(connector->modes[0]));
    drmModeSetCrtc(dev.fd, crtcid, bo.fb_id, 0, 0,
        &connector->connector_id, 1, &connector->modes[0]);

    // primary plane
    drmModePlanePtr plane = getTopPlane(dev.resources, connector);

    int x=0, y=0;
    if (bo.width > BMP_W) {
        x = (bo.width - BMP_W) / 2;
    }
    if (bo.height > BMP_H) {
        y = (bo.height - BMP_H) / 2;
    }
    plog("display dst[%d %d %d %d] src[%d %d %d %d]\n",
        0, 0, connector->modes[0].hdisplay, connector->modes[0].vdisplay,
        x, y, BMP_W, BMP_H);
    drmModeSetPlane(dev.fd, plane->plane_id, crtcid, bo.fb_id, 0,
        0, 0, connector->modes[0].hdisplay, connector->modes[0].vdisplay,
        x<<16, y<<16, BMP_W<<16, BMP_H<<16);

    memset(bo.fb, 0, bo.size);

    //set_gamma(dev.fd, crtcid);

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

    destroy_fb(dev.fd, &bo);
exit:
    free_resources(dev.resources);
    drmClose(dev.fd);
    exit(0);
}

