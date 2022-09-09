#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <linux/v4l2-subdev.h>

#include "syswrapper.h"

#define XS9922_PATH "/dev/v4l-subdev2"

#define XS9922_SET_FMT	\
    _IOR('V', BASE_VIDIOC_PRIVATE + 32, struct v4l2_subdev_format)

static struct
{
    char *prototype;
    /*
    * 0 -- no parameter
    * 1 -- one parameter, int
    * -1 -- one parameter, point var
    */
    int paranum;
    char *funcname;
    void *func;
} funcs[] = {
    {"int getOutputBrightness()", 0, "getOutputBrightness", getOutputBrightness},
    {"int getOutputContrast()", 0, "getOutputContrast", getOutputContrast},
    {"int getOutputSaturation()", 0, "getOutputSaturation", getOutputSaturation},
    {"int getOutputSharpness()", 0, "getOutputSharpness", getOutputSharpness},
    {"ANALOG_STD analogStandard()", 0, "analogStandard", analogStandard},
    {"OUTPUT_PORT displayCurDevice()", 0, "displayCurDevice", displayCurDevice},
    {"int getDisplayTiming(struct disTiming *pmode)", 0, "getDisplayTiming", getDisplayTiming},
    {"int setDisplayTiming(struct disTiming *pmode)", 0, "setDisplayTiming", setDisplayTiming},
    {"int getVoltage()", 0, "getVoltage", getVoltage},
    {"drmModePlanePtr getVideoPlane()", 0, "getVideoPlane", getVideoPlane},

    {"int wifiName(char *, int)", 0, "wifiName", wifiName},
    {"int wifiPassword(char *, int)", 0, "wifiPassword", wifiPassword},
    {"int boardInfo(char *, int)", 0, "boardInfo", boardInfo},
    {"int supplier(char *, int)", 0, "supplier", supplier},
    {"int solution(char *, int)", 0, "solution", solution},
    {"int machineID(char *, int)", 0, "machineID", machineID},
    {"int displayDevice(char *, int)", 0, "displayDevice", displayDevice},
    {"int firmwareVer(char *, int)", 0, "firmwareVer", firmwareVer},
    {"int resetSys()", 0, "resetSys", resetSys},
    {"int getOutputHue()", 0, "getOutputHue", getOutputHue},

    {"int getCamMode(int *, int *)", 0, "getCamMode", getCamMode},
    {"int screenSize(int *, int *)", 0, "screenSize", screenSize},
    {"int displayBufSize(int *, int *)", 0, "displayBufSize", displayBufSize},

    {"int setOutputBrightness(int)", 1, "setOutputBrightness", setOutputBrightness},
    {"int setOutputContrast(int)", 1, "setOutputContrast", setOutputContrast},
    {"int setOutputSaturation(int)", 1, "setOutputSaturation", setOutputSaturation},
    {"int setOutputSharpness(int)", 1, "setOutputSharpness", setOutputSharpness},
    {"int setAnalogStandard(ANALOG_STD std)", 1, "setAnalogStandard", setAnalogStandard},
    {"int setOutputHue(int value)", 1, "setOutputHue", setOutputHue},
    {"int gsensor_shock_en(int level)", 1, "gsensor_shock_en", gsensor_shock_en},

    {"int setCamMode(int width, int height)", 2, "setCamMode", setCamMode},

    {"int setWifiName(char *buf)", -1, "setWifiName", setWifiName},
    {"int setWifiPassword(char *buf)", -1, "setWifiPassword", setWifiPassword},
    {"int upgrade(char *path)", -1, "upgrade", upgrade},
    {"void analogStandardList(ANALOG_STD list[], int itemCount)", -1, "analogStandardList", analogStandardList},
    {"int cust_timing(CUSTIMING *timing)", -1, "cust_timing", cust_timing},
    {"int lvds_timing(LVDSTIMING *timing)", -1, "lvds_timing", lvds_timing},
};

static void usage() {
    int i;

    printf("query:\n");
    printf("  mode -- get current mode\n");
    printf("  con -- get contrast\n");
    printf("  bri -- get brightness\n");
    printf("  sat -- get saturation\n");
    printf("  hue -- get hue\n");
    printf("\n");

    printf("setting:\n");
    printf("  720p -- set ahd 720P\n");
    printf("  con xx -- set contrast\n");
    printf("  bri xx -- set brightness\n");
    printf("  sat xx -- set saturation\n");
    printf("  hue xx -- set hue\n");
    printf("\n");

    printf("test libsyswrapper.so:\n");
    printf("  para1 -- function name\n");
    printf("  para2..n -- parameter list of tested function\n");
    for (i=0; i<sizeof(funcs)/sizeof(funcs[0]); i++)
        printf("    %s\n", funcs[i].prototype);
}

int main(int argc, char** argv)
{
    int fd = 0;
    int ret;

    if (argc < 2) {
        usage();
        return 0;
    }

    fd = open(XS9922_PATH, O_RDWR);

    if (0 == strcmp("720p", argv[1])) {
        struct v4l2_subdev_format fmt;

        fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
        fmt.format.width = 1280;
        fmt.format.height = 720;

        ret = ioctl(fd, XS9922_SET_FMT, &ret);
        if (ret < 0)
            printf("error: %s\n", strerror(errno));
    } else if (0 == strcmp("con", argv[1])) {
        struct v4l2_control ctl;
        ctl.id=V4L2_CID_CONTRAST;

        if (3 == argc) {
            sscanf(argv[2], "%d", &ctl.value);
            ioctl(fd,VIDIOC_S_CTRL,&ctl);
        } else {
            ioctl(fd,VIDIOC_G_CTRL,&ctl);
            printf("bright:%d\n", ctl.value);
        }
    } else if (0 == strcmp("bri", argv[1])) {
        struct v4l2_control ctl;
        ctl.id=V4L2_CID_BRIGHTNESS;

        if (3 == argc) {
            sscanf(argv[2], "%d", &ctl.value);
            ioctl(fd,VIDIOC_S_CTRL,&ctl);
        } else {
            ioctl(fd,VIDIOC_G_CTRL,&ctl);
            printf("bright:%d\n", ctl.value);
        }
    } else if (0 == strcmp("sat", argv[1])) {
        struct v4l2_control ctl;
        ctl.id=V4L2_CID_SATURATION;

        if (3 == argc) {
            sscanf(argv[2], "%d", &ctl.value);
            ioctl(fd,VIDIOC_S_CTRL,&ctl);
        } else {
            ioctl(fd,VIDIOC_G_CTRL,&ctl);
            printf("bright:%d\n", ctl.value);
        }
    } else if (0 == strcmp("hue", argv[1])) {
        struct v4l2_control ctl;
        ctl.id=V4L2_CID_HUE;

        if (3 == argc) {
            sscanf(argv[2], "%d", &ctl.value);
            ioctl(fd,VIDIOC_S_CTRL,&ctl);
        } else {
            ioctl(fd,VIDIOC_G_CTRL,&ctl);
            printf("bright:%d\n", ctl.value);
        }
    } else if (0 == strcmp("analogStandardList", argv[1]) ) {
        ANALOG_STD list[100];
        analogStandardList(list, sizeof(list)/sizeof(list[0]));
    } else if (0 == strcmp("getDisplayTiming", argv[1]) ) {
        struct disTiming mode;
        getDisplayTiming(&mode);
    } else if (0 == strcmp("setDisplayTiming", argv[1]) ) {
        struct disTiming mode;

        mode.xres = atoi(argv[2]);
        mode.yres = atoi(argv[3]);
        mode.pixclock = atoi(argv[4]);
        mode.leftMargin = atoi(argv[5]);
        mode.rightMargin = atoi(argv[6]);
        mode.upperMargin = atoi(argv[7]);
        mode.lowerMargin = atoi(argv[8]);
        mode.hsyncLen = atoi(argv[9]);
        mode.vsyncLen = atoi(argv[10]);
        mode.hsyncPolarity = atoi(argv[11]);
        mode.vsyncPolarity = atoi(argv[12]);
        setDisplayTiming(&mode);
    } else if (0 == strcmp("cust_timing", argv[1]) ) {
        CUSTIMING mode;

        mode.vrefresh = atoi(argv[2]);
        mode.hdisplay = atoi(argv[3]);
        mode.hsync_start = atoi(argv[4]);
        mode.hsync_end = atoi(argv[5]);
        mode.htotal = atoi(argv[6]);
        mode.vdisplay = atoi(argv[7]);
        mode.vsync_start = atoi(argv[8]);
        mode.vsync_end = atoi(argv[9]);
        mode.vtotal = atoi(argv[10]);
        mode.clock = atoi(argv[11]);
        mode.flags = atoi(argv[12]);
        cust_timing(&mode);
    } else if (0 == strcmp("lvds_timing", argv[1]) ) {
        LVDSTIMING mode;

        strcpy(mode.name, argv[2]);
        sscanf(argv[3], "%x", &mode.bus_format);
        lvds_timing(&mode);
    } else {
        int i=0;
        for (i=0; i<sizeof(funcs)/sizeof(funcs[0]); i++) {
            if (0 == strcmp(argv[1], funcs[i].funcname )) {
                switch ( funcs[i].paranum) {
                case 0:
                    {
                        if (NULL != strstr(funcs[i].prototype, "(char *, int)") ) {
                            char buf[128] = {0};

                            int (*func)(char *, int) =funcs[i].func ;
                            func(buf, sizeof(buf));
                        } else if (NULL != strstr(funcs[i].prototype, "(int *, int *)") ) {
                            int a=0, b=0;

                            int (*func)(int *, int *) =funcs[i].func ;
                            func(&a, &b);
                        } else if (NULL != strstr(funcs[i].prototype, "(char *, int, int)") ) {
                            int (*func)(char *, int, int) =funcs[i].func ;
                            func(argv[2], atoi(argv[3]), atoi(argv[4]) );
                        } else {
                            printf("[hardy] %s:%d\n", __FUNCTION__, __LINE__);
                            int (*func)() =funcs[i].func ;
                            func();
                        }
                    }
                    break;
                case 1:
                    {
                        int (*func)(int) =funcs[i].func ;
                        func(atoi(argv[2]));
                    }
                    break;
                case -1:
                    {
                        int (*func)(void *) =funcs[i].func ;
                        func(argv[2]);
                    }
                    break;
                case 2:
                    {
                        int (*func)(int, int) =funcs[i].func ;
                        func(atoi(argv[2]), atoi(argv[3]));
                    }
                    break;
                }
            }
        }
    }

    close (fd);

    return 0;
}

