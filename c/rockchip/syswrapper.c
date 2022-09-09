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
#include <fcntl.h>
#include <sys/time.h>
#include <linux/input.h>

#include "common.h"
#include "format.h"
#include "kms.h"
#include "pattern.h"

#include "buffers.h"
#include "syswrapper.h"

#define plog //printf
#define SYS_CONF_FILE "/oem/sys.conf"
#define BOARD_CONF_FILE "/etc/board.conf"

#define __CONNECTOR__   "connector"
#define __ENCODER__         "encoder"
#define __CRTC__                   "crtc"

int encoders = 0, connectors = 0, crtcs = 0, planes = 0, fbs = 0;
int atomic = 0, dump_only;
#define need_resource(type) (!dump_only || type##s)

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
    drmModeRes *res;
    drmModePlaneRes *plane_res;

    struct crtc *crtcs;
    struct encoder *encoders;
    struct connector *connectors;
    struct fb *fbs;
    struct plane *planes;
};

static struct device {
    int fd;

    struct resources *resources;

    struct {
        unsigned int width;
        unsigned int height;

        unsigned int fb_id;
        struct bo *bo;
        struct bo *cursor_bo;
    } mode;
} dev = {-1, NULL,
    {0, 0, 0, NULL, NULL}
};

#define GET_RESOURCE(dev) \
    if (NULL == dev.resources) \
        do { \
            if (-1 == dev.fd) { \
                planes = 1; \
                dev.fd = open("/dev/dri/card0", O_RDWR); \
                drmSetClientCap(dev.fd, DRM_CLIENT_CAP_ATOMIC, 1); \
            } \
            dev.resources = get_resources(&dev); \
        } while(0)

#if 1
static inline int64_t U642I64(uint64_t val)
{
    return (int64_t)*((int64_t *)&val);
}

#define bit_name_fn(res)                    \
const char * res##_str(int type) {              \
    unsigned int i;                     \
    const char *sep = "";                   \
    for (i = 0; i < ARRAY_SIZE(res##_names); i++) {     \
        if (type & (1 << i)) {              \
            printf("%s%s", sep, res##_names[i]);    \
            sep = ", ";             \
        }                       \
    }                           \
    return NULL;                        \
}

static const char *mode_type_names[] = {
    "builtin",
    "clock_c",
    "crtc_c",
    "preferred",
    "default",
    "userdef",
    "driver",
};

static bit_name_fn(mode_type)

static const char *mode_flag_names[] = {
    "phsync",
    "nhsync",
    "pvsync",
    "nvsync",
    "interlace",
    "dblscan",
    "csync",
    "pcsync",
    "ncsync",
    "hskew",
    "bcast",
    "pixmux",
    "dblclk",
    "clkdiv2"
};

static bit_name_fn(mode_flag)

static void dump_fourcc(uint32_t fourcc)
{
    printf(" %c%c%c%c",
        fourcc,
        fourcc >> 8,
        fourcc >> 16,
        fourcc >> 24);
}

static void dump_encoders(struct device *dev)
{
    drmModeEncoder *encoder;
    int i;

    printf("Encoders:\n");
    printf("id\tcrtc\ttype\tpossible crtcs\tpossible clones\t\n");
    for (i = 0; i < dev->resources->res->count_encoders; i++) {
        encoder = dev->resources->encoders[i].encoder;
        if (!encoder)
            continue;

        printf("%d\t%d\t%s\t0x%08x\t0x%08x\n",
               encoder->encoder_id,
               encoder->crtc_id,
               util_lookup_encoder_type_name(encoder->encoder_type),
               encoder->possible_crtcs,
               encoder->possible_clones);
    }
    printf("\n");
}

static void dump_mode(drmModeModeInfo *mode)
{
    printf("  %s %d %d %d %d %d %d %d %d %d %d",
           mode->name,
           mode->vrefresh,
           mode->hdisplay,
           mode->hsync_start,
           mode->hsync_end,
           mode->htotal,
           mode->vdisplay,
           mode->vsync_start,
           mode->vsync_end,
           mode->vtotal,
           mode->clock);

    printf(" flags: ");
    mode_flag_str(mode->flags);
    printf("; type: ");
    mode_type_str(mode->type);
    printf("\n");
}

static void dump_blob(struct device *dev, uint32_t blob_id)
{
    uint32_t i;
    unsigned char *blob_data;
    drmModePropertyBlobPtr blob;

    blob = drmModeGetPropertyBlob(dev->fd, blob_id);
    if (!blob) {
        printf("\n");
        return;
    }

    blob_data = blob->data;

    for (i = 0; i < blob->length; i++) {
        if (i % 16 == 0)
            printf("\n\t\t\t");
        printf("%.2hhx", blob_data[i]);
    }
    printf("\n");

    drmModeFreePropertyBlob(blob);
}

static const char *modifier_to_string(uint64_t modifier)
{
    switch (modifier) {
    case DRM_FORMAT_MOD_INVALID:
        return "INVALID";
    case DRM_FORMAT_MOD_LINEAR:
        return "LINEAR";
    case I915_FORMAT_MOD_X_TILED:
        return "X_TILED";
    case I915_FORMAT_MOD_Y_TILED:
        return "Y_TILED";
    case I915_FORMAT_MOD_Yf_TILED:
        return "Yf_TILED";
    case I915_FORMAT_MOD_Y_TILED_CCS:
        return "Y_TILED_CCS";
    case I915_FORMAT_MOD_Yf_TILED_CCS:
        return "Yf_TILED_CCS";
    case DRM_FORMAT_MOD_SAMSUNG_64_32_TILE:
        return "SAMSUNG_64_32_TILE";
    case DRM_FORMAT_MOD_VIVANTE_TILED:
        return "VIVANTE_TILED";
    case DRM_FORMAT_MOD_VIVANTE_SUPER_TILED:
        return "VIVANTE_SUPER_TILED";
    case DRM_FORMAT_MOD_VIVANTE_SPLIT_TILED:
        return "VIVANTE_SPLIT_TILED";
    case DRM_FORMAT_MOD_VIVANTE_SPLIT_SUPER_TILED:
        return "VIVANTE_SPLIT_SUPER_TILED";
    case NV_FORMAT_MOD_TEGRA_TILED:
        return "MOD_TEGRA_TILED";
    case NV_FORMAT_MOD_TEGRA_16BX2_BLOCK(0):
        return "MOD_TEGRA_16BX2_BLOCK(0)";
    case NV_FORMAT_MOD_TEGRA_16BX2_BLOCK(1):
        return "MOD_TEGRA_16BX2_BLOCK(1)";
    case NV_FORMAT_MOD_TEGRA_16BX2_BLOCK(2):
        return "MOD_TEGRA_16BX2_BLOCK(2)";
    case NV_FORMAT_MOD_TEGRA_16BX2_BLOCK(3):
        return "MOD_TEGRA_16BX2_BLOCK(3)";
    case NV_FORMAT_MOD_TEGRA_16BX2_BLOCK(4):
        return "MOD_TEGRA_16BX2_BLOCK(4)";
    case NV_FORMAT_MOD_TEGRA_16BX2_BLOCK(5):
        return "MOD_TEGRA_16BX2_BLOCK(5)";
    case DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED:
        return "MOD_BROADCOM_VC4_T_TILED";
    default:
        return "(UNKNOWN MODIFIER)";
    }
}

static void dump_in_formats(struct device *dev, uint32_t blob_id)
{
    uint32_t i, j;
    drmModePropertyBlobPtr blob;
    struct drm_format_modifier_blob *header;
    uint32_t *formats;
    struct drm_format_modifier *modifiers;

    printf("\t\tin_formats blob decoded:\n");
    blob = drmModeGetPropertyBlob(dev->fd, blob_id);
    if (!blob) {
        printf("\n");
        return;
    }

    header = blob->data;
    formats = (uint32_t *) ((char *) header + header->formats_offset);
    modifiers = (struct drm_format_modifier *)
        ((char *) header + header->modifiers_offset);

    for (i = 0; i < header->count_formats; i++) {
        printf("\t\t\t");
        dump_fourcc(formats[i]);
        printf(": ");
        for (j = 0; j < header->count_modifiers; j++) {
            uint64_t mask = 1ULL << i;
            if (modifiers[j].formats & mask)
                printf(" %s", modifier_to_string(modifiers[j].modifier));
        }
        printf("\n");
    }

    drmModeFreePropertyBlob(blob);
}

static void dump_prop(struct device *dev, drmModePropertyPtr prop,
              uint32_t prop_id, uint64_t value)
{
    int i;
    printf("\t%d", prop_id);
    if (!prop) {
        printf("\n");
        return;
    }

    printf(" %s:\n", prop->name);

    printf("\t\tflags:");
    if (prop->flags & DRM_MODE_PROP_PENDING)
        printf(" pending");
    if (prop->flags & DRM_MODE_PROP_IMMUTABLE)
        printf(" immutable");
    if (drm_property_type_is(prop, DRM_MODE_PROP_SIGNED_RANGE))
        printf(" signed range");
    if (drm_property_type_is(prop, DRM_MODE_PROP_RANGE))
        printf(" range");
    if (drm_property_type_is(prop, DRM_MODE_PROP_ENUM))
        printf(" enum");
    if (drm_property_type_is(prop, DRM_MODE_PROP_BITMASK))
        printf(" bitmask");
    if (drm_property_type_is(prop, DRM_MODE_PROP_BLOB))
        printf(" blob");
    if (drm_property_type_is(prop, DRM_MODE_PROP_OBJECT))
        printf(" object");
    printf("\n");

    if (drm_property_type_is(prop, DRM_MODE_PROP_SIGNED_RANGE)) {
        printf("\t\tvalues:");
        for (i = 0; i < prop->count_values; i++)
            printf(" %d", U642I64(prop->values[i]));
        printf("\n");
    }

    if (drm_property_type_is(prop, DRM_MODE_PROP_RANGE)) {
        printf("\t\tvalues:");
        for (i = 0; i < prop->count_values; i++)
            printf(" %d", prop->values[i]);
        printf("\n");
    }

    if (drm_property_type_is(prop, DRM_MODE_PROP_ENUM)) {
        printf("\t\tenums:");
        for (i = 0; i < prop->count_enums; i++)
            printf(" %s=%llu", prop->enums[i].name,
                   prop->enums[i].value);
        printf("\n");
    } else if (drm_property_type_is(prop, DRM_MODE_PROP_BITMASK)) {
        printf("\t\tvalues:");
        for (i = 0; i < prop->count_enums; i++)
            printf(" %s=0x%llx", prop->enums[i].name,
                   (1LL << prop->enums[i].value));
        printf("\n");
    } else {
        assert(prop->count_enums == 0);
    }

    if (drm_property_type_is(prop, DRM_MODE_PROP_BLOB)) {
        printf("\t\tblobs:\n");
        for (i = 0; i < prop->count_blobs; i++)
            dump_blob(dev, prop->blob_ids[i]);
        printf("\n");
    } else {
        assert(prop->count_blobs == 0);
    }

    printf("\t\tvalue:");
    if (drm_property_type_is(prop, DRM_MODE_PROP_BLOB))
        dump_blob(dev, value);
    else if (drm_property_type_is(prop, DRM_MODE_PROP_SIGNED_RANGE))
        printf(" %d\n", value);
    else
        printf(" %d\n", value);

    if (strcmp(prop->name, "IN_FORMATS") == 0)
        dump_in_formats(dev, value);
}

static void dump_connectors(struct device *dev)
{
    int i, j;

    printf("Connectors:\n");
    printf("id\tencoder\tstatus\t\tname\t\tsize (mm)\tmodes\tencoders\n");
    for (i = 0; i < dev->resources->res->count_connectors; i++) {
        struct connector *_connector = &dev->resources->connectors[i];
        drmModeConnector *connector = _connector->connector;
        if (!connector)
            continue;

        printf("%d\t%d\t%s\t%-15s\t%dx%d\t\t%d\t",
               connector->connector_id,
               connector->encoder_id,
               util_lookup_connector_status_name(connector->connection),
               _connector->name,
               connector->mmWidth, connector->mmHeight,
               connector->count_modes);

        for (j = 0; j < connector->count_encoders; j++)
            printf("%s%d", j > 0 ? ", " : "", connector->encoders[j]);
        printf("\n");

        if (connector->count_modes) {
            printf("  modes:\n");
            printf("\tname refresh (Hz) hdisp hss hse htot vdisp "
                   "vss vse vtot)\n");
            for (j = 0; j < connector->count_modes; j++)
                dump_mode(&connector->modes[j]);
        }

        if (_connector->props) {
            printf("  props:\n");
            for (j = 0; j < (int)_connector->props->count_props; j++)
                dump_prop(dev, _connector->props_info[j],
                      _connector->props->props[j],
                      _connector->props->prop_values[j]);
        }
    }
    printf("\n");
}

static void dump_crtcs(struct device *dev)
{
    int i;
    uint32_t j;

    printf("CRTCs:\n");
    printf("id\tfb\tpos\tsize\n");
    for (i = 0; i < dev->resources->res->count_crtcs; i++) {
        struct crtc *_crtc = &dev->resources->crtcs[i];
        drmModeCrtc *crtc = _crtc->crtc;
        if (!crtc)
            continue;

        printf("%d\t%d\t(%d,%d)\t(%dx%d)\n",
               crtc->crtc_id,
               crtc->buffer_id,
               crtc->x, crtc->y,
               crtc->width, crtc->height);
        dump_mode(&crtc->mode);

        if (_crtc->props) {
            printf("  props:\n");
            for (j = 0; j < _crtc->props->count_props; j++)
                dump_prop(dev, _crtc->props_info[j],
                      _crtc->props->props[j],
                      _crtc->props->prop_values[j]);
        } else {
            printf("  no properties found\n");
        }
    }
    printf("\n");
}

static void dump_fbs(struct device *dev)
{
    drmModeFB *fb;
    int i;

    printf("Frame buffers:\n");
    printf("id\tsize\tpitch\n");
    for (i = 0; i < dev->resources->res->count_fbs; i++) {
        fb = dev->resources->fbs[i].fb;
        if (!fb)
            continue;

        printf("%u\t(%ux%u)\t%u\n",
               fb->fb_id,
               fb->width, fb->height,
               fb->pitch);
    }
    printf("\n");
}

static void dump_planes(struct device *dev)
{
    unsigned int i, j;

    printf("Planes:\n");
    printf("id\tcrtc\tfb\tCRTC x,y\tx,y\tgamma size\tpossible crtcs\n");

    if (!dev->resources->plane_res)
        return;

    for (i = 0; i < dev->resources->plane_res->count_planes; i++) {
        struct plane *plane = &dev->resources->planes[i];
        drmModePlane *ovr = plane->plane;
        if (!ovr)
            continue;

        printf("%d\t%d\t%d\t%d,%d\t\t%d,%d\t%-8d\t0x%08x\n",
               ovr->plane_id, ovr->crtc_id, ovr->fb_id,
               ovr->crtc_x, ovr->crtc_y, ovr->x, ovr->y,
               ovr->gamma_size, ovr->possible_crtcs);

        if (!ovr->count_formats)
            continue;

        printf("  formats:");
        for (j = 0; j < ovr->count_formats; j++)
            dump_fourcc(ovr->formats[j]);
        printf("\n");

        if (plane->props) {
            printf("  props:\n");
            for (j = 0; j < plane->props->count_props; j++)
                dump_prop(dev, plane->props_info[j],
                      plane->props->props[j],
                      plane->props->prop_values[j]);
        } else {
            printf("  no properties found\n");
        }
    }
    printf("\n");

    return;
}
#endif

static void free_resources(struct resources *res)
{
    int i;

    if (!res)
        return;

#define free_resource(_res, __res, type, Type)                  \
    if (need_resource(type))                        \
    do {                                    \
        if (!(_res)->type##s)                       \
            break;                          \
        for (i = 0; i < (int)(_res)->__res->count_##type##s; ++i) { \
            if (!(_res)->type##s[i].type)               \
                break;                      \
            drmModeFree##Type((_res)->type##s[i].type);     \
        }                               \
        free((_res)->type##s);                      \
    } while (0)

#define free_properties(_res, __res, type)                  \
    if (need_resource(type))                        \
    do {                                    \
        for (i = 0; i < (int)(_res)->__res->count_##type##s; ++i) { \
            drmModeFreeObjectProperties(res->type##s[i].props); \
            free(res->type##s[i].props_info);           \
        }                               \
    } while (0)

    if (res->res) {
        free_properties(res, res, crtc);

        free_resource(res, res, crtc, Crtc);
        free_resource(res, res, encoder, Encoder);

        if (need_resource(connector))
        for (i = 0; i < res->res->count_connectors; i++)
            free(res->connectors[i].name);

        free_resource(res, res, connector, Connector);
        free_resource(res, res, fb, FB);

        drmModeFreeResources(res->res);
    }

    if (res->plane_res) {
        free_properties(res, plane_res, plane);

        free_resource(res, plane_res, plane, Plane);

        drmModeFreePlaneResources(res->plane_res);
    }

    free(res);
}

static struct resources *get_resources(struct device *dev)
{
    struct resources *res;
    int i;

    res = calloc(1, sizeof(*res));
    if (res == 0)
        return NULL;

    if (atomic)
        drmSetClientCap(dev->fd, DRM_CLIENT_CAP_ATOMIC, 1);
    else
        drmSetClientCap(dev->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

    res->res = drmModeGetResources(dev->fd);
    if (!res->res) {
        printf("drmModeGetResources failed: %s\n",
            strerror(errno));
        goto error;
    }

    res->crtcs = calloc(res->res->count_crtcs, sizeof(*res->crtcs));
    res->encoders = calloc(res->res->count_encoders, sizeof(*res->encoders));
    res->connectors = calloc(res->res->count_connectors, sizeof(*res->connectors));
    res->fbs = calloc(res->res->count_fbs, sizeof(*res->fbs));

    if (!res->crtcs || !res->encoders || !res->connectors || !res->fbs)
        goto error;

#define get_resource(_res, __res, type, Type)                   \
    if (need_resource(type))                        \
    do {                                    \
        for (i = 0; i < (int)(_res)->__res->count_##type##s; ++i) { \
            (_res)->type##s[i].type =               \
                drmModeGet##Type(dev->fd, (_res)->__res->type##s[i]); \
            if (!(_res)->type##s[i].type)               \
                printf("could not get %s %i: %s\n", \
                    #type, (_res)->__res->type##s[i],   \
                    strerror(errno));           \
        }                               \
    } while (0)

    get_resource(res, res, crtc, Crtc);
    get_resource(res, res, encoder, Encoder);
    get_resource(res, res, connector, Connector);
    get_resource(res, res, fb, FB);

    /* Set the name of all connectors based on the type name and the per-type ID. */
    if (need_resource(connector))
    for (i = 0; i < res->res->count_connectors; i++) {
        struct connector *connector = &res->connectors[i];
        drmModeConnector *conn = connector->connector;

        asprintf(&connector->name, "%s-%u",
             util_lookup_connector_type_name(conn->connector_type),
             conn->connector_type_id);
    }

#define get_properties(_res, __res, type, Type)                 \
    if (need_resource(type))                        \
    do {                                    \
        for (i = 0; i < (int)(_res)->__res->count_##type##s; ++i) { \
            struct type *obj = &res->type##s[i];            \
            unsigned int j;                     \
            obj->props =                        \
                drmModeObjectGetProperties(dev->fd, obj->type->type##_id, \
                               DRM_MODE_OBJECT_##Type); \
            if (!obj->props) {                  \
                printf("could not get %s %i properties: %s\n", \
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

    get_properties(res, res, crtc, CRTC);
    get_properties(res, res, connector, CONNECTOR);

    if (need_resource(crtc))
    for (i = 0; i < res->res->count_crtcs; ++i)
        res->crtcs[i].mode = &res->crtcs[i].crtc->mode;

    res->plane_res = drmModeGetPlaneResources(dev->fd);
    if (!res->plane_res) {
        printf("drmModeGetPlaneResources failed: %s\n",
            strerror(errno));
        return res;
    }

    res->planes = calloc(res->plane_res->count_planes, sizeof(*res->planes));
    if (!res->planes)
        goto error;

    get_resource(res, plane_res, plane, Plane);
    get_properties(res, plane_res, plane, PLANE);

    return res;

error:
    free_resources(res);
    return NULL;
}

static int get_crtc_index(struct device *dev, uint32_t id)
{
    int i;

    for (i = 0; i < dev->resources->res->count_crtcs; ++i) {
        drmModeCrtc *crtc = dev->resources->crtcs[i].crtc;
        if (crtc && crtc->crtc_id == id)
            return i;
    }

    return -1;
}

static drmModeConnector *get_connector_by_name(struct device *dev, const char *name)
{
    struct connector *connector;
    int i;

    for (i = 0; i < dev->resources->res->count_connectors; i++) {
        connector = &dev->resources->connectors[i];

        if (strcmp(connector->name, name) == 0)
            return connector->connector;
    }

    return NULL;
}

static drmModeConnector *get_connector_by_id(struct device *dev, uint32_t id)
{
    drmModeConnector *connector;
    int i;

    for (i = 0; i < dev->resources->res->count_connectors; i++) {
        connector = dev->resources->connectors[i].connector;
        if (connector && connector->connector_id == id)
            return connector;
    }

    return NULL;
}

static drmModeEncoder *get_encoder_by_id(struct device *dev, uint32_t id)
{
    drmModeEncoder *encoder;
    int i;

    for (i = 0; i < dev->resources->res->count_encoders; i++) {
        encoder = dev->resources->encoders[i].encoder;
        if (encoder && encoder->encoder_id == id)
            return encoder;
    }

    return NULL;
}

/* -----------------------------------------------------------------------------
 * Pipes and planes
 */

/*
 * Mode setting with the kernel interfaces is a bit of a chore.
 * First you have to find the connector in question and make sure the
 * requested mode is available.
 * Then you need to find the encoder attached to that connector so you
 * can bind it with a free crtc.
 */
struct pipe_arg {
    const char **cons;
    uint32_t *con_ids;
    unsigned int num_cons;
    uint32_t crtc_id;
    char mode_str[64];
    char format_str[5];
    unsigned int vrefresh;
    unsigned int fourcc;
    drmModeModeInfo *mode;
    struct crtc *crtc;
    unsigned int fb_id[2], current_fb_id;
    struct timeval start;

    int swap_count;
};

struct plane_arg {
    uint32_t plane_id;  /* the id of plane to use */
    uint32_t crtc_id;  /* the id of CRTC to bind to */
    bool has_position;
    int32_t x, y;
    uint32_t w, h;
    double scale;
    unsigned int fb_id;
    struct bo *bo;
    char format_str[5]; /* need to leave room for terminating \0 */
    unsigned int fourcc;
};

static drmModeModeInfo *
connector_find_mode(struct device *dev, uint32_t con_id, const char *mode_str,
        const unsigned int vrefresh)
{
    drmModeConnector *connector;
    drmModeModeInfo *mode;
    int i;

    connector = get_connector_by_id(dev, con_id);
    if (!connector || !connector->count_modes)
        return NULL;

    for (i = 0; i < connector->count_modes; i++) {
        mode = &connector->modes[i];
        if (!strcmp(mode->name, mode_str)) {
            /* If the vertical refresh frequency is not specified then return the
             * first mode that match with the name. Else, return the mode that match
             * the name and the specified vertical refresh frequency.
             */
            if (vrefresh == 0)
                return mode;
            else if (mode->vrefresh == vrefresh)
                return mode;
        }
    }

    return NULL;
}

static struct crtc *pipe_find_crtc(struct device *dev, struct pipe_arg *pipe)
{
    uint32_t possible_crtcs = ~0;
    uint32_t active_crtcs = 0;
    unsigned int crtc_idx;
    unsigned int i;
    int j;

    for (i = 0; i < pipe->num_cons; ++i) {
        uint32_t crtcs_for_connector = 0;
        drmModeConnector *connector;
        drmModeEncoder *encoder;
        int idx;

        connector = get_connector_by_id(dev, pipe->con_ids[i]);
        if (!connector)
            return NULL;

        for (j = 0; j < connector->count_encoders; ++j) {
            encoder = get_encoder_by_id(dev, connector->encoders[j]);
            if (!encoder)
                continue;

            crtcs_for_connector |= encoder->possible_crtcs;

            idx = get_crtc_index(dev, encoder->crtc_id);
            if (idx >= 0)
                active_crtcs |= 1 << idx;
        }

        possible_crtcs &= crtcs_for_connector;
    }

    if (!possible_crtcs)
        return NULL;

    /* Return the first possible and active CRTC if one exists, or the first
     * possible CRTC otherwise.
     */
    if (possible_crtcs & active_crtcs)
        crtc_idx = ffs(possible_crtcs & active_crtcs);
    else
        crtc_idx = ffs(possible_crtcs);

    return &dev->resources->crtcs[crtc_idx - 1];
}

static int pipe_find_crtc_and_mode(struct device *dev, struct pipe_arg *pipe)
{
    drmModeModeInfo *mode = NULL;
    int i;

    pipe->mode = NULL;

    for (i = 0; i < (int)pipe->num_cons; i++) {
        mode = connector_find_mode(dev, pipe->con_ids[i],
                       pipe->mode_str, pipe->vrefresh);
        if (mode == NULL) {
            printf(
                "failed to find mode \"%s\" for connector %s\n",
                pipe->mode_str, pipe->cons[i]);
            return -EINVAL;
        }
    }

    /* If the CRTC ID was specified, get the corresponding CRTC. Otherwise
     * locate a CRTC that can be attached to all the connectors.
     */
    if (pipe->crtc_id != (uint32_t)-1) {
        for (i = 0; i < dev->resources->res->count_crtcs; i++) {
            struct crtc *crtc = &dev->resources->crtcs[i];

            if (pipe->crtc_id == crtc->crtc->crtc_id) {
                pipe->crtc = crtc;
                break;
            }
        }
    } else {
        pipe->crtc = pipe_find_crtc(dev, pipe);
    }

    if (!pipe->crtc) {
        printf( "failed to find CRTC for pipe\n");
        return -EINVAL;
    }

    pipe->mode = mode;
    pipe->crtc->mode = mode;

    return 0;
}

/* -----------------------------------------------------------------------------
 * Properties
 */

struct property_arg {
    uint32_t obj_id;
    uint32_t obj_type;
    char name[DRM_PROP_NAME_LEN+1];
    uint32_t prop_id;
    uint64_t value;
};

static void set_property(struct device *dev, struct property_arg *p)
{
    drmModeObjectProperties *props = NULL;
    drmModePropertyRes **props_info = NULL;
    const char *obj_type;
    int ret;
    int i;

    p->obj_type = 0;
    p->prop_id = 0;

#define find_object(_res, __res, type, Type)                    \
    do {                                    \
        for (i = 0; i < (int)(_res)->__res->count_##type##s; ++i) { \
            struct type *obj = &(_res)->type##s[i];         \
            if (obj->type->type##_id != p->obj_id)          \
                continue;                   \
            p->obj_type = DRM_MODE_OBJECT_##Type;           \
            obj_type = #Type;                   \
            props = obj->props;                 \
            props_info = obj->props_info;               \
        }                               \
    } while(0)                              \

    find_object(dev->resources, res, crtc, CRTC);
    if (p->obj_type == 0)
        find_object(dev->resources, res, connector, CONNECTOR);
    if (p->obj_type == 0)
        find_object(dev->resources, plane_res, plane, PLANE);
    if (p->obj_type == 0) {
        printf( "Object %i not found, can't set property\n",
            p->obj_id);
            return;
    }

    if (!props) {
        printf( "%s %i has no properties\n",
            obj_type, p->obj_id);
        return;
    }

    for (i = 0; i < (int)props->count_props; ++i) {
        if (!props_info[i])
            continue;
        if (strcmp(props_info[i]->name, p->name) == 0)
            break;
    }

    if (i == (int)props->count_props) {
        printf( "%s %i has no %s property\n",
            obj_type, p->obj_id, p->name);
        return;
    }

    p->prop_id = props->props[i];

    ret = drmModeObjectSetProperty(dev->fd, p->obj_id, p->obj_type,
                       p->prop_id, p->value);
    if (ret < 0)
        printf( "failed to set %s %i property %s to %" PRIu64 ": %s\n",
            obj_type, p->obj_id, p->name, p->value, strerror(errno));
}

/* -------------------------------------------------------------------------- */

static void
page_flip_handler(int fd, unsigned int frame,
          unsigned int sec, unsigned int usec, void *data)
{
    struct pipe_arg *pipe;
    unsigned int new_fb_id;
    struct timeval end;
    double t;

    pipe = data;
    if (pipe->current_fb_id == pipe->fb_id[0])
        new_fb_id = pipe->fb_id[1];
    else
        new_fb_id = pipe->fb_id[0];

    drmModePageFlip(fd, pipe->crtc->crtc->crtc_id, new_fb_id,
            DRM_MODE_PAGE_FLIP_EVENT, pipe);
    pipe->current_fb_id = new_fb_id;
    pipe->swap_count++;
    if (pipe->swap_count == 60) {
        gettimeofday(&end, NULL);
        t = end.tv_sec + end.tv_usec * 1e-6 -
            (pipe->start.tv_sec + pipe->start.tv_usec * 1e-6);
        printf( "freq: %.02fHz\n", pipe->swap_count / t);
        pipe->swap_count = 0;
        pipe->start = end;
    }
}

static bool format_support(const drmModePlanePtr ovr, uint32_t fmt)
{
    unsigned int i;

    for (i = 0; i < ovr->count_formats; ++i) {
        if (ovr->formats[i] == fmt)
            return true;
    }

    return false;
}

static int set_plane(struct device *dev, struct plane_arg *p)
{
    drmModePlane *ovr;
    uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};
    uint32_t plane_id;
    struct bo *plane_bo;
    uint32_t plane_flags = 0;
    int crtc_x, crtc_y, crtc_w, crtc_h;
    struct crtc *crtc = NULL;
    unsigned int pipe;
    unsigned int i;

    /* Find an unused plane which can be connected to our CRTC. Find the
     * CRTC index first, then iterate over available planes.
     */
    for (i = 0; i < (unsigned int)dev->resources->res->count_crtcs; i++) {
        if (p->crtc_id == dev->resources->res->crtcs[i]) {
            crtc = &dev->resources->crtcs[i];
            pipe = i;
            break;
        }
    }

    if (!crtc) {
        printf( "CRTC %u not found\n", p->crtc_id);
        return -1;
    }

    plane_id = p->plane_id;

    for (i = 0; i < dev->resources->plane_res->count_planes; i++) {
        ovr = dev->resources->planes[i].plane;
        if (!ovr)
            continue;

        if (plane_id && plane_id != ovr->plane_id)
            continue;

        if (!format_support(ovr, p->fourcc))
            continue;

        if ((ovr->possible_crtcs & (1 << pipe)) &&
            (ovr->crtc_id == 0 || ovr->crtc_id == p->crtc_id)) {
            plane_id = ovr->plane_id;
            break;
        }
    }

    if (i == dev->resources->plane_res->count_planes) {
        printf( "no unused plane available for CRTC %u\n",
            crtc->crtc->crtc_id);
        return -1;
    }

    printf( "testing %dx%d@%s overlay plane %u\n",
        p->w, p->h, p->format_str, plane_id);

    plane_bo = bo_create(dev->fd, p->fourcc, p->w, p->h, handles,
                 pitches, offsets, UTIL_PATTERN_TILES);
    if (plane_bo == NULL)
        return -1;

    p->bo = plane_bo;

    /* just use single plane format for now.. */
    if (drmModeAddFB2(dev->fd, p->w, p->h, p->fourcc,
            handles, pitches, offsets, &p->fb_id, plane_flags)) {
        printf( "failed to add fb: %s\n", strerror(errno));
        return -1;
    }

    crtc_w = p->w * p->scale;
    crtc_h = p->h * p->scale;
    if (!p->has_position) {
        /* Default to the middle of the screen */
        crtc_x = (crtc->mode->hdisplay - crtc_w) / 2;
        crtc_y = (crtc->mode->vdisplay - crtc_h) / 2;
    } else {
        crtc_x = p->x;
        crtc_y = p->y;
    }

    /* note src coords (last 4 args) are in Q16 format */
    if (drmModeSetPlane(dev->fd, plane_id, crtc->crtc->crtc_id, p->fb_id,
                plane_flags, crtc_x, crtc_y, crtc_w, crtc_h,
                0, 0, p->w << 16, p->h << 16)) {
        printf( "failed to enable plane: %s\n",
            strerror(errno));
        return -1;
    }

    ovr->crtc_id = crtc->crtc->crtc_id;

    return 0;
}

static void clear_planes(struct device *dev, struct plane_arg *p, unsigned int count)
{
    unsigned int i;

    for (i = 0; i < count; i++) {
        if (p[i].fb_id)
            drmModeRmFB(dev->fd, p[i].fb_id);
        if (p[i].bo)
            bo_destroy(p[i].bo);
    }
}


static void set_mode(struct device *dev, struct pipe_arg *pipes, unsigned int count)
{
    uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};
    unsigned int fb_id;
    struct bo *bo;
    unsigned int i;
    unsigned int j;
    int ret, x;

    dev->mode.width = 0;
    dev->mode.height = 0;
    dev->mode.fb_id = 0;

    for (i = 0; i < count; i++) {
        struct pipe_arg *pipe = &pipes[i];

        ret = pipe_find_crtc_and_mode(dev, pipe);
        if (ret < 0)
            continue;

        dev->mode.width += pipe->mode->hdisplay;
        if (dev->mode.height < pipe->mode->vdisplay)
            dev->mode.height = pipe->mode->vdisplay;
    }

    bo = bo_create(dev->fd, pipes[0].fourcc, dev->mode.width,
               dev->mode.height, handles, pitches, offsets,
               UTIL_PATTERN_SMPTE);
    if (bo == NULL)
        return;

    dev->mode.bo = bo;

    ret = drmModeAddFB2(dev->fd, dev->mode.width, dev->mode.height,
                pipes[0].fourcc, handles, pitches, offsets, &fb_id, 0);
    if (ret) {
        printf( "failed to add fb (%ux%u): %s\n",
            dev->mode.width, dev->mode.height, strerror(errno));
        return;
    }

    dev->mode.fb_id = fb_id;

    x = 0;
    for (i = 0; i < count; i++) {
        struct pipe_arg *pipe = &pipes[i];

        if (pipe->mode == NULL)
            continue;

        printf("setting mode %s-%dHz@%s on connectors ",
               pipe->mode_str, pipe->mode->vrefresh, pipe->format_str);
        for (j = 0; j < pipe->num_cons; ++j)
            printf("%s, ", pipe->cons[j]);
        printf("crtc %d\n", pipe->crtc->crtc->crtc_id);

        ret = drmModeSetCrtc(dev->fd, pipe->crtc->crtc->crtc_id, fb_id,
                     x, 0, pipe->con_ids, pipe->num_cons,
                     pipe->mode);

        /* XXX: Actually check if this is needed */
        drmModeDirtyFB(dev->fd, fb_id, NULL, 0);

        x += pipe->mode->hdisplay;

        if (ret) {
            printf( "failed to set mode: %s\n", strerror(errno));
            return;
        }
    }
}

static void clear_mode(struct device *dev)
{
    if (dev->mode.fb_id)
        drmModeRmFB(dev->fd, dev->mode.fb_id);
    if (dev->mode.bo)
        bo_destroy(dev->mode.bo);
}

static void set_planes(struct device *dev, struct plane_arg *p, unsigned int count)
{
    unsigned int i;

    /* set up planes/overlays */
    for (i = 0; i < count; i++)
        if (set_plane(dev, &p[i]))
            return;
}

#if 0
static void set_cursors(struct device *dev, struct pipe_arg *pipes, unsigned int count)
{
    uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};
    struct bo *bo;
    unsigned int i;
    int ret;

    /* maybe make cursor width/height configurable some day */
    uint32_t cw = 64;
    uint32_t ch = 64;

    /* create cursor bo.. just using PATTERN_PLAIN as it has
     * translucent alpha
     */
    bo = bo_create(dev->fd, DRM_FORMAT_ARGB8888, cw, ch, handles, pitches,
               offsets, UTIL_PATTERN_PLAIN);
    if (bo == NULL)
        return;

    dev->mode.cursor_bo = bo;

    for (i = 0; i < count; i++) {
        struct pipe_arg *pipe = &pipes[i];
        ret = cursor_init(dev->fd, handles[0],
                pipe->crtc->crtc->crtc_id,
                pipe->mode->hdisplay, pipe->mode->vdisplay,
                cw, ch);
        if (ret) {
            printf( "failed to init cursor for CRTC[%u]\n",
                    pipe->crtc_id);
            return;
        }
    }

    cursor_start();
}

static void clear_cursors(struct device *dev)
{
    cursor_stop();

    if (dev->mode.cursor_bo)
        bo_destroy(dev->mode.cursor_bo);
}
#endif
static void test_page_flip(struct device *dev, struct pipe_arg *pipes, unsigned int count)
{
    uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};
    unsigned int other_fb_id;
    struct bo *other_bo;
    drmEventContext evctx;
    unsigned int i;
    int ret;

    other_bo = bo_create(dev->fd, pipes[0].fourcc, dev->mode.width,
                 dev->mode.height, handles, pitches, offsets,
                 UTIL_PATTERN_PLAIN);
    if (other_bo == NULL)
        return;

    ret = drmModeAddFB2(dev->fd, dev->mode.width, dev->mode.height,
                pipes[0].fourcc, handles, pitches, offsets,
                &other_fb_id, 0);
    if (ret) {
        printf( "failed to add fb: %s\n", strerror(errno));
        goto err;
    }

    for (i = 0; i < count; i++) {
        struct pipe_arg *pipe = &pipes[i];

        if (pipe->mode == NULL)
            continue;

        ret = drmModePageFlip(dev->fd, pipe->crtc->crtc->crtc_id,
                      other_fb_id, DRM_MODE_PAGE_FLIP_EVENT,
                      pipe);
        if (ret) {
            printf( "failed to page flip: %s\n", strerror(errno));
            goto err_rmfb;
        }
        gettimeofday(&pipe->start, NULL);
        pipe->swap_count = 0;
        pipe->fb_id[0] = dev->mode.fb_id;
        pipe->fb_id[1] = other_fb_id;
        pipe->current_fb_id = other_fb_id;
    }

    memset(&evctx, 0, sizeof evctx);
    evctx.version = DRM_EVENT_CONTEXT_VERSION;
    evctx.vblank_handler = NULL;
    evctx.page_flip_handler = page_flip_handler;

    while (1) {
#if 0
        struct pollfd pfd[2];

        pfd[0].fd = 0;
        pfd[0].events = POLLIN;
        pfd[1].fd = fd;
        pfd[1].events = POLLIN;

        if (poll(pfd, 2, -1) < 0) {
            printf( "poll error\n");
            break;
        }

        if (pfd[0].revents)
            break;
#else
        struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };
        fd_set fds;

        FD_ZERO(&fds);
        FD_SET(0, &fds);
        FD_SET(dev->fd, &fds);
        ret = select(dev->fd + 1, &fds, NULL, NULL, &timeout);

        if (ret <= 0) {
            printf( "select timed out or error (ret %d)\n",
                ret);
            continue;
        } else if (FD_ISSET(0, &fds)) {
            break;
        }
#endif

        drmHandleEvent(dev->fd, &evctx);
    }

err_rmfb:
    drmModeRmFB(dev->fd, other_fb_id);
err:
    bo_destroy(other_bo);
}

#define min(a, b)   ((a) < (b) ? (a) : (b))

static int parse_connector(struct pipe_arg *pipe, const char *arg)
{
    unsigned int len;
    unsigned int i;
    const char *p;
    char *endp;

    pipe->vrefresh = 0;
    pipe->crtc_id = (uint32_t)-1;
    strcpy(pipe->format_str, "XR24");

    /* Count the number of connectors and allocate them. */
    pipe->num_cons = 1;
    for (p = arg; *p && *p != ':' && *p != '@'; ++p) {
        if (*p == ',')
            pipe->num_cons++;
    }

    pipe->con_ids = calloc(pipe->num_cons, sizeof(*pipe->con_ids));
    pipe->cons = calloc(pipe->num_cons, sizeof(*pipe->cons));
    if (pipe->con_ids == NULL || pipe->cons == NULL)
        return -1;

    /* Parse the connectors. */
    for (i = 0, p = arg; i < pipe->num_cons; ++i, p = endp + 1) {
        endp = strpbrk(p, ",@:");
        if (!endp)
            break;

        pipe->cons[i] = strndup(p, endp - p);

        if (*endp != ',')
            break;
    }

    if (i != pipe->num_cons - 1)
        return -1;

    /* Parse the remaining parameters. */
    if (*endp == '@') {
        arg = endp + 1;
        pipe->crtc_id = strtoul(arg, &endp, 10);
    }
    if (*endp != ':')
        return -1;

    arg = endp + 1;

    /* Search for the vertical refresh or the format. */
    p = strpbrk(arg, "-@");
    if (p == NULL)
        p = arg + strlen(arg);
    len = min(sizeof pipe->mode_str - 1, (unsigned int)(p - arg));
    strncpy(pipe->mode_str, arg, len);
    pipe->mode_str[len] = '\0';

    if (*p == '-') {
        pipe->vrefresh = strtoul(p + 1, &endp, 10);
        p = endp;
    }

    if (*p == '@') {
        strncpy(pipe->format_str, p + 1, 4);
        pipe->format_str[4] = '\0';
    }

    pipe->fourcc = util_format_fourcc(pipe->format_str);
    if (pipe->fourcc == 0)  {
        printf( "unknown format %s\n", pipe->format_str);
        return -1;
    }

    return 0;
}

static int parse_plane(struct plane_arg *plane, const char *p)
{
    char *end;

    plane->plane_id = strtoul(p, &end, 10);
    if (*end != '@')
        return -EINVAL;

    p = end + 1;
    plane->crtc_id = strtoul(p, &end, 10);
    if (*end != ':')
        return -EINVAL;

    p = end + 1;
    plane->w = strtoul(p, &end, 10);
    if (*end != 'x')
        return -EINVAL;

    p = end + 1;
    plane->h = strtoul(p, &end, 10);

    if (*end == '+' || *end == '-') {
        plane->x = strtol(end, &end, 10);
        if (*end != '+' && *end != '-')
            return -EINVAL;
        plane->y = strtol(end, &end, 10);

        plane->has_position = true;
    }

    if (*end == '*') {
        p = end + 1;
        plane->scale = strtod(p, &end);
        if (plane->scale <= 0.0)
            return -EINVAL;
    } else {
        plane->scale = 1.0;
    }

    if (*end == '@') {
        p = end + 1;
        if (strlen(p) != 4)
            return -EINVAL;

        strcpy(plane->format_str, p);
    } else {
        strcpy(plane->format_str, "XR24");
    }

    plane->fourcc = util_format_fourcc(plane->format_str);
    if (plane->fourcc == 0) {
        printf( "unknown format %s\n", plane->format_str);
        return -EINVAL;
    }

    return 0;
}

static int parse_property(struct property_arg *p, const char *arg)
{
    if (sscanf(arg, "%d:%32[^:]:%" SCNu64, &p->obj_id, p->name, &p->value) != 3)
        return -1;

    p->obj_type = 0;
    p->name[DRM_PROP_NAME_LEN] = '\0';

    return 0;
}
#if 0
static void usage(char *name)
{
    printf( "usage: %s [-cDdefMPpsCvw]\n", name);

    printf( "\n Query options:\n\n");
    printf( "\t-c\tlist connectors\n");
    printf( "\t-e\tlist encoders\n");
    printf( "\t-f\tlist framebuffers\n");
    printf( "\t-p\tlist CRTCs and planes (pipes)\n");

    printf( "\n Test options:\n\n");
    printf( "\t-P <plane_id>@<crtc_id>:<w>x<h>[+<x>+<y>][*<scale>][@<format>]\tset a plane\n");
    printf( "\t-s <connector_id>[,<connector_id>][@<crtc_id>]:<mode>[-<vrefresh>][@<format>]\tset a mode\n");
    printf( "\t-C\ttest hw cursor\n");
    printf( "\t-v\ttest vsynced page flipping\n");
    printf( "\t-w <obj_id>:<prop_name>:<value>\tset property\n");

    printf( "\n Generic options:\n\n");
    printf( "\t-a\tenable cap atomic\n");
    printf( "\t-d\tdrop master after mode set\n");
    printf( "\t-M module\tuse the given driver\n");
    printf( "\t-D device\tuse the given device\n");

    printf( "\n\tDefault is to dump all info.\n");
    exit(0);
}
#endif
static int page_flipping_supported(void)
{
    /*FIXME: generic ioctl needed? */
    return 1;
#if 0
    int ret, value;
    struct drm_i915_getparam gp;

    gp.param = I915_PARAM_HAS_PAGEFLIPPING;
    gp.value = &value;

    ret = drmCommandWriteRead(fd, DRM_I915_GETPARAM, &gp, sizeof(gp));
    if (ret) {
        printf( "drm_i915_getparam: %m\n");
        return 0;
    }

    return *gp.value;
#endif
}

static int cursor_supported(void)
{
    /*FIXME: generic ioctl needed? */
    return 1;
}

static int pipe_resolve_connectors(struct device *dev, struct pipe_arg *pipe)
{
    drmModeConnector *connector;
    unsigned int i;
    uint32_t id;
    char *endp;

    for (i = 0; i < pipe->num_cons; i++) {
        id = strtoul(pipe->cons[i], &endp, 10);
        if (endp == pipe->cons[i]) {
            connector = get_connector_by_name(dev, pipe->cons[i]);
            if (!connector) {
                printf( "no connector named '%s'\n",
                    pipe->cons[i]);
                return -ENODEV;
            }

            id = connector->connector_id;
        }

        pipe->con_ids[i] = id;
    }

    return 0;
}

#if 0
static char optstr[] = "acdD:efM:P:ps:Cvw:";

static int execCmd(int argc, char **argv)
{
    struct device dev;

    int c;
    int drop_master = 0;
    int test_vsync = 0;
    int test_cursor = 0;
    char *device = NULL;
    char *module = NULL;
    unsigned int i;
    unsigned int count = 0, plane_count = 0;
    unsigned int prop_count = 0;
    struct pipe_arg *pipe_args = NULL;
    struct plane_arg *plane_args = NULL;
    struct property_arg *prop_args = NULL;
    unsigned int args = 0;
    int ret;

    memset(&dev, 0, sizeof dev);

    while ((c = getopt(argc, argv, optstr)) != -1) {
        args++;

        switch (c) {
        case 'a':
            atomic = 1;
            args--;
            break;
        case 'c':
            connectors = 1;
            break;
        case 'D':
//          device = optarg;
            args--;
            break;
        case 'd':
            drop_master = 1;
            break;
        case 'e':
            encoders = 1;
            break;
        case 'f':
            fbs = 1;
            break;
        case 'M':
//          module = optarg;
            /* Preserve the default behaviour of dumping all information. */
            args--;
            break;
        case 'P':
            plane_args = realloc(plane_args,
                         (plane_count + 1) * sizeof *plane_args);
            if (plane_args == NULL) {
                printf( "memory allocation failed\n");
                return 1;
            }
            memset(&plane_args[plane_count], 0, sizeof(*plane_args));

//          if (parse_plane(&plane_args[plane_count], optarg) < 0)
    //          usage(argv[0]);

            plane_count++;
            break;
        case 'p':
            crtcs = 1;
            planes = 1;
            break;
        case 's':
            pipe_args = realloc(pipe_args,
                        (count + 1) * sizeof *pipe_args);
            if (pipe_args == NULL) {
                printf( "memory allocation failed\n");
                return 1;
            }
            memset(&pipe_args[count], 0, sizeof(*pipe_args));

//          if (parse_connector(&pipe_args[count], optarg) < 0)
    //          usage(argv[0]);

            count++;
            break;
        case 'C':
            test_cursor = 1;
            break;
        case 'v':
            test_vsync = 1;
            break;
        case 'w':
            prop_args = realloc(prop_args,
                       (prop_count + 1) * sizeof *prop_args);
            if (prop_args == NULL) {
                printf( "memory allocation failed\n");
                return 1;
            }
            memset(&prop_args[prop_count], 0, sizeof(*prop_args));

//          if (parse_property(&prop_args[prop_count], optarg) < 0)
    //          usage(argv[0]);

            prop_count++;
            break;
        default:
            //usage(argv[0]);
            break;
        }
    }

    if (!args)
        encoders = connectors = crtcs = planes = fbs = 1;

    dump_only = !count && !plane_count && !prop_count;

    if (dump_only && !device)
        dev.fd = open("/dev/dri/card0", O_RDWR);
    else
        dev.fd = util_open(device, module);
    if (dev.fd < 0)
        return -1;

    if (test_vsync && !page_flipping_supported()) {
        printf( "page flipping not supported by drm.\n");
        return -1;
    }

    if (test_vsync && !count) {
        printf( "page flipping requires at least one -s option.\n");
        return -1;
    }

    if (test_cursor && !cursor_supported()) {
        printf( "hw cursor not supported by drm.\n");
        return -1;
    }

    dev.resources = get_resources(&dev);
    if (!dev.resources) {
        drmClose(dev.fd);
        return 1;
    }

    for (i = 0; i < count; i++) {
        if (pipe_resolve_connectors(&dev, &pipe_args[i]) < 0) {
            free_resources(dev.resources);
            drmClose(dev.fd);
            return 1;
        }
    }

#define dump_resource(dev, res) if (res) dump_##res(dev)

#if 1
    dump_resource(&dev, encoders);
    dump_resource(&dev, connectors);
    dump_resource(&dev, crtcs);
    dump_resource(&dev, planes);
    dump_resource(&dev, fbs);
#else

dump_resource(&dev, connectors);

#endif
    for (i = 0; i < prop_count; ++i)
        set_property(&dev, &prop_args[i]);

    if (count || plane_count) {
        uint64_t cap = 0;

        ret = drmGetCap(dev.fd, DRM_CAP_DUMB_BUFFER, &cap);
        if (ret || cap == 0) {
            printf( "driver doesn't support the dumb buffer API\n");
            return 1;
        }

        if (count)
            set_mode(&dev, pipe_args, count);

        if (plane_count)
            set_planes(&dev, plane_args, plane_count);
#if 0
        if (test_cursor)
            set_cursors(&dev, pipe_args, count);
#endif
        if (test_vsync)
            test_page_flip(&dev, pipe_args, count);

        if (drop_master)
            drmDropMaster(dev.fd);

        getchar();
#if 0
        if (test_cursor)
            clear_cursors(&dev);
#endif
        if (plane_count)
            clear_planes(&dev, plane_args, plane_count);

        if (count)
            clear_mode(&dev);
    }

    free_resources(dev.resources);

    return 0;
}
#endif

static int propValue(const char *propName)
{
    int ret = FAIL_COMMUNICATION;
#if 0
    char *argv[] = {
            "",
            "-c"
    };
    execCmd(ARRAY_SIZE(argv), argv);
#else
    int i, j;

    GET_RESOURCE(dev);

#if 0
    dump_connectors(&dev);
#else
    for (i = 0; i < dev.resources->res->count_connectors; i++) {
        struct connector *_connector = &dev.resources->connectors[i];
        drmModeConnector *connector = _connector->connector;
        if (!connector)
            continue;

        if (DRM_MODE_CONNECTED == connector->connection) {
            if (_connector->props) {
                for (j = 0; j < (int)_connector->props->count_props; j++) {
                    //dump_prop(&dev, _connector->props_info[j], _connector->props->props[j], _connector->props->prop_values[j]);
                    //printf("[hardy] %s:%s\n", propName, _connector->props_info[j]->name );
                    if (_connector->props_info[j]
                        && (0 == strcmp(propName, _connector->props_info[j]->name)) ) {
                        ret = _connector->props->prop_values[j];
                        goto exit;
                    }
                }
            }
        }
    }
#endif

exit:
#endif
    plog("[sys] %s:%d\n", propName, ret );
    return ret;
}

static int setPropValue(const char *propName, int value)
{
    int i, j;

    GET_RESOURCE(dev);

    for (i = 0; i < dev.resources->res->count_connectors; i++) {
        struct connector *_connector = &dev.resources->connectors[i];
        drmModeConnector *connector = _connector->connector;
        if (!connector)
            continue;

        if (DRM_MODE_CONNECTED == connector->connection) {
            if (_connector->props) {
                for (j = 0; j < (int)_connector->props->count_props; j++) {
                    if (_connector->props_info[j]
                        && (0 == strcmp(propName, _connector->props_info[j]->name)) ) {
                        plog("[sys] set %s:[%d %d]\n", propName, _connector->props->prop_values[j], value );
                        if (_connector->props->prop_values[j] != value) {
                            drmModeObjectSetProperty(dev.fd, connector->connector_id, DRM_MODE_OBJECT_CONNECTOR,
                                _connector->props_info[j]->prop_id, value);

                            _connector->props->prop_values[j] = value;
                        }
                        break;
                    }
                }
            }
        }
    }

    return 0;
}

int getOutputBrightness()
{
    return propValue("brightness");
}

int getOutputContrast()
{
    return propValue("contrast");
}

int getOutputSaturation()
{
    return propValue("saturation");
}

int getOutputSharpness()
{
    plog("[sys] sharpness:%d\n", FAIL_NOEXIST );
    return FAIL_NOEXIST;
}

int getOutputHue()
{
    return propValue("hue");
}

int setOutputBrightness(int value)
{
    return setPropValue("brightness", value);
}

int setOutputContrast(int value)
{
    return setPropValue("contrast", value);
}

int setOutputSaturation(int value)
{
    return setPropValue("saturation", value);
}

int setOutputSharpness(int value)
{
    plog("[sys] sharpness:%d\n", FAIL_NOEXIST );
    return FAIL_NOEXIST;
}

int setOutputHue(int value)
{
    return setPropValue("hue", value);
}

static int getConf(const char *file, const char *name, char *value)
{
    char nameStr[128] = {0};
    char confLine[256] = {0};
    FILE * fp = NULL;

    if (NULL == value ) return FAIL_INVALID_PARAM;

    value[0] = 0;
    fp = fopen(file, "r");
    if (NULL != fp) {
        char *val = NULL;

        sprintf(nameStr, "%s=", name);
        while(fgets(confLine, sizeof(confLine), fp)) {

            val = strstr(confLine, nameStr);
            if (val == confLine) {
                val = confLine + strlen(nameStr);
                strcpy(value, val);

                // remove '\r' '\n' and space
                while((strlen(value) > 0)
                    && isspace(value[strlen(value)-1]) ) {
                    value[strlen(value)-1] = 0;
                }
                break;
            }
        }
        fclose(fp);
    }
    plog("[sys] get conf %s=%s\n", name, value);

    return SYS_SUCCESS;
}

static int setBoardConf(const char *name, const char *value)
{
    char command[256] = {0};
    FILE * fp = NULL;

    if ((NULL == name) ||(NULL == value) ) return FAIL_INVALID_PARAM;

    sprintf(command, "mount -o remount,rw / && sed -i 's/\\(%s\\)=.*/\\1=%s/' "BOARD_CONF_FILE";sync;mount -o remount,ro /",
                        name, value);

//    plog("[sys] command:%s\n", command);
    plog("[sys] set conf %s=%s\n", name, value);

    system(command);

    return SYS_SUCCESS;
}

int getCamMode(int *width, int *height)
{
    char mode[64];

    getConf(BOARD_CONF_FILE, "camode", mode);

    plog("[sys] get camode:%s\n", mode);

    sscanf(mode, "%dx%d", width, height);

    return SYS_SUCCESS;
}

int setCamMode(int width, int height)
{
    char timing[32] = {0};
    char command[128] = {0};

    plog("[sys] set camera [%d %d]\n", width, height);
    if (1280==width && 720==height) {
        strcpy(timing, "1280x720-25");
    } else if (1920==width && 1080==height) {
        strcpy(timing, "1920x1080-25");
    }

    return setBoardConf("camode", timing);
}

static const struct
{
    ANALOG_STD std;
    char *mode;
} ana_mode[] = {
    {AHD_1080P_30, "ahd 1080p30"},
    {AHD_1080P_25, "ahd 1080p25"},
    {AHD_720P_30, "ahd 720p30"},
    {AHD_720P_25, "ahd 720p25"},
    {AHD_720P_60, "ahd 720p60"},
    {AHD_720P_50, "ahd 720p50"},

    {TVI_1080P_30, "tvi 1080p30"},
    {TVI_1080P_25, "tvi 1080p25"},
    {TVI_720P_30, "tvi 720p30"},
    {TVI_720P_25, "tvi 720p25"},
    {TVI_720P_60, "tvi 720p60"},
    {TVI_720P_50, "tvi 720p50"},

    {CVI_1080P_30, "cvi 1080p30"},
    {CVI_1080P_25, "cvi 1080p25"},
    {CVI_1080P_60, "cvi 1080p60"},
    {CVI_1080P_50, "cvi 1080p50"},
    {CVI_720P_30, "cvi 720p30"},
    {CVI_720P_25, "cvi 720p25"},
    {CVI_720P_60, "cvi 720p60"},
    {CVI_720P_50, "cvi 720p50"},

    {ANA_PAL, "720pal"},
    {ANA_NTSC, "720n"},

    {ANA_UNSUPPORT, "unsupport"},
};

OUTPUT_PORT displayCurDevice()
{
    int i;
    OUTPUT_PORT port= PORT_ANALOG;

    GET_RESOURCE(dev);

    for (i = 0; i < dev.resources->res->count_connectors; i++) {
        struct connector *_connector = &dev.resources->connectors[i];
        drmModeConnector *connector = _connector->connector;
        if (!connector)
            continue;

        if ((DRM_MODE_CONNECTED == connector->connection)
            && (0 == strcmp("HDMI-A-1", _connector->name)) ){
            port= PORT_HDMI;
            break;
        }
    }

    plog("[sys] active port:%d\n", port);

    return port;
}

#define drive_list "/proc/bus/input/devices"
static int device_val(const char *devname, const char *name, char *value)
{
    int ret = FAIL_COMMUNICATION;
    FILE *fp = NULL;

    int found = 0;
    char buf[256] = {0};
    char findstr[128] = {0};
    int len;

    fp = fopen(drive_list,"r");
    if (NULL == fp) goto exit;

    sprintf(findstr, "Name=\"%s\"", devname);
    while (NULL != fgets(buf, sizeof(buf), fp)) {
        if (NULL != strstr(buf, findstr)) {
            found = 1;
            break;
        }
    }

    if (found) {
        found = 0;
        char *pe = NULL;
        while (NULL != fgets(buf, sizeof(buf), fp)) {
            if (NULL != (pe=strstr(buf, name)) ) {
                found = 1;
                pe += strlen(name);
                while ((len=strlen(pe)) > 0) {
                    char *pc = pe+len-1;

                    if (isspace(*pc)) {
                        *pc = 0;
                    } else {
                        break;
                    }
                }
                strcpy(value, pe);
                break;
            }
        }
    }

    fclose(fp);

    if (!found) goto exit;

    ret = SYS_SUCCESS;

exit:
    //plog("[sys] boardType:%d\n", boardType);
    return ret;
}

/*
 * F -- pra2002 flag
 * L -- pra2002 ld
 * C -- pra2002 core
 */
static int getBoardType()
{
    static char boardType = 0xff;
    int i, diff, closest = 0x7fffffff;
    FILE *fp = NULL;
    char buf[16] = {0};
    int vol = 0;

    const struct
    {
        int vol;
        char type;
    } boardtype[] = {
        {14, KEY_F}, // PRA2002F
        {524, KEY_S}, // PRA2002 900mv
        {233, KEY_C}, // PRA2002-CORE
        {15, KEY_B}, // PRA2005
        {0, 0xFF},
    };

    if (0xff != boardType) goto exit;

    fp = fopen("/sys/devices/platform/fe720000.saradc/iio:device0/in_voltage1_raw","r");
    fscanf(fp, "%d", &vol);
    fclose(fp);

    for (i = 0; 0xFF != boardtype[i].type; i++) {
        diff = abs(boardtype[i].vol - vol);
        if (diff < closest) {
            closest = diff;
            boardType = boardtype[i].type;
        }
    }

exit:
    plog("[sys] boardType:%d\n", boardType);
    return boardType;
}

int displayDevice(char *buf, int bufLen)
{
    int board = getBoardType();

    if (NULL == buf) return FAIL_INVALID_PARAM;

    switch (board) {
        default: // rk3566 flag, rk3566 std
            strcpy(buf, "HDMI, ANALOG");
            break;
        case KEY_C:
            strcpy(buf, "HDMI");
            break;
        case KEY_B:
            strcpy(buf, "HDMI, MIPI, LVDS");
            break;
    }

    return SYS_SUCCESS;
}

int analogStandardList(ANALOG_STD list[], int itemCount)
{
    int i=0;
    ANALOG_STD supportList [] = {
        AHD_1080P_30, AHD_1080P_25, AHD_720P_30, AHD_720P_25, AHD_720P_60, AHD_720P_50,
        TVI_1080P_30, TVI_1080P_25, TVI_720P_30, TVI_720P_25, TVI_720P_60, TVI_720P_50,
        CVI_1080P_30, CVI_1080P_25, CVI_1080P_60, CVI_1080P_50, CVI_720P_30, CVI_720P_25, CVI_720P_60, CVI_720P_50,
        ANA_PAL, ANA_NTSC
    };

    for (i=0; i<ARRAY_SIZE(supportList) && i<itemCount; i++) {
        list[i] = supportList[i];
    }

    plog("[sys] got %d items\n", i);

    return i;
}

static inline ANALOG_STD mode2std(const char *mode)
{
    int i=0;

    for (i=0; i<ARRAY_SIZE(ana_mode); i++) {
        if (0 == strncmp(mode, ana_mode[i].mode, strlen(ana_mode[i].mode))) {
            return ana_mode[i].std;
        }
    }

    return ANA_UNSUPPORT;
}

ANALOG_STD analogStandard()
{
    ANALOG_STD std = ANA_UNSUPPORT;
    char mode[64];

    getConf(BOARD_CONF_FILE, "anamode", mode);
    std = mode2std(mode);

    plog("[sys] standard:0x%x \"%s\"\n", std, mode);
    return std;
}

int setAnalogStandard(ANALOG_STD std)
{
    int i=0;
    char timing[64] = {0};
    char command[128] = {0};

    for (i=0; i<ARRAY_SIZE(ana_mode); i++) {
        if (ana_mode[i].std == std) {
            setBoardConf("anamode", ana_mode[i].mode);
            break;
        }
    }

    if (i >= ARRAY_SIZE(ana_mode)) return FAIL_NOEXIST;

    switch(std) {
    default:// AHD_1080P_25 TVI_1080P_25 CVI_1080P_25
        strcpy(timing, "1920x1080-25");
        break;

    case AHD_1080P_30:
    case TVI_1080P_30:
    case CVI_1080P_30:
        strcpy(timing, "1920x1080-30");
        break;

    case CVI_1080P_60:
        strcpy(timing, "1920x1080-60");
        break;

    case CVI_1080P_50:
        strcpy(timing, "1920x1080-50");
        break;

    case AHD_720P_30:
    case TVI_720P_30:
    case CVI_720P_30:
        strcpy(timing, "1280x720-30");
        break;

    case AHD_720P_25:
    case TVI_720P_25:
    case CVI_720P_25:
        strcpy(timing, "1280x720-25");
        break;

    case AHD_720P_60:
    case TVI_720P_60:
    case CVI_720P_60:
        strcpy(timing, "1280x720-60");
        break;

    case AHD_720P_50:
    case TVI_720P_50:
    case CVI_720P_50:
        strcpy(timing, "1280x720-50");
        break;
    case ANA_PAL:
        strcpy(timing, "720x576-50");
        break;

    case ANA_NTSC:
        strcpy(timing, "720x480-60");
        break;
    }

    return setBoardConf("defaultiming", timing);
}

int getDisplayTiming(struct disTiming  *pmode)
{
    int i, j;
    drmModeConnector *connector = NULL;
    drmModeEncoder *encoder = NULL;
    char mode[128] = {0};
    char command[256] = {0};
    FILE * fp = NULL;

    if (NULL == pmode ) return FAIL_INVALID_PARAM;
    memset(pmode, 0, sizeof(struct disTiming));

    GET_RESOURCE(dev);

    for (i = 0; i < dev.resources->res->count_connectors; i++) {
        struct connector *_connector = &dev.resources->connectors[i];
        connector = _connector->connector;
        if (!connector)
            continue;

        if (DRM_MODE_CONNECTED == connector->connection) {
            break;
        }
    }

    for (i = 0; i < dev.resources->res->count_encoders; i++) {
        encoder = dev.resources->encoders[i].encoder;
        if (!encoder)
            continue;

        if (encoder->encoder_id == connector->encoder_id) break;
    }

    for (i = 0; i < dev.resources->res->count_crtcs; i++) {
        struct crtc *_crtc = &dev.resources->crtcs[i];
        drmModeCrtc *crtc = _crtc->crtc;
        if (!crtc)
            continue;

        if (encoder->crtc_id == crtc->crtc_id) {
            pmode->xres = crtc->mode.hdisplay;
            pmode->yres = crtc->mode.vdisplay;
            pmode->pixclock = crtc->mode.clock;
            pmode->hsyncLen = crtc->mode.htotal-crtc->mode.hdisplay;
            pmode->vsyncLen = crtc->mode.vtotal-crtc->mode.vdisplay;
            pmode->hsyncPolarity = crtc->mode.flags&0x01; // bit0
            pmode->vsyncPolarity = (crtc->mode.flags>>2) & 0x01; // bit2
            if (_crtc->props) {
                for (j = 0; j < _crtc->props->count_props; j++) {
                    if (NULL != strstr(_crtc->props_info[j]->name, "left margin")) {
                        pmode->leftMargin = 100-_crtc->props->prop_values[j];
                    } else if (NULL != strstr(_crtc->props_info[j]->name, "right margin")) {
                        pmode->rightMargin = 100-_crtc->props->prop_values[j];
                    } else if (NULL != strstr(_crtc->props_info[j]->name, "top margin")) {
                        pmode->upperMargin = 100-_crtc->props->prop_values[j];
                    } else if (NULL != strstr(_crtc->props_info[j]->name, "bottom margin")) {
                        pmode->lowerMargin = 100-_crtc->props->prop_values[j];
                    }
                }
            }

            break;
        }
    }

    getConf(BOARD_CONF_FILE, "outiming", mode);
    if (strlen(mode) > strlen("customized ") ) {
        CUSTIMING custiming = {0};
        sscanf(mode, "%s %d %d %d %d %d %d %d %d %d %d %d",
            &command, &custiming.vrefresh,
            &custiming.hdisplay, &custiming.hsync_start, &custiming.hsync_end, &custiming.htotal,
            &custiming.vdisplay, &custiming.vsync_start, &custiming.vsync_end, &custiming.vtotal,
            &custiming.clock, &custiming.flags);

        pmode->xres = custiming.hdisplay;
        pmode->yres = custiming.vdisplay;
        pmode->pixclock = custiming.clock;
        pmode->hsyncLen = custiming.hsync_end-custiming.hsync_start;
        pmode->vsyncLen = custiming.vsync_end-custiming.vsync_start;
        pmode->hsyncPolarity = (0 != (custiming.flags & DRM_MODE_FLAG_PHSYNC));
        pmode->vsyncPolarity = (0 != (custiming.flags & DRM_MODE_FLAG_PVSYNC));

        plog("[sys] customized %d %d %d %d %d %d %d %d %d %d %d\n",
            pmode->xres, pmode->yres, pmode->pixclock
            , pmode->leftMargin, pmode->rightMargin, pmode->upperMargin, pmode->lowerMargin
            , pmode->hsyncLen,pmode->vsyncLen, pmode->hsyncPolarity, pmode->vsyncPolarity );
    }

    if ((0 == pmode->xres || 0 == pmode->yres)
        || (DRM_MODE_CONNECTOR_HDMIA == connector->connector_type
            && 2 == connector->connector_type_id) ) {
        if (connector->modes ) {
            pmode->xres = connector->modes[0].hdisplay;
            pmode->yres = connector->modes[0].vdisplay;
            pmode->pixclock = connector->modes[0].clock;
            pmode->hsyncLen = connector->modes[0].htotal-connector->modes[0].hdisplay;
            pmode->vsyncLen = connector->modes[0].vtotal-connector->modes[0].vdisplay;
            pmode->hsyncPolarity = connector->modes[0].flags&0x01; // bit0
            pmode->vsyncPolarity = (connector->modes[0].flags>>2) & 0x01; // bit2
        }
    }

    plog("[sys] xres yres pixclock"
        " leftMargin rightMargin upperMargin lowerMargin"
        " hsyncLen vsyncLen hsyncPolarity vsyncPolarity\n");

    plog("[sys] %d %d %d %d %d %d %d %d %d %d %d\n",
        pmode->xres, pmode->yres, pmode->pixclock
        , pmode->leftMargin, pmode->rightMargin, pmode->upperMargin, pmode->lowerMargin
        , pmode->hsyncLen,pmode->vsyncLen, pmode->hsyncPolarity, pmode->vsyncPolarity );

    return SYS_SUCCESS;
}

int setDisplayTiming(struct disTiming  *pmode)
{
    int i, j;
    drmModeConnector *connector = NULL;
    drmModeEncoder *encoder = NULL;

    char optarg[128] = {0};
    unsigned int prop_count = 0;
    struct property_arg *prop_args = NULL;

    if (NULL == pmode ||
        pmode -> leftMargin<0 ||pmode -> leftMargin>100 ||
        pmode -> rightMargin<0 ||pmode -> rightMargin>100 ||
        pmode -> upperMargin<0 ||pmode -> upperMargin>100 ||
        pmode -> lowerMargin<0 ||pmode -> lowerMargin>100
        ) {
        return FAIL_INVALID_PARAM;
    }

    plog("[sys] set margin [%d %d %d %d]\n", pmode->leftMargin, pmode->rightMargin,
        pmode->upperMargin, pmode->lowerMargin);

    GET_RESOURCE(dev);

    for (i = 0; i < dev.resources->res->count_connectors; i++) {
        struct connector *_connector = &dev.resources->connectors[i];
        connector = _connector->connector;
        if (!connector)
            continue;

        if (DRM_MODE_CONNECTED == connector->connection) {
            break;
        }
    }

    for (i = 0; i < dev.resources->res->count_encoders; i++) {
        encoder = dev.resources->encoders[i].encoder;
        if (!encoder)
            continue;

        if (encoder->encoder_id == connector->encoder_id) break;
    }

    for (i = 0; i < dev.resources->res->count_crtcs; i++) {
        struct crtc *_crtc = &dev.resources->crtcs[i];
        drmModeCrtc *crtc = _crtc->crtc;
        if (!crtc)
            continue;

        if (0 == encoder->crtc_id
            || encoder->crtc_id == crtc->crtc_id) {
            if (_crtc->props) {
                for (j = 0; j < _crtc->props->count_props; j++) {
                    if (NULL != strstr(_crtc->props_info[j]->name, "left margin")
                        && ((100-pmode->leftMargin) != _crtc->props->prop_values[j])) {
                        plog("[sys] set left margin [%d %d]\n", 100-_crtc->props->prop_values[j], pmode->leftMargin);
                        _crtc->props->prop_values[j] = 100-pmode->leftMargin;
                        drmModeObjectSetProperty(dev.fd, crtc->crtc_id, DRM_MODE_OBJECT_CRTC,
                            _crtc->props_info[j]->prop_id, _crtc->props->prop_values[j]);
                    } else if (NULL != strstr(_crtc->props_info[j]->name, "right margin")
                        && ((100-pmode->rightMargin) != _crtc->props->prop_values[j])) {
                        plog("[sys] set right margin [%d %d]\n", 100-_crtc->props->prop_values[j], pmode->rightMargin);
                        _crtc->props->prop_values[j] = 100-pmode->rightMargin;
                        drmModeObjectSetProperty(dev.fd, crtc->crtc_id, DRM_MODE_OBJECT_CRTC,
                            _crtc->props_info[j]->prop_id, _crtc->props->prop_values[j]);
                    } else if (NULL != strstr(_crtc->props_info[j]->name, "top margin")
                        && ((100-pmode->upperMargin) != _crtc->props->prop_values[j])) {
                        plog("[sys] set top margin [%d %d]\n", 100-_crtc->props->prop_values[j], pmode->upperMargin);
                        _crtc->props->prop_values[j] = 100-pmode->upperMargin;
                        drmModeObjectSetProperty(dev.fd, crtc->crtc_id, DRM_MODE_OBJECT_CRTC,
                            _crtc->props_info[j]->prop_id, _crtc->props->prop_values[j]);
                    } else if (NULL != strstr(_crtc->props_info[j]->name, "bottom margin")
                        && ((100-pmode->lowerMargin) != _crtc->props->prop_values[j])) {
                        plog("[sys] set bottom margin [%d %d]\n", 100-_crtc->props->prop_values[j], pmode->lowerMargin);
                        _crtc->props->prop_values[j] = 100-pmode->lowerMargin;
                        drmModeObjectSetProperty(dev.fd, crtc->crtc_id, DRM_MODE_OBJECT_CRTC,
                            _crtc->props_info[j]->prop_id, _crtc->props->prop_values[j]);
                    }
                }
            }

        }
    }

    return SYS_SUCCESS;
}

int resetSys()
{
    system("/usr/sbin/resetsys.sh;reboot");

    return SYS_SUCCESS;
}

int screenSize(int *width, int *height)
{
    int i;

    GET_RESOURCE(dev);

    *width = *height = 0;

    for (i = 0; i < dev.resources->res->count_connectors; i++) {
        struct connector *_connector = &dev.resources->connectors[i];
        drmModeConnector *connector = _connector->connector;
        if (!connector)
            continue;

        if ((DRM_MODE_CONNECTED == connector->connection)
            && (0 == strcmp("HDMI-A-1", _connector->name)) ){
            *width = connector->mmWidth;
            *height = connector->mmHeight;
        }
    }

    plog("[hardy] screen:[%d %d]\n", *width, *height );

    return SYS_SUCCESS;
}

int displayBufSize(int *width, int *height)
{
    struct disTiming mode;

    if (SYS_SUCCESS == getDisplayTiming(&mode) ) {
        *width = mode.xres;
        *height = mode.yres;
    } else {
        *width = *height = 0;
    }

    plog ("[sys] width:%d height:%d\n", *width, *height);

    return SYS_SUCCESS;
}

int wifiName(char *buf, int bufLen)
{
    FILE * fp = NULL;

    if ((NULL == buf) ||(bufLen < 33) ) {
        printf("[sys] requirement -- bufLen >= 33\n");
        return FAIL_INVALID_PARAM;
    }

    memset(buf, 0, bufLen);
    fp = popen("cat /proc/sys/kernel/hostname", "r");
    if (NULL != fp) {
        size_t len=fread (buf, 1, bufLen, fp);
        if (len > 0) {
            buf[len] = 0;
        }
        pclose(fp);
    }

    plog("[sys] ssid:%s\n", buf);

    return SYS_SUCCESS;
}

int wifiPassword(char *buf, int bufLen)
{
    return FAIL_NOIMPLEMENT;
}

int setWifiName(char *buf)
{
    return FAIL_NOIMPLEMENT;
}

int setWifiPassword(char *buf)
{
    return FAIL_NOIMPLEMENT;
}

int boardInfo(char *buf, int bufLen)
{
    int ret = SYS_SUCCESS;
    char name[17] = {0};
    char version[9] = {0};
    char boardversion[8] = {0};
//    int board = getBoardType();

    if ((NULL == buf) ||(bufLen < 22) ) {
        printf("[sys] requirement -- bufLen >= 22\n");
        return FAIL_INVALID_PARAM;
    }

    getConf(SYS_CONF_FILE, "boardtype", name);
    getConf(SYS_CONF_FILE, "boardversion", boardversion);
    getConf(SYS_CONF_FILE, "firmwarever", version);

    if ((strlen(name)+strlen(boardversion)+strlen(version)+1) > bufLen) return FAIL_INVALID_PARAM;
    sprintf(buf, "%s:%s:%s", name, boardversion, version);
    plog("[sys] %s\n", buf);

    return SYS_SUCCESS;
}

int firmwareVer(char *buf, int bufLen)
{
    return getConf(SYS_CONF_FILE, "firmwarever", buf);
}

int supplier(char *buf, int bufLen)
{
    if ((NULL == buf) ||(bufLen < 17) ) {
        printf("[sys] requirement -- bufLen >= 17\n");
        return FAIL_INVALID_PARAM;
    }

    return getConf(SYS_CONF_FILE, "vendor", buf);
}

int solution(char *buf, int bufLen)
{
    if ((NULL == buf) ||(bufLen < 17) ) {
        printf("[sys] requirement -- bufLen >= 17\n");
        return FAIL_INVALID_PARAM;
    }

    return getConf(SYS_CONF_FILE, "solution", buf);
}

int machineID(char *buf, int bufLen)
{
    FILE * fp = NULL;

    if ((NULL == buf) ||(bufLen < 17) ) {
        printf("[sys] requirement -- bufLen >= 17\n");
        return FAIL_INVALID_PARAM;
    }

    memset(buf, 0, bufLen);
    fp = popen("awk '/^Serial/ {printf(\"%s\", $3)}' /proc/cpuinfo", "r");
    if (NULL != fp) {
        size_t len=fread (buf, 1, bufLen, fp);
        if (len > 0) {
            buf[len] = 0;
        }
        pclose(fp);
    }

    plog("[sys] Serial:%s\n", buf);
    return SYS_SUCCESS;

}

int upgrade(char *path)
{
    char command[128];

    sprintf(command, "echo \"%s\" > /tmp/upath", path);
    system(command);

    return SYS_SUCCESS;
}

/*
 * gsensor
 *
 * name -- "sc7a20",
 *
 * en -- 1: eanble vibration interrupt
 *    0: disable vibration interrupt
 *
 * ths -- sensitivity (0 ~ 127)
 *             ^   ^
 *             H   L
 *
 *     experience value: level  value
 *                   0          0x7f
 *                   1          0x20
 *                   2          0x08
 *                   3          0x04
 */
int gsensor_shock_en(int level)
{
    int ret = FAIL_COMMUNICATION;
    char sysfs[128] = {0};
    char command[256];
    char *name = "sc7a20";
    int ths = 0;

    switch (level) {
    default:
        plog("[sys] %s: invalid param\n", __FUNCTION__);
        return FAIL_INVALID_PARAM;
        break;

    case 0:
        ths = 0x7f;
        break;
    case 1:
        ths = 0x20;
        break;
    case 2:
        ths = 0x08;
        break;
    case 3:
        ths = 0x04;
        break;
    }

    ret = device_val(name, "Sysfs=", sysfs);
    if (SYS_SUCCESS == ret) {
        //sprintf(command, "echo %d > /sys%s/slope_en", en, sysfs);
        //system(command);
        //plog("[sys] cmd: %s\n", command);

        sprintf(command, "echo %d > /sys%s/slope_set", ths, sysfs);
        system(command);
        //plog("[sys] cmd: %s\n", command);
    }

exit:
    plog("[sys] sensor level:%d\n", level);
    return ret;
}

int cust_timing(CUSTIMING *timing)
{
    CUSTIMING custiming = {0};
    char mode[128];
    char value[128] = {0};

    getConf(BOARD_CONF_FILE, "outiming", mode);
    plog("[sys] old outiming:%s\n", mode);

    if (NULL == timing || 0 == timing->clock ) {
        return (strlen(mode) < strlen("customized ") ) ?
            FAIL_NOIMPLEMENT:setBoardConf("outiming", "");
    }

    memset(&custiming, 0, sizeof(cust_timing));
    if (strlen(mode) > strlen("customized ") ) {
        sscanf(mode, "%s %d %d %d %d %d %d %d %d %d %d %d",
            value, &custiming.vrefresh,
            &custiming.hdisplay, &custiming.hsync_start, &custiming.hsync_end, &custiming.htotal,
            &custiming.vdisplay, &custiming.vsync_start, &custiming.vsync_end, &custiming.vtotal,
            &custiming.clock, &custiming.flags);
    }

    if (custiming.vrefresh != timing->vrefresh
        || custiming.hdisplay != timing->hdisplay
        || custiming.hsync_start != timing->hsync_start
        || custiming.hsync_end != timing->hsync_end
        || custiming.htotal != timing->htotal
        || custiming.vdisplay != timing->vdisplay
        || custiming.vsync_start != timing->vsync_start
        || custiming.vsync_end != timing->vsync_end
        || custiming.vtotal != timing->vtotal
        || custiming.clock != timing->clock
        || custiming.flags != timing->flags) {

        sprintf(value, "customized %d %d %d %d %d %d %d %d %d %d %d",
            timing->vrefresh,
            timing->hdisplay, timing->hsync_start, timing->hsync_end, timing->htotal,
            timing->vdisplay, timing->vsync_start, timing->vsync_end, timing->vtotal,
            timing->clock, timing->flags);
        return setBoardConf("outiming", value);
    }

    return FAIL_NOIMPLEMENT;
}

int lvds_timing(LVDSTIMING *timing)
{
    LVDSTIMING custiming = {0};
    char mode[128];
    char value[128] = {0};

    getConf(BOARD_CONF_FILE, "outiming", mode);
    plog("[sys] old outiming:%s\n", mode);

    if (NULL == timing || 0 == strlen(timing->name) ) {
        return (strlen(mode) < strlen("vvvpdd") ) ?
            FAIL_NOIMPLEMENT:setBoardConf("outiming", "");
    }

    memset(&custiming, 0, sizeof(cust_timing));
    if (strlen(mode) > strlen("vvvpdd") ) {
        sscanf(mode, "%s 0x%x", custiming.name, &custiming.bus_format);
    }

    if (0 != strcmp(custiming.name, timing->name)
        || custiming.bus_format != timing->bus_format) {
        sprintf(value, "%s 0x%x", timing->name, timing->bus_format);
        return setBoardConf("outiming", value);
    }

    return FAIL_NOIMPLEMENT;
}

int getVoltage()
{
    int voltage = 0;

    FILE *fp = NULL;
    char buf[16] = {0};

    fp = fopen("/sys/devices/platform/fe720000.saradc/iio:device0/in_voltage3_raw","r");
    if (NULL == fp) return 12000; // default 12v

    fscanf(fp, "%d", &voltage);
    fclose(fp);

    voltage = ((int)(voltage*1.8*16*1000)) >> 10 ;

    // Corrected wire loss
    switch(voltage/1000*10) {
    case 90:
        voltage += 520;
        break;

    case 100:
        voltage += 340;
        break;

    case 110:
        voltage += 320;
        break;

    case 120:
        voltage += 260;
        break;

    case 130:
        voltage += 190;
        break;

    case 140:
        voltage += 179;
        break;

    case 150:
        voltage += 54;
        break;

    default:
        voltage += 41;
        break;

    }

//    plog("[sys] voltage:%d\n", voltage);

    return voltage;
}

static drmModePlanePtr sysPlaneByName(const char *name)
{
    int i, j;

    for (i=0; i<dev.resources->plane_res->count_planes; i++) {
        struct plane *planes = dev.resources->planes+i;

        for (j=0; j<planes->props->count_props; j++) {
            if (!strcmp(planes->props_info[j]->name, "NAME")
                && !strcmp(name, planes->props_info[j]->enums[0].name) ) {
                plog("%s plane name:%s id:%d\n", __FUNCTION__,
                    planes->props_info[j]->enums[0].name, planes->plane->plane_id);
                return planes->plane;
            }
        }
    }

    return NULL;
}

static void sysAdjZpos(uint32_t planeid, int zpos)
{
    int i, j;

    for (i=0; i<dev.resources->plane_res->count_planes; i++) {
        struct plane *planes = dev.resources->planes+i;

        if (planes->plane->plane_id != planeid) continue;

        for (j=0; j<planes->props->count_props; j++) {
            plog("%s plane id:%d name:%s\n", __FUNCTION__,
                planes->plane->plane_id, planes->props_info[j]->name);

            if (!strcmp(planes->props_info[j]->name, "zpos") ) {
                drmModeAtomicReq *req;

                req = drmModeAtomicAlloc();
                drmModeAtomicAddProperty(req, planeid, planes->props_info[j]->prop_id, zpos);
                drmModeAtomicCommit(dev.fd, req, 0, NULL);
                drmModeAtomicFree(req);
                return;
            }
        }
    }
}

drmModePlanePtr getVideoPlane()
{
    int i, j;
    drmModeConnector *connector = NULL;
    drmModePlanePtr plane = NULL;

    GET_RESOURCE(dev);

    for (i = 0; i < dev.resources->res->count_connectors; i++) {
        struct connector *_connector = &dev.resources->connectors[i];
        connector = _connector->connector;
        if (!connector)
            continue;

        if (DRM_MODE_CONNECTED == connector->connection) {
            break;
        }
    }

    if(access("/oem/pos/bin/rkmenu", F_OK) == 0) {
        if (DRM_MODE_CONNECTOR_HDMIA == connector->connector_type
            && 1 == connector->connector_type_id) {
            plane = sysPlaneByName("Smart0-win0");
            sysAdjZpos(plane->plane_id, 4);
        } else {
            plane = sysPlaneByName("Esmart0-win0");
        }
    }else {
        if (DRM_MODE_CONNECTOR_HDMIA == connector->connector_type
            && 1 == connector->connector_type_id) {
            plane = sysPlaneByName("Esmart0-win0");
        } else {
            plane = sysPlaneByName("Smart0-win0");
            sysAdjZpos(plane->plane_id, 4);
        }
    }

    plog("video plane id:%d\n", plane->plane_id);

    return plane;
}

