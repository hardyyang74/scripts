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

#define TTY_NAME "/dev/ttyX2"
#define BAUDRATE B115200

#define XS_SUCCESS 0
#define XS_FAIL 1

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
        tv.tv_usec = 5*1000;  //100000us = 0.1s

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

int sendData(int fd, char *send_buf, int Len)
{
    ssize_t ret;

    ret = write(fd, send_buf, Len);
    if (ret == -1) {
        printf("write device error\n");
        return XS_FAIL;
    }

    return XS_SUCCESS;
}

int main(int argc, char** argv) {
    char *format = NULL;
    int fd = 0, fd2 = 0;
    char buf[1024] = {0};
    int tryTimes;
    char c;

    if (argc < 2) {
        printf("    no parameter -- read serial\n");
        //return 0;
    }

    fd = open(TTY_NAME, O_RDWR | O_NOCTTY | O_NDELAY);
    if (-1 == fd) {
        perror("Can't Open Serial Port");
        return (-1);
    }

#if 0
    if (0)
    {
        fd2 = open("/dev/ttyS2", O_RDWR | O_NOCTTY | O_NDELAY);
        setOpt(fd2);
        close (fd2);
    }
#endif

#if 0
    buf[0] = 0xAA;
    buf[1] = 0x55;
    buf[2] = 0x0;
    buf[3] = strlen("Are you muart0?");
    strcpy(buf+4, "Are you muart0?");
    sendData(fd, buf, buf[3]+4);
#else
//    strcpy(buf, "Are you muart0?");
  //  sendData(fd, buf, strlen("Are you muart0?"));
#endif

#if 1
    tryTimes = 0x0fffffff;
    strcpy(buf, "Are you muart3?");
    int chlen = strlen(buf);

    int needsend = 0;
    char *tmp = buf;

    while (tryTimes--) {
        if (1 == readbytes(fd, &c, 1)) {
            if (isprint(c) || iscntrl(c)) {
                putchar(c);
                needsend++;
                //putchar('\n');
            } else {
                printf("0x%02x\n", c);
            }
        } else {
            //putchar('\n');
            while (needsend > 0) {
                //printf("needsend:%d\n", needsend);
                if (((tmp-buf) + needsend) <= chlen) {
                    sendData(fd, tmp, needsend);
                    tmp += needsend;
                    needsend = 0;
                } else {
                    int sent = chlen-(tmp-buf);
                    sendData(fd, tmp, sent);
                    tmp += sent;
                    needsend -= sent;
                }
                if (tmp >= buf+chlen) tmp=buf;
                //printf("needsend2:%d\n", needsend);
            }
        }
    }
#endif
    close (fd);

    return 0;
}

