/*
 * config file -- /oem/sys.conf
 */
#ifndef __SYS_INTERFACE_H__
#define __SYS_INTERFACE_H__

#include "xf86drm.h"
#include "xf86drmMode.h"
#include <libdrm/drm_fourcc.h>

#define SYS_SUCCESS         0
#define FAIL_NOEXIST        -1
#define FAIL_COMMUNICATION  -2
#define FAIL_INVALID_PARAM  -3
#define FAIL_NOIMPLEMENT    -100

typedef enum {
    PORT_HDMI = 0,
    PORT_ANALOG = 2
} OUTPUT_PORT;

typedef enum {
    AHD_1080P_30=0,
    AHD_1080P_25,
    AHD_720P_30,
    AHD_720P_25,
    AHD_720P_60,
    AHD_720P_50,

    TVI_1080P_30=0x10,
    TVI_1080P_25,
    TVI_720P_30,
    TVI_720P_25,
    TVI_720P_60,
    TVI_720P_50,

    CVI_1080P_30=0x20,
    CVI_1080P_25,
    CVI_1080P_60,
    CVI_1080P_50,
    CVI_720P_30,
    CVI_720P_25,
    CVI_720P_60,
    CVI_720P_50,

    ANA_PAL = 0x30,
    ANA_NTSC,

    ANA_UNSUPPORT = 0xFF
}ANALOG_STD;

#ifdef __cplusplus
extern "C"{
#endif

/*
 * get TX's property
*/
int getOutputBrightness();
int getOutputContrast();
int getOutputSaturation();
int getOutputSharpness();
int getOutputHue();

/*
 * set TX's property
 *
 * valid value -- 0~100
*/
int setOutputBrightness(int value);
int setOutputContrast(int value);
int setOutputSaturation(int value);
int setOutputSharpness(int value);
int setOutputHue(int value);

/*
 *
 */
int getCamMode(int *width, int *height);

/*
 * according to xs9922
 *
 * "1920x1080" or "1280x720"
 * it will be valid after reboot
 */
int setCamMode(int width, int height);

/*
 * get current output device
 */
OUTPUT_PORT displayCurDevice();

int displayDevice(char *buf, int bufLen);

/*
 * itemCount -- the number of ANALOG_STD
 *
 * return value
 *  <0  -- fail
 *  >=0  -- got items of list
*/
int analogStandardList(ANALOG_STD list[], int itemCount);

/*
 * get current analog standard of output
 */
ANALOG_STD analogStandard();

/*
 * It will be valid after reboot
 */
int setAnalogStandard(ANALOG_STD std);

struct disTiming {
    int xres;
    int yres;
    int pixclock; // unit k
    int leftMargin; // 0~100
    int rightMargin; // 0~100
    int upperMargin; // 0~100
    int lowerMargin; // 0~100
    int hsyncLen;
    int vsyncLen;
    int hsyncPolarity;
    int vsyncPolarity;
};
int getDisplayTiming(struct disTiming  *pmode);
int setDisplayTiming(struct disTiming  *pmode);

int resetSys();

/*
 * only for HDMI
 * unit: mm
 */
int screenSize(int *width, int *height);

/*
 *
 */
int displayBufSize(int *width, int *height);

/*
 * time -- system command
 *  demo:
 *       set tim: date -s "20140712 18:30:50"
 *       save time to rtc: hwclock -w
 *       switch timezone: ln -sf /usr/share/zoneinfo/Asia/Shanghai /etc/localtime
 *
 *  query: date --help
 *       date -R
 */
// time
// timeZone

int wifiName(char *buf, int bufLen);
int wifiPassword(char *buf, int bufLen);

/*
 * It will be valid after reboot
 *
 * password len: 8~16 bytes
 */
int setWifiName(char *buf);
int setWifiPassword(char *buf);

int boardInfo(char *buf, int bufLen);
int firmwareVer(char *buf, int bufLen);
int supplier(char *buf, int bufLen);
int solution(char *buf, int bufLen);
int machineID(char *buf, int bufLen);

/*
 * path -- the path of upgrade package
 */
int upgrade(char *path);

/*
** level: 0 ~ 3
*/
int gsensor_shock_en(int level);

/*
** timing will be cleaned if timing is NULL
**
** |      active video     | fporch  | sync | bporch |
** ---------------------------------------------------
** |                                 ^      ^        |
** |                                 |      |        |
** |                                start  end       |
** |                                                 |
** |<---------------------total--------------------->|
**
** vrefresh = clock/htotal/vtotal
**
*/
typedef struct {
    int clock;		/* in kHz */
    int hdisplay;   /* active video */
    int hsync_start;
    int hsync_end;
    int htotal;
    int vdisplay;
    int vsync_start;
    int vsync_end;
    int vtotal;

    /**
    * @vrefresh:
    *
    * Vertical refresh rate, for debug output in human readable form. Not
    * used in a functional way.
    *
    * This value is in Hz.
    */
    int vrefresh;

    /*
    ** #define DRM_MODE_FLAG_PHSYNC			(1 << 0)
    ** #define DRM_MODE_FLAG_NHSYNC			(1 << 1)
    ** #define DRM_MODE_FLAG_PVSYNC			(1 << 2)
    ** #define DRM_MODE_FLAG_NVSYNC			(1 << 3)
    **
    ** #define DRM_MODE_FLAG_RGB_MASK       (0xff<<16)
    **  ex: flag |= (MEDIA_BUS_FMT_RGB888_1X24 & 0xff) << 16
    */
    unsigned int flags;
}CUSTIMING;
int cust_timing(CUSTIMING *timing);

/*
*   panel                                       timing              bus_format
*
*   nissan-sylphy-y19/y20 hsae-8inch:           1280x720p60         MEDIA_BUS_FMT_RGB888_1X24
*   nissan-xtrail-y19 hsae-9inch:               1280x720p60         MEDIA_BUS_FMT_RGB888_1X24
*   vw-polo-y21 tb-8inch:                       1280x720p60         MEDIA_BUS_FMT_RGB888_1X24
*
*   vw-magotan-y21 desay-8inch:                 1280x720_vw         MEDIA_BUS_FMT_RGB888_1X24
*
*   toyota-corolla-y21 desay-8inch:             1024x600p60         MEDIA_BUS_FMT_RGB888_1X24
*   toyota-highlander-y22 desay-8inch:          1024x600p60         MEDIA_BUS_FMT_RGB888_1X24
*   toyota-highlander-y19/y20 fujitsu-10inch:   1024x600p60         MEDIA_BUS_FMT_RGB888_1X24
*
*   nissan-teana-y19 boshi-8inch:               800x480p60          MEDIA_BUS_FMT_RGB888_1X24
*/
typedef struct {
    /*
    * support:
    *   1024x600p60 60 1024 1130 1238 1344 600 611 624 635 51200 5
    *   1280x720p60 60 1280 1390 1430 1650 720 725 730 750 74250 5
    *   1280x720_vw 60 1280 2008 2052 2200 720 1084 1089 1125 148500 10
    *   800x480p60 60 800 1186 1211 1256 480 493 495 533 40000 5
    */
    char name[32];

    /*
    * default is MEDIA_BUS_FMT_RGB888_1X24
    *
    * #define MEDIA_BUS_FMT_RGB888_1X24		    0x100a
    * #define MEDIA_BUS_FMT_RGB666_1X7X3_SPWG	0x1010
    */
    unsigned short bus_format;
}LVDSTIMING;

int lvds_timing(LVDSTIMING *timing);

/*
* for pra2002
*
* unit: mv
*/
int getVoltage();

drmModePlanePtr getVideoPlane();

#ifdef __cplusplus
}
#endif
#endif // __SYS_INTERFACE_H__
