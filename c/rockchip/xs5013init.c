#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#include <termios.h> //set baud rate

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>

/* cvi 720P60 pattern
xs5013 "mem w 0x11003400 0x0"
xs5013 "mem w 0x11000060 0x0011"
xs5013 "mem w 0x11000060 0x1003"
xs5013 "mem w 0x11001010 0x0"
xs5013 "mem w 0x11001010 0xffffffff"
xs5013 "mem w 0x11048080 0x03"
xs5013 "mem w 0x11008188 0xff"
xs5013 "mem w 0x11008188 0x0"
xs5013 "mem w 0x1104805c 0x1"
xs5013 "mem w 0x11003400 0x1"
*/

#define TTY_NAME "/dev/ttyS3"
#define BAUDRATE B115200

#define XS_SUCCESS 0
#define XS_FAIL 1

//
#define PKG_HEAD 0x02
#define PKG_TAIL 0x03

#define IMG_ADDRESS (0x00010000)
#define XS_ADDRESS_LEN (4)
#define XS_COUNT_LEN (4)
#define XS_PERDATA_LEN (4)

// command
#define BM_CMD_NOTIFY 36
#define BM_CMD_DEBUG 20
#define BM_CMD_DLOAD 24
#define BM_CMD_WRITE 25
#define BM_CMD_READ 26
#define BM_CMD_RUN 27
#define BM_CMD_EXIT 101
#define BM_CMD_SHAKE 85
#define BM_CMD_ACK 170
#define BM_CMD_OK 5
#define BM_CMD_ERR 10
#define MB_CMD_MINIBOOT 117
#define MB_CMD_ERASE 69

enum {
    STX = 0,
    CMD,
    CHKSUM,
    ETX,
    HEADLEN,
    extDATA = HEADLEN
};

typedef struct {
    char stx;
    char cmd;
    char chksum;
    char etx;
}NoDataCmd;

static NoDataCmd NoDataCmdPkg = {
    .stx = PKG_HEAD,
    .cmd = 0,
    .chksum = 0,
    .etx = PKG_TAIL
};

// hexdump -v -e '4/1 "0x%02x, " "\n"' miniboot_reload_xs5013_ahd.img > ahdData
static const char ahdData[] = {
#include "ahdData"
};

// hexdump -v -e '4/1 "0x%02x, " "\n"' miniboot_reload_xs5013_cvi.img > cviData
static const char cviData[] = {
#include "cviData"
};

// hexdump -v -e '4/1 "0x%02x, " "\n"' miniboot_reload_xs5013_tvi.img > tviData
static const char tviData[] = {
#include "tviData"
};

static const char patternData[] = {
#include "patternData"
};


static inline int bytes2int(char *buf)
{
    return buf[3] |(buf[2]<<24) | (buf[1]<<16) | (buf[0]);
}

static inline void int2bytes(int i, char *buf)
{
    buf[0] = (i>>24)&0xff;
    buf[1] = (i>>16)&0xff;
    buf[2] = (i>>8)&0xff;
    buf[3] = i&0xff;
}

int setOpt(int fd) {
    struct termios newtio, oldtio;

    if (tcgetattr(fd, &oldtio) != 0) {
        perror("SetupSerial");
        return -1;
    }

    bzero(&newtio, sizeof(newtio));

    newtio.c_lflag &= ~ECHO;
    cfsetispeed(&newtio, BAUDRATE);
    cfsetospeed(&newtio, BAUDRATE);

#if 1
    newtio.c_cflag |= CLOCAL | CREAD;
    newtio.c_cflag &= ~CSIZE;
    newtio.c_cflag |= CS8;
    newtio.c_cflag &= ~PARENB;

    newtio.c_cflag &= ~CSTOPB;

    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 0;
    tcflush(fd, TCIFLUSH);
#endif

    if ((tcsetattr(fd, TCSANOW, &newtio)) != 0) {
        perror("com set error");
        return -1;
    }

    return 0;
}

int readbytes(int fd, char *rcv_buf, int Len)
{
    int ret=0, pos, time=0;
    fd_set rfds;
    struct timeval tv;

    pos = 0;
    while (pos < Len && time <3*Len) {
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = 0;  //set the rcv wait time
        tv.tv_usec = 100*1000;  //100000us = 0.1s

        ret = select(fd + 1, &rfds, NULL, NULL, &tv);

        time ++;

        if (ret == -1){
            perror("select()");
            continue;
        }
        else if (ret) {
            ret = read(fd, rcv_buf+pos, 1);
            //printf("[hardy] ret:%d 0x%02x\n", ret, *(rcv_buf + pos));
            pos++;
        } else {
            //printf("[hardy]: timeout\n");
            continue;
        }
    }

    return pos;
}

int readPckHead(int fd, char *rcv_buf)
{
    int ret, trytimes=3;

    memset(rcv_buf, 0, HEADLEN);
    while (trytimes > 0) {
        trytimes--;
        if (PKG_HEAD != rcv_buf[STX]) {
            ret = readbytes(fd, rcv_buf, 1);
            if (ret < 0) continue;
        }

        if (PKG_HEAD == rcv_buf[STX]) {
            // read package head
            ret = readbytes(fd, &rcv_buf[CMD], HEADLEN-1);
            if (ret < 0) continue;
            if (PKG_TAIL == rcv_buf[ETX]) {
                break;
            }
        }
    }

    //printf("recv package head [0x%02x%02x%02x%02x]\n", rcv_buf[STX], rcv_buf[CMD], rcv_buf[CHKSUM], rcv_buf[ETX]);

    return (PKG_HEAD != rcv_buf[STX] || PKG_TAIL != rcv_buf[ETX]);
}

int sendDataTty(int fd, char *send_buf, int Len)
{
    ssize_t ret;

    ret = write(fd, send_buf, Len);
    if (ret == -1) {
        printf("write device error\n");
        return XS_FAIL;
    }

    return XS_SUCCESS;
}

static inline int sendNoDataCmd(int fd, char cmd)
{
    NoDataCmdPkg.cmd = cmd;
    sendDataTty(fd, (char*)&NoDataCmdPkg, sizeof(NoDataCmdPkg));

    printf("send [0x%02x%02x%02x%02x]\n", NoDataCmdPkg.stx, NoDataCmdPkg.cmd, NoDataCmdPkg.chksum, NoDataCmdPkg.etx);
}

int exeCmdDLoad(int fd, const char *mode)
{
    #define DATA_NUM (0x200)
    char snd_buf[HEADLEN+XS_ADDRESS_LEN+XS_COUNT_LEN+DATA_NUM*XS_PERDATA_LEN] =
        {PKG_HEAD, BM_CMD_DLOAD, 0, PKG_TAIL};

    int sentlen = 0; //
    int sendlen = 0; // data lenght will be sent
    const char* imgData = NULL;
    int datasize = 0;;
    int i, tryTimes, allDataLen;
    char rcv_buf[HEADLEN] = {0};

    if (0 == strcmp(mode, "cvi")) {
        imgData = cviData;
        datasize = sizeof(cviData);
    } else if (0 == strcmp(mode, "tvi")) {
        imgData = tviData;
        datasize = sizeof(tviData);
    } else if (0 == strcmp(mode, "ahd")) {
        imgData = ahdData;
        datasize = sizeof(ahdData);
    } else {
        imgData = patternData;
        datasize = sizeof(patternData);
    }

    while (sentlen < datasize) {
        if (DATA_NUM*XS_PERDATA_LEN < (datasize-sentlen)) {
            sendlen = DATA_NUM*XS_PERDATA_LEN;
        } else {
            sendlen = datasize-sentlen;
        }
        //printf("will send len:0x%x\n", sendlen);

        // address
        int2bytes(IMG_ADDRESS+sentlen, snd_buf+HEADLEN);
        // count
        int2bytes(sendlen/XS_PERDATA_LEN, snd_buf+HEADLEN+XS_ADDRESS_LEN);
        // data
        memcpy(snd_buf+HEADLEN+XS_ADDRESS_LEN+XS_COUNT_LEN,
            imgData+sentlen, sendlen);

        // chksum
        allDataLen = HEADLEN+XS_ADDRESS_LEN+XS_COUNT_LEN+sendlen;
        snd_buf[CHKSUM] = 0;
        for (i=HEADLEN; i<allDataLen; i++) {
            snd_buf[CHKSUM] += snd_buf[i];
            //printf("chksum:0x%02x data:0x%02x\n", snd_buf[CHKSUM], snd_buf[i]);
        }

        //printf("send [0x%02x%02x%02x%02x 0x%02x%02x%02x%02x 0x%02x%02x%02x%02x 0x%02x%02x%02x%02x\n",
//        printf("send [0x%02x%02x%02x%02x 0x%02x%02x%02x%02x 0x%02x%02x%02x%02x\n",
  //          snd_buf[0], snd_buf[1], snd_buf[2], snd_buf[3], // head
    //        snd_buf[4], snd_buf[5], snd_buf[6], snd_buf[7], // address
      //      snd_buf[8], snd_buf[9], snd_buf[10], snd_buf[11]); // count
            //snd_buf[12], snd_buf[13], snd_buf[14], snd_buf[15]); // data

        tryTimes = 3;
        while (tryTimes > 0) {
            if (XS_SUCCESS == sendDataTty(fd, snd_buf, allDataLen)) break;
            tryTimes--;
        }
        if (0 == tryTimes) assert(0);

        tryTimes = 5;
        while (XS_SUCCESS == readPckHead(fd, rcv_buf) ) {
            if (BM_CMD_OK == rcv_buf[CMD]) {
                sentlen += sendlen;
                break;
            } else if (BM_CMD_ERR == rcv_buf[CMD]) {
                break;
            } else {
                tryTimes--;
            }
        }
    }

    return 0;
}

static int sendRunCmd(int fd)
{
    char snd_buf[HEADLEN+XS_ADDRESS_LEN] = {PKG_HEAD, BM_CMD_RUN, 0, PKG_TAIL};
    int i, tryTimes, allDataLen;
    char rcv_buf[HEADLEN] = {0};

    // address
    int2bytes(IMG_ADDRESS, snd_buf+HEADLEN);

    // chksum
    allDataLen = HEADLEN+XS_ADDRESS_LEN;
    snd_buf[CHKSUM] = 0;
    for (i=HEADLEN; i<allDataLen; i++) {
        snd_buf[CHKSUM] += snd_buf[i];
        //printf("chksum:0x%02x data:0x%02x\n", snd_buf[CHKSUM], snd_buf[i]);
    }

    printf("send [0x%02x%02x%02x%02x 0x%02x%02x%02x%02x]\n",
        snd_buf[0], snd_buf[1], snd_buf[2], snd_buf[3], // head
        snd_buf[4], snd_buf[5], snd_buf[6], snd_buf[7]); // address

    tryTimes = 3;
    while (tryTimes > 0) {
        if (XS_SUCCESS == sendDataTty(fd, snd_buf, allDataLen)) break;
        tryTimes--;
    }
    if (0 == tryTimes) assert(0);

    tryTimes = 5;
    while (XS_SUCCESS == readPckHead(fd, rcv_buf) ) {
        if (BM_CMD_OK == rcv_buf[CMD]) {
            break;
        } else if (BM_CMD_ERR == rcv_buf[CMD]) {
            XS_FAIL;
        } else {
            tryTimes--;
            break;
        }
    }

    return XS_SUCCESS;
}

int initXs5013(int fd, const char *mode)
{
    int ret;
    char rcv_buf[HEADLEN];

    while (1) {
        if (XS_SUCCESS == readPckHead(fd, rcv_buf) ) {
    printf("recv package head [0x%02x%02x%02x%02x]\n", rcv_buf[STX], rcv_buf[CMD], rcv_buf[CHKSUM], rcv_buf[ETX]);
            if (BM_CMD_NOTIFY != rcv_buf[CMD]) {
                printf("incorrect command %d\n", rcv_buf[CMD]);
            } else {
                sendNoDataCmd(fd, BM_CMD_DEBUG);
                usleep(100*1000);
                exeCmdDLoad(fd, mode);
                sendRunCmd(fd);
                //printf("xs5013 is ready\n");
                break;
            }
        } else {
            return -1;
        }
    }

    return 0;
}

int main(int argc, char** argv) {
    char *format = NULL;
    int fd = 0;
    char buf[100] = {0};
    int tryTimes;
    char c;

    if (argc < 2) {
        printf("    no parameter -- read serial\n");
        printf("    para1: command\n");
        printf("    ahd/cvi/tvi/pattern -- download img\n");
        printf("    720p25 -- switch video format\n");
        printf("    720p30 -- switch video format\n");
        printf("    720p50 -- switch video format\n");
        printf("    720p60 -- switch video format\n");
        printf("    1080p25 -- switch video format\n");
        printf("    1080p30 -- switch video format\n");
        printf("    1080p50 -- switch video format, only for cvi\n");
        printf("    1080p60 -- switch video format, only for cvi\n");
        printf("    720pal -- switch video format, 720x576-50\n");
        printf("    720n -- switch video format, 720x480-60\n\n");
        printf("    ex: xs5013 ahd\n\n");
        //return 0;
    }

    fd = open(TTY_NAME, O_RDWR | O_NOCTTY | O_NDELAY);
    if (-1 == fd) {
        perror("Can't Open Serial Port");
        return (-1);
    }

    setOpt(fd);

    if (argc >= 2 ) {
        if (0 == strcmp("ahd", argv[1])
            ||0 == strcmp("cvi", argv[1])
            || 0 == strcmp("tvi", argv[1])
            || 0 == strcmp("pattern", argv[1]) ) {
            int ret = initXs5013(fd, argv[1]);

            if (argc == 2) {
                if (0 != ret) return -1;
            } else {
                printf("init %s\n", argv[2]);
                sprintf(buf, "%s\r", argv[2]);
                sendDataTty(fd, buf, strlen(buf));
                printf("init %s end\n", argv[2]);
            }
        } else {
            printf("switch %s\n", argv[1]);

            sprintf(buf, "%s\r", argv[1]);
            sendDataTty(fd, buf, strlen(buf));
        }
    }

    tryTimes = 256;
    while (tryTimes--) {
        if (1 == readbytes(fd, &c, 1)) {
            if (isprint(c) || iscntrl(c)) {
                printf("%c", c);
            } else {
                printf("0x%02x\n", c);
            }
        } else {
            break;
        }
        if ('#' == c) {
            printf("\n");
            break;
        }
    }
    close (fd);

    return 0;
}

