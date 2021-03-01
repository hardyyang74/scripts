#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <fcntl.h>
#include <unistd.h>

#include <termios.h> //set baud rate

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>

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

// hexdump -v -e '4/1 "0x%02x, " "\n"' miniboot_reload_dh5021a.img > imgData
char imgData[] = {
#include "imgData"
};

#if 0
int readDataTty(int fd, char *rcv_buf, int TimeOut, int Len)
{
    int retval;
    fd_set rfds;
    struct timeval tv;
    int ret, pos;
    tv.tv_sec = TimeOut / 1000;  //set the rcv wait time
    tv.tv_usec = TimeOut % 1000 * 1000;  //100000us = 0.1s

    pos = 0;
    while (1) {
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        retval = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (retval == -1)
        {
            perror("select()");
            break;
        }
        else if (retval)
        {
            ret = read(fd, rcv_buf + pos, 1);
            if (-1 == ret)
            {
                perror("read()");
                break;
            }
            printf("[hardy] %c\n", *(rcv_buf + pos));

            pos++;
            if (Len <= pos)
            {
                break;
            }
        }
        else
        {
            printf("[hardy: %s\n", strerror(errno));
            break;
        }
    }

    return pos;
}
#endif

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
    int ret=0, pos;
    fd_set rfds;
    struct timeval tv;

    pos = 0;
    while (pos < Len) {
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = 1;  //set the rcv wait time
        tv.tv_usec = 0;  //100000us = 0.1s

        ret = select(fd + 1, &rfds, NULL, NULL, &tv);

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

    return 0;
}

int readPckHead(int fd, char *rcv_buf)
{
    int ret, trytimes=20;

    memset(rcv_buf, 0, HEADLEN);
    while (trytimes > 0) {
        if (PKG_HEAD != rcv_buf[STX]) {
            ret = readbytes(fd, rcv_buf, 1);
        }

        if (PKG_HEAD == rcv_buf[STX]) {
            // read package head
            ret = readbytes(fd, &rcv_buf[CMD], HEADLEN-1);
            if (PKG_TAIL == rcv_buf[ETX]) {
                break;
            }
        }
    }

    printf("recv package head [0x%02x%02x%02x%02x]\n", rcv_buf[STX], rcv_buf[CMD], rcv_buf[CHKSUM], rcv_buf[ETX]);

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

    printf("send [0x%02x%02x%02x%02x\n", NoDataCmdPkg.stx, NoDataCmdPkg.cmd, NoDataCmdPkg.chksum, NoDataCmdPkg.etx);
}

int exeCmdDLoad(int fd)
{
    #define DATA_NUM (0x40)
    char snd_buf[HEADLEN+XS_ADDRESS_LEN+XS_COUNT_LEN+DATA_NUM*XS_PERDATA_LEN] =
        {PKG_HEAD, BM_CMD_DLOAD, 0, PKG_TAIL};

    int sentlen = 0; //
    int sendlen = 0; // data lenght will be sent
    int datasize = sizeof(imgData);
    int i, tryTimes, allDataLen;
    char rcv_buf[HEADLEN] = {0};

    while (sentlen < datasize) {
        if (DATA_NUM*XS_PERDATA_LEN < (datasize-sentlen)) {
            sendlen = DATA_NUM*XS_PERDATA_LEN;
        } else {
            sendlen = datasize-sentlen;
        }
        printf("will send len:0x%x\n", sendlen);

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
        printf("send [0x%02x%02x%02x%02x 0x%02x%02x%02x%02x 0x%02x%02x%02x%02x\n",
            snd_buf[0], snd_buf[1], snd_buf[2], snd_buf[3], // head
            snd_buf[4], snd_buf[5], snd_buf[6], snd_buf[7], // address
            snd_buf[8], snd_buf[9], snd_buf[10], snd_buf[11]); // count
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

    printf("send [0x%02x%02x%02x%02x 0x%02x%02x%02x%02x\n",
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

void initXs5013(int fd)
{
    int ret;
    char rcv_buf[HEADLEN];

    while (1) {
        if (XS_SUCCESS == readPckHead(fd, rcv_buf) ) {
            if (BM_CMD_NOTIFY != rcv_buf[CMD]) {
                printf("incorrect command %d\n", rcv_buf[CMD]);
            } else {
                sendNoDataCmd(fd, BM_CMD_DEBUG);
                usleep(100*1000);
                exeCmdDLoad(fd);
                sendRunCmd(fd);
                //printf("xs5013 is ready\n");
                break;
            }
        }
    }
}

int main(int argc, char** argv) {
    char *format = NULL;
    int fd = 0;
    char buf[100] = {0};

    if (argc < 2) {
        printf("invalid parameter\n");
        printf("  para1: command\n");
        printf("    init -- download img\n");
        printf("    ahd720p60 -- switch video format\n");
        printf("    hdcvi720p60 -- switch video format\n");
        printf("    tvi720p60 -- switch video format\n\n");
        printf("  ex: xs5013 init\n");
        return 0;
    }

    fd = open(TTY_NAME, O_RDWR | O_NOCTTY | O_NDELAY);
    if (-1 == fd) {
        perror("Can't Open Serial Port");
        return (-1);
    }

    setOpt(fd);

//    sprintf(buf, "imgLen:%d\n", sizeof(imgData));
//    sendDataTty(fd, buf, strlen(buf));

    if (0 == strcmp("init", argv[1])) {
        initXs5013(fd);
        if (argc >= 3) {
            printf("switch %s\n", argv[2]);
            sendDataTty(fd, argv[2], strlen(argv[2]));
        }
    } else {
        printf("switch %s\n", argv[1]);
        sendDataTty(fd, argv[1], strlen(argv[1]));
    }

    while (1) {
        char c;
        readbytes(fd, &c, 1);
        printf("%c", c);
        sleep(1);
    }

    close (fd);

    return 0;
}

