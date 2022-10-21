#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <pthread.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <netinet/ether.h>
#include <wpa_ctrl.h>

#include <iostream>
#include <string>
#include <vector>

using std::string;
using std::vector;
using std::cout;
using std::endl;

#define U_SUCCESS 0
#define U_FAIL (-1)

#define MAX_RECV_SIZE 4096

#define SERVER_PORT 1234
#define SERVER_IP "192.168.88.1"

#define PING_TIMEOUT 5000000 // 3s
#define plog printf

typedef struct
{
    int sock;
    pid_t pid;
}SOCK_INFO;

typedef struct
{
    int no;
    struct timeval t;
} PINGINFO;

typedef struct
{
    int sentCount;
    int sock;
    char s[18];
}RECVINFO;

static vector<SOCK_INFO> sockinfo;
static vector<PINGINFO> vecPing;
static pthread_t recv_thread = -1;

void showHelp()
{
    printf(
        "demo: testwifi -s 192.168.88.1\n"
        "\t-s I am a server\n"
        "\t-t ping ip\n"
        "\t-p use the port of server/client\n"
        "\t-d test wlan0 status\n"
        "\t-c get the count of linking into hotspot\n"
        "");
}

static int add_socket(int sock)
{
    SOCK_INFO socknode = {sock, -1};

    sockinfo.push_back(socknode);

#if 0
    for (vector<SOCK_INFO>::iterator it = sockinfo.begin(); it != sockinfo.end(); ++it)
    {
        cout << "sockinfo pid:" << it->pid << " sock:" << it->sock << endl;
    }
#endif

    return 1;
}

static int getsocks(fd_set *fdsr)
{
    int maxsock = 0;

    for (vector<SOCK_INFO>::iterator it = sockinfo.begin(); it != sockinfo.end(); ++it)
    {
        FD_SET(it->sock, fdsr);
        if (it->sock > maxsock)
        {
            maxsock = it->sock;
        }
    }

    return maxsock;
}

static int readsock(fd_set *fdsr)
{
    for (vector<SOCK_INFO>::iterator it = sockinfo.begin(); it != sockinfo.end();)
    {
        if (FD_ISSET(it->sock, fdsr))
        {
            int iDataNum = 0;
            char recvbuf[MAX_RECV_SIZE];

            iDataNum = recv(it->sock, recvbuf, sizeof(PINGINFO), 0);

            //cout << "read sock: " << it->sock << " len: " << iDataNum << endl;
            if (0 == iDataNum) {
                // client is disconnected
                it = sockinfo.erase(it);
                continue;
            } else if (sizeof(PINGINFO) == iDataNum) {
                send(it->sock, recvbuf, sizeof(PINGINFO), 0);
            } else {
            }
        }

        it++;
    }
}

static void createServer(string ipStr)
{
    int ret, i;
    int svr_sock, clt_sock, maxsock;
    struct sockaddr_in svr_addr, clt_addr;
    int addr_len = sizeof(clt_addr);
    fd_set fdsr;
    struct timeval tv;

    if((svr_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("create socket error: %s\n", strerror(errno));
        return;
    }

    memset(&svr_addr, 0, sizeof(struct sockaddr_in));
    svr_addr.sin_family = AF_INET;
    inet_aton(ipStr.c_str(), &svr_addr.sin_addr);
    svr_addr.sin_port = htons(SERVER_PORT);
    if(bind(svr_sock, (struct sockaddr *)&svr_addr, sizeof(svr_addr)) < 0)
    {
        printf("socket bind: %s\n", strerror(errno));
        return;
    }

    if(listen(svr_sock, 20) < 0)
    {
        printf("socket listen: %s\n", strerror(errno));
        return;
    }
    printf("Listening on port: %d\n", SERVER_PORT);

    while (1)
    {
        FD_ZERO(&fdsr);
        maxsock = getsocks(&fdsr);
        FD_SET(svr_sock, &fdsr);
        if (svr_sock > maxsock) maxsock = svr_sock;

        tv.tv_sec = 6;
        tv.tv_usec = 0;

        ret = select(maxsock + 1, &fdsr, NULL, NULL, &tv);

        if (ret < 0) {
            printf("socket select: %s\n", strerror(errno));
            assert(0);
        } else if (ret == 0) {
            //printf("socket select timeout \n");
            continue;
        }

        readsock(&fdsr);

        //printf("try accept\n");
        if (FD_ISSET(svr_sock, &fdsr))
        {
            clt_sock = accept(svr_sock, (struct sockaddr*)&clt_addr, (socklen_t*)&addr_len);
            if(clt_sock < 0)
            {
                printf("socket accept: %s\n", strerror(errno));
                continue;
            }

            //printf("IP is %s\n", inet_ntoa(clt_addr.sin_addr));
            printf("connected, client %s port is %d\n", inet_ntoa(clt_addr.sin_addr), ntohs(clt_addr.sin_port));
            add_socket(clt_sock);
        }
    }

    close(svr_sock);
}

static int connect_server(string server, int port)
{
    int clientSocket;
    struct sockaddr_in serverAddr;

    if((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("create socket error: %s\n", strerror(errno));
        return U_FAIL;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    //serverAddr.sin_addr.s_addr = inet_addr(server.c_str());
    inet_aton(server.c_str(), &serverAddr.sin_addr);
    if(connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        printf("socket connect error: %s\n", strerror(errno));
        return U_FAIL;
    }

    return clientSocket;
}

static void* recv_ping(void* arg)
{
    RECVINFO *info = (RECVINFO*)arg;
    int sock = info->sock;
    PINGINFO recvbuf;
    int iDataNum = 0;
    struct timeval time_now = {0};
    unsigned int recvtim = 0;

    int ret;
    fd_set fdsr;
    struct timeval tv;

    while (1)
    {
        FD_ZERO(&fdsr);
        FD_SET(sock, &fdsr);

        tv.tv_sec = 0;
        tv.tv_usec = 50*1000;
        ret = select(sock + 1, &fdsr, NULL, NULL, &tv);

        if (ret < 0) {
            printf("socket select: %s\n", strerror(errno));
            assert(0);
        } else if (ret == 0) {
            //printf("socket select timeout \n");
            continue;
        }

        iDataNum = recv(sock, &recvbuf, sizeof(PINGINFO), 0);
        //printf("[hardy] recv msg len:%d\n", iDataNum);
        if (0 == iDataNum)
        {
            // server is disconnected
            break;
        }

        gettimeofday(&time_now,NULL);
        recvtim = time_now.tv_sec*1000000 + time_now.tv_usec
            - recvbuf.t.tv_sec*1000000 - recvbuf.t.tv_usec;
        printf("%d bytes from %s: tcp_seq=%d time=%d.%dms\n",
            sizeof(PINGINFO), info->s, recvbuf.no,
            recvtim/1000, recvtim%1000);

        for (vector<PINGINFO>::iterator it = vecPing.begin(); it != vecPing.end(); )
        {
            if (it->no == recvbuf.no) {
                it = vecPing.erase(it);
                continue;
            } else if (it->no < recvbuf.no) {
                gettimeofday(&time_now,NULL);

                int duration = time_now.tv_sec*1000000 + time_now.tv_usec
                    - it->t.tv_sec*1000000 - it->t.tv_usec;

                printf("tcp_seq %d is lost, time: %d.%dms\n",
                    it->no, duration/1000, duration%1000);

                it = vecPing.erase(it);
                continue;
            }
            it++;
        }
    }
}

void detect_hotspot()
{
    int fd, retval;
    char buf[MAX_RECV_SIZE] = {0};
    int len = MAX_RECV_SIZE;
    struct sockaddr_nl addr;
    struct nlmsghdr *nh;
    struct ifinfomsg *ifinfo;
    struct rtattr *attr;

    fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &len, sizeof(len));
    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTNLGRP_LINK;
    bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    while ((retval = read(fd, buf, MAX_RECV_SIZE)) > 0)
    {
        //plog("retval = %d \n", retval);
        for (nh = (struct nlmsghdr *)buf; NLMSG_OK(nh, retval); nh = NLMSG_NEXT(nh, retval))
        {
            plog("[hardy] nlmsg_type:%d\n", nh->nlmsg_type);
            //struct sockaddr_in *addr = (struct sockaddr_in*)nh->msg_name;

            if (nh->nlmsg_type == NLMSG_DONE)
                break;
            else if (nh->nlmsg_type == NLMSG_ERROR)
                return;
            else if (nh->nlmsg_type != RTM_NEWLINK)
                continue;

            ifinfo = (struct ifinfomsg*)NLMSG_DATA(nh);
            plog("[hardy] ifi_index:%d ifi_flags:0x%x\n", ifinfo->ifi_index, ifinfo->ifi_flags);

            attr = (struct rtattr*)(((char*)nh) + NLMSG_SPACE(sizeof(*ifinfo)));
            len = nh->nlmsg_len - NLMSG_SPACE(sizeof(*ifinfo));
            for (; RTA_OK(attr, len); attr = RTA_NEXT(attr, len))
            {
                //plog("[hardy] rta_type:%d\n", attr->rta_type);
                switch (attr->rta_type) {
                    default:
                        //printf(" %s", (char*)RTA_DATA(attr));
                        break;

                    case IFLA_IFNAME:
                        plog(" %s", (char*)RTA_DATA(attr));
                        plog(" %s", (ifinfo->ifi_flags & IFF_UP) ? "up" : "down");
                        plog(" %s", (ifinfo->ifi_flags & IFF_RUNNING) ? "linked" : "unlink");
                        break;
                    case IFLA_ADDRESS:
                        //plog(" %s", ether_ntoa((struct ether_addr*)RTA_DATA(attr)));
                        break;

                    case IFLA_AF_SPEC:
                        break;
                }
            }
            printf("\n");
        }
    }
}

static int strcpyline(char *line, int line_index, char *src)
{
    int i = 0;
    char *p = src;
    char *q = line;

    while (*p && i != line_index ) {
        if (*p == '\n') {
            i++;
        }
        p++;
    }

    if (*p) {
        while (*p && *p != '\n') {
            *q = *p;
            p++;
            q++;
        }
        *q = '\0';
    } else {
        return -1;
    }

    return 0;
}

int wifi_ap_get_count(const char * const hotspot)
{
    struct wpa_ctrl *wpa_ctrl = wpa_ctrl_open(hotspot);

    if (!wpa_ctrl) {
        printf("Err: wpa_ctrl_open()\n");
        return -1;
    }

    int cnt = 0;
    int ret = 0;
    char cmd[32];
    char reply[4096];
    size_t len;
    char addr[32];

    strcpy(cmd, "STA-FIRST");
    len = 4095;
    ret = wpa_ctrl_request(wpa_ctrl, cmd, strlen(cmd), reply, &len, 0);

    if (ret == 0 && len > 0 && strncmp("FAIL", reply, 4) != 0) {
        if (strcpyline(addr, 0, reply) < 0) {
            ret = -1;
            goto ERROR;
        }

        cnt++;
    } else {
        ret = -1;
        goto ERROR;
    }

    while (1) {
        sprintf(cmd, "STA-NEXT %s", addr);
        len = 4095;
        ret = wpa_ctrl_request(wpa_ctrl, cmd, strlen(cmd), reply, &len, 0);
        if (strcpyline(addr, 0, reply) < 0) {
            break;
        }

        /*
         * ret == 0 && len == 0 已经没有更多数据了
         */
        if (ret ==0 && len > 0 && strncmp("FAIL", reply, 4) != 0) {
            cnt++;
        } else {
            break;
        }
    }

ERROR:
    wpa_ctrl_close(wpa_ctrl);
    printf("clients number:%d\n", cnt);
    return ret;
}

int main(int argc, char** argv)
{
    int result = 0;
    string ipStr = SERVER_IP;
    int port = SERVER_PORT;

    int sock = 0;
    RECVINFO threadArg = {0, 0};

    if (1 == argc) {
        printf("pls input parameter!\n");
        showHelp();
        return 0;
    }

    while( (result = getopt(argc, argv, "hc:ds:t:p:")) != -1 ) {
        switch (result) {
            default:
            case 'h':
                showHelp();
                return 0;

            case 'c':
                wifi_ap_get_count(optarg);
                return 0;

            case 'd':
                detect_hotspot();
                return 0;

            case 's':
                ipStr = optarg;
                createServer(ipStr);
                return 0;

            case 't':
                ipStr = optarg;
                //printf("option=t, optarg=%s\n", ipStr.c_str());
                break;

            case 'p':
                port = atoi(optarg);
                //printf("option=p, optarg=%d\n", port);
                break;
        }
    }

    printf("connecting to %s:%d\n", ipStr.c_str(), port);
    if ((sock = connect_server(ipStr, port)) < 0 )
    {
        printf("connect server fail!\n");
        return -1;
    }

    threadArg.sentCount = 0;
    threadArg.sock = sock;
    strcpy(threadArg.s, ipStr.c_str());
    pthread_create (&recv_thread, NULL, recv_ping, &threadArg);

    while (1) {
        PINGINFO *sendbuf = (PINGINFO*)malloc(sizeof(PINGINFO));

        threadArg.sentCount++;

        sendbuf->no = threadArg.sentCount;
        gettimeofday(&sendbuf->t,NULL);
        send(sock, sendbuf, sizeof(PINGINFO), 0);

        vecPing.push_back(*sendbuf);

        sleep(3);
    }

    if (-1 != recv_thread)
    {
        pthread_join(recv_thread, NULL);
        recv_thread = -1;
    }

    return 0;
}

