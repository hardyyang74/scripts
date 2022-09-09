#include <asm/types.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/socket.h>
#include <stdio.h>
#include <string.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

typedef unsigned char           u8;
typedef unsigned short          u16;
typedef unsigned int            u32;
typedef unsigned long long      u64;
typedef signed char             s8;
typedef short                   s16;
typedef int                     s32;
typedef long long               s64;

#define __force
#define __bitwise
typedef u16 __bitwise be16;
typedef u16 __bitwise __le16;
typedef u32 __bitwise be32;
typedef u32 __bitwise __le32;
typedef u64 __bitwise be64;
typedef u64 __bitwise le64;
#define __constant_htonl(x) ((__force __be32)___constant_swab32((x)))
#define __constant_ntohl(x) ___constant_swab32((__force __be32)(x))
#define __constant_htons(x) ((__force __be16)___constant_swab16((x)))
#define __constant_ntohs(x) ___constant_swab16((__force __be16)(x))
#define __constant_cpu_to_le64(x) ((__force __le64)(__u64)(x))
#define __constant_le64_to_cpu(x) ((__force __u64)(__le64)(x))
#define __constant_cpu_to_le32(x) ((__force __le32)(__u32)(x))
#define __constant_le32_to_cpu(x) ((__force __u32)(__le32)(x))
#define __constant_cpu_to_le16(x) ((__force __le16)(__u16)(x))
#define __constant_le16_to_cpu(x) ((__force __u16)(__le16)(x))
#define __constant_cpu_to_be64(x) ((__force __be64)___constant_swab64((x)))
#define __constant_be64_to_cpu(x) ___constant_swab64((__force __u64)(__be64)(x))
#define __constant_cpu_to_be32(x) ((__force __be32)___constant_swab32((x)))
#define __constant_be32_to_cpu(x) ___constant_swab32((__force __u32)(__be32)(x))
#define __constant_cpu_to_be16(x) ((__force __be16)___constant_swab16((x)))
#define __constant_be16_to_cpu(x) ___constant_swab16((__force __u16)(__be16)(x))
#define __cpu_to_le64(x) ((__force __le64)(__u64)(x))
#define __le64_to_cpu(x) ((__force __u64)(__le64)(x))
#define __cpu_to_le32(x) ((__force __le32)(__u32)(x))
#define __le32_to_cpu(x) ((__force __u32)(__le32)(x))
#define __cpu_to_le16(x) ((__force __le16)(__u16)(x))
#define __le16_to_cpu(x) ((__force __u16)(__le16)(x))
#define __cpu_to_be64(x) ((__force __be64)__swab64((x)))
#define __be64_to_cpu(x) __swab64((__force __u64)(__be64)(x))
#define __cpu_to_be32(x) ((__force __be32)__swab32((x)))
#define __be32_to_cpu(x) __swab32((__force __u32)(__be32)(x))
#define __cpu_to_be16(x) ((__force __be16)__swab16((x)))
#define __be16_to_cpu(x) __swab16((__force __u16)(__be16)(x))
#define cpu_to_le64 __cpu_to_le64
#define le64_to_cpu __le64_to_cpu
#define cpu_to_le32 __cpu_to_le32
#define le32_to_cpu __le32_to_cpu
#define cpu_to_le16 __cpu_to_le16
#define le16_to_cpu __le16_to_cpu
#define cpu_to_be64 __cpu_to_be64
#define be64_to_cpu __be64_to_cpu
#define cpu_to_be32 __cpu_to_be32
#define be32_to_cpu __be32_to_cpu
#define cpu_to_be16 __cpu_to_be16
#define be16_to_cpu __be16_to_cpu
#define cpu_to_le64p __cpu_to_le64p
#define le64_to_cpup __le64_to_cpup
#define cpu_to_le32p __cpu_to_le32p
#define le32_to_cpup __le32_to_cpup
#define cpu_to_le16p __cpu_to_le16p
#define le16_to_cpup __le16_to_cpup
#define cpu_to_be64p __cpu_to_be64p
#define be64_to_cpup __be64_to_cpup
#define cpu_to_be32p __cpu_to_be32p
#define be32_to_cpup __be32_to_cpup
#define cpu_to_be16p __cpu_to_be16p
#define be16_to_cpup __be16_to_cpup
#define cpu_to_le64s __cpu_to_le64s
#define le64_to_cpus __le64_to_cpus
#define cpu_to_le32s __cpu_to_le32s
#define le32_to_cpus __le32_to_cpus
#define cpu_to_le16s __cpu_to_le16s
#define le16_to_cpus __le16_to_cpus
#define cpu_to_be64s __cpu_to_be64s
#define be64_to_cpus __be64_to_cpus
#define cpu_to_be32s __cpu_to_be32s
#define be32_to_cpus __be32_to_cpus
#define cpu_to_be16s __cpu_to_be16s
#define be16_to_cpus __be16_to_cpus

#define	PRINT_MAC_ADDR_STR		"%02x:%02x:%02x:%02x:%02x:%02x"
#define PRINT_MAC_ADDR_VALUE(x)	x[0], x[1], x[2], x[3], x[4], x[5]

struct ieee80211_hdr {
        u16 frame_control;
        u16 duration_id;
        u8 addr1[6];
        u8 addr2[6];
        u8 addr3[6];
        u16 seq_ctrl;
        u8 addr4[6];
} __attribute__ ((__packed__));

/*
 * DS bit usage
 *
 * TA = transmitter address
 * RA = receiver address
 * DA = destination address
 * SA = source address
 *
 * ToDS    FromDS  A1(RA)  A2(TA)  A3      A4      Use
 * -----------------------------------------------------------------
 *  0       0       DA      SA      BSSID   -       IBSS/DLS
 *  0       1       DA      BSSID   SA      -       AP -> STA
 *  1       0       BSSID   SA      DA      -       AP <- STA
 *  1       1       RA      TA      DA      SA      unspecified (WDS)
 */

#define FCS_LEN 4

#define IEEE80211_FCTL_VERS		0x0003
#define IEEE80211_FCTL_FTYPE		0x000c
#define IEEE80211_FCTL_STYPE		0x00f0
#define IEEE80211_FCTL_TODS		0x0100
#define IEEE80211_FCTL_FROMDS		0x0200
#define IEEE80211_FCTL_MOREFRAGS	0x0400
#define IEEE80211_FCTL_RETRY		0x0800
#define IEEE80211_FCTL_PM		0x1000
#define IEEE80211_FCTL_MOREDATA		0x2000
#define IEEE80211_FCTL_PROTECTED	0x4000
#define IEEE80211_FCTL_ORDER		0x8000
#define IEEE80211_FCTL_CTL_EXT		0x0f00

#define IEEE80211_SCTL_FRAG		0x000F
#define IEEE80211_SCTL_SEQ		0xFFF0

#define IEEE80211_FTYPE_MGMT		0x0000
#define IEEE80211_FTYPE_CTL		0x0004
#define IEEE80211_FTYPE_DATA		0x0008
#define IEEE80211_FTYPE_EXT		0x000c

/* management */
#define IEEE80211_STYPE_ASSOC_REQ	0x0000
#define IEEE80211_STYPE_ASSOC_RESP	0x0010
#define IEEE80211_STYPE_REASSOC_REQ	0x0020
#define IEEE80211_STYPE_REASSOC_RESP	0x0030
#define IEEE80211_STYPE_PROBE_REQ	0x0040
#define IEEE80211_STYPE_PROBE_RESP	0x0050
#define IEEE80211_STYPE_BEACON		0x0080
#define IEEE80211_STYPE_ATIM		0x0090
#define IEEE80211_STYPE_DISASSOC	0x00A0
#define IEEE80211_STYPE_AUTH		0x00B0
#define IEEE80211_STYPE_DEAUTH		0x00C0
#define IEEE80211_STYPE_ACTION		0x00D0

/* control */
#define IEEE80211_STYPE_CTL_EXT		0x0060
#define IEEE80211_STYPE_BACK_REQ	0x0080
#define IEEE80211_STYPE_BACK		0x0090
#define IEEE80211_STYPE_PSPOLL		0x00A0
#define IEEE80211_STYPE_RTS		0x00B0
#define IEEE80211_STYPE_CTS		0x00C0
#define IEEE80211_STYPE_ACK		0x00D0
#define IEEE80211_STYPE_CFEND		0x00E0
#define IEEE80211_STYPE_CFENDACK	0x00F0

/* data */
#define IEEE80211_STYPE_DATA			0x0000
#define IEEE80211_STYPE_DATA_CFACK		0x0010
#define IEEE80211_STYPE_DATA_CFPOLL		0x0020
#define IEEE80211_STYPE_DATA_CFACKPOLL		0x0030
#define IEEE80211_STYPE_NULLFUNC		0x0040
#define IEEE80211_STYPE_CFACK			0x0050
#define IEEE80211_STYPE_CFPOLL			0x0060
#define IEEE80211_STYPE_CFACKPOLL		0x0070
#define IEEE80211_STYPE_QOS_DATA		0x0080
#define IEEE80211_STYPE_QOS_DATA_CFACK		0x0090
#define IEEE80211_STYPE_QOS_DATA_CFPOLL		0x00A0
#define IEEE80211_STYPE_QOS_DATA_CFACKPOLL	0x00B0
#define IEEE80211_STYPE_QOS_NULLFUNC		0x00C0
#define IEEE80211_STYPE_QOS_CFACK		0x00D0
#define IEEE80211_STYPE_QOS_CFPOLL		0x00E0
#define IEEE80211_STYPE_QOS_CFACKPOLL		0x00F0

/* extension, added by 802.11ad */
#define IEEE80211_STYPE_DMG_BEACON		0x0000

/* control extension - for IEEE80211_FTYPE_CTL | IEEE80211_STYPE_CTL_EXT */
#define IEEE80211_CTL_EXT_POLL		0x2000
#define IEEE80211_CTL_EXT_SPR		0x3000
#define IEEE80211_CTL_EXT_GRANT	0x4000
#define IEEE80211_CTL_EXT_DMG_CTS	0x5000
#define IEEE80211_CTL_EXT_DMG_DTS	0x6000
#define IEEE80211_CTL_EXT_SSW		0x8000
#define IEEE80211_CTL_EXT_SSW_FBACK	0x9000
#define IEEE80211_CTL_EXT_SSW_ACK	0xa000

size_t
strlcpy(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}

/**
 * ieee80211_is_data - check if type is IEEE80211_FTYPE_DATA
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_data(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_DATA);
}

/**
 * ieee80211_has_a4 - check if IEEE80211_FCTL_TODS and IEEE80211_FCTL_FROMDS are set
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_has_a4(__le16 fc)
{
	__le16 tmp = cpu_to_le16(IEEE80211_FCTL_TODS | IEEE80211_FCTL_FROMDS);
	return (fc & tmp) == tmp;
}

/**
 * ieee80211_has_fromds - check if IEEE80211_FCTL_FROMDS is set
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_has_fromds(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FROMDS)) != 0;
}

/**
 * ieee80211_has_tods - check if IEEE80211_FCTL_TODS is set
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_has_tods(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_TODS)) != 0;
}

/**
 * ieee80211_is_mgmt - check if type is IEEE80211_FTYPE_MGMT
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_mgmt(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_MGMT);
}

/**
 * ieee80211_is_ctl - check if type is IEEE80211_FTYPE_CTL
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_ctl(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_CTL);
}

/**
 * ieee80211_get_SA - get pointer to SA
 * @hdr: the frame
 *
 * Given an 802.11 frame, this function returns the offset
 * to the source address (SA). It does not verify that the
 * header is long enough to contain the address, and the
 * header must be long enough to contain the frame control
 * field.
 */
static inline u8 *ieee80211_get_SA(struct ieee80211_hdr *hdr)
{
	if (ieee80211_has_a4(hdr->frame_control))
		return hdr->addr4;
	if (ieee80211_has_fromds(hdr->frame_control))
		return hdr->addr3;
	return hdr->addr2;
}

/**
 * ieee80211_get_DA - get pointer to DA
 * @hdr: the frame
 *
 * Given an 802.11 frame, this function returns the offset
 * to the destination address (DA). It does not verify that
 * the header is long enough to contain the address, and the
 * header must be long enough to contain the frame control
 * field.
 */
static inline u8 *ieee80211_get_DA(struct ieee80211_hdr *hdr)
{
	if (ieee80211_has_tods(hdr->frame_control))
		return hdr->addr3;
	else
		return hdr->addr1;
}

/* A generic radio capture format is desirable. There is one for
 * Linux, but it is neither rigidly defined (there were not even
 * units given for some fields) nor easily extensible.
 *
 * I suggest the following extensible radio capture format. It is
 * based on a bitmap indicating which fields are present.
 *
 * I am trying to describe precisely what the application programmer
 * should expect in the following, and for that reason I tell the
 * units and origin of each measurement (where it applies), or else I
 * use sufficiently weaselly language ("is a monotonically nondecreasing
 * function of...") that I cannot set false expectations for lawyerly
 * readers.
 */

/*
 * The radio capture header precedes the 802.11 header.
 * All data in the header is little endian on all platforms.
 */
struct ieee80211_radiotap_header {
        u8 it_version;          /* Version 0. Only increases
                                 * for drastic changes,
                                 * introduction of compatible
                                 * new fields does not count.
                                 */
        u8 it_pad;
        __le16 it_len;          /* length of the whole
                                 * header in bytes, including
                                 * it_version, it_pad,
                                 * it_len, and data fields.
                                 */
        __le32 it_present;      /* A bitmap telling which
                                 * fields are present. Set bit 31
                                 * (0x80000000) to extend the
                                 * bitmap by another 32 bits.
                                 * Additional extensions are made
                                 * by setting bit 31.
                                 */
} __packed;

//unsigned short protocol = 0x888e;
unsigned short protocol = 0x0003;
#define NAME "wlan0"
int cc = 0;

static const u_char mcast_key0[] = { 0x01, 0x00, 0x5e, 0x00, 0x48, 0x35 };
static const u_char mcast_key1[] = { 0x01, 0x00, 0x5e, 0x01, 0x68, 0x2b };
static const u_char mcast_key2[] = { 0x01, 0x00, 0x5e, 0x02, 0x5c, 0x31 };
static const u_char mcast_key3[] = { 0x01, 0x00, 0x5e, 0x03, 0x00, 0x00 };

#define MAX_SSID_LEN 16
#define MAX_PSK_LEN  32
#define ADDR_LEN 6

#define KEY_MASK        0x0f
#define KEY0_MASK       (1<<0)
#define KEY1_MASK       (1<<1)
#define KEY2_MASK       (1<<2)
#define KEY3_MASK       (1<<3)

#define plog printf

static struct
{
    char ssid[MAX_SSID_LEN+1];
    char psk[MAX_PSK_LEN+1];
    char mac[ADDR_LEN+1];
    char status;

    int ssidlen;
    int psklen;
} wlan_config;

int get_config_data(struct ieee80211_hdr *hdr)
{
    const char * const data = ieee80211_get_DA(hdr);
    char strdata[ADDR_LEN+1] = {0};
    char strmac[ADDR_LEN+1] = {0};
    int i=0;

    plog("SA " PRINT_MAC_ADDR_STR " -> DA " PRINT_MAC_ADDR_STR "\n", PRINT_MAC_ADDR_VALUE(ieee80211_get_SA(hdr)), PRINT_MAC_ADDR_VALUE(ieee80211_get_DA(hdr)));

    memcpy(strdata, data, ADDR_LEN);
    memcpy(strmac, ieee80211_get_SA(hdr), ADDR_LEN);

    switch (data[3]) {
        case 0:
            if (0 == strcmp(strdata, mcast_key0)) wlan_config.status |= KEY0_MASK;
            if (0x07 == (wlan_config.status&0x07)) {
                strcpy(wlan_config.mac, strmac);
            }
            break;

        case 1:
            if (0 == strcmp(strdata, mcast_key1)) wlan_config.status |= KEY1_MASK;
            if (0x07 == (wlan_config.status&0x07)) {
                strcpy(wlan_config.mac, strmac);
            }
            break;

        case 2:
            if (0 == strcmp(strdata, mcast_key2)) wlan_config.status |= KEY2_MASK;
            if (0x07 == (wlan_config.status&0x07)) {
                strcpy(wlan_config.mac, strmac);
            }
            break;

        case 3:
            if (0x07 == (wlan_config.status&0x07)
                && 0 == strcmp(wlan_config.mac, strmac)) {
                wlan_config.status |= KEY3_MASK;
                wlan_config.ssidlen = data[4];
                wlan_config.psklen = data[5];
            }
            break;

        default:
            if (KEY_MASK == (wlan_config.status&KEY_MASK)
                && 0 == strcmp(wlan_config.mac, strmac) ) {
                int len = wlan_config.ssidlen;
                int ssidok=1, pskok=1;

                if (wlan_config.psklen > len) len = wlan_config.psklen;

                if ((data[3]-4) < len) {
                    wlan_config.ssid[data[3]-4] = data[4];
                    wlan_config.psk[data[3]-4] = data[5];
                }

                for (i=0; i<wlan_config.ssidlen; i++) {
                    if (0 == wlan_config.ssid[i]) {
                        ssidok = 0;
                        break;
                    }
                }

                for (i=0; i<wlan_config.psklen; i++) {
                    if (0 == wlan_config.psk[i]) {
                        pskok = 0;
                        break;
                    }
                }

                if (ssidok && pskok) {
                    printf("mac="PRINT_MAC_ADDR_STR"\nssid=%s\npsk=%s\n",
                        PRINT_MAC_ADDR_VALUE(ieee80211_get_SA(hdr)),
                        wlan_config.ssid, wlan_config.psk);
                    return 1;
                }
            }
            break;
    }

    return 0;
}

int main(int argc, char ** argv)
{
    struct ifreq ifr;
    struct sockaddr_ll ll;
    int fd;

    memset(&wlan_config, 0, sizeof(wlan_config));

    fd = socket(PF_PACKET, SOCK_DGRAM, htons(protocol));
    printf("fd = %d \n", fd);
    if(argv[1])
        printf("name = %s \n", argv[1]);

    memset(&ifr, 0, sizeof(ifr));
    if(argv[1])
        strlcpy(ifr.ifr_name, argv[1], sizeof(ifr.ifr_name));
    else
        strlcpy(ifr.ifr_name, NAME, sizeof(ifr.ifr_name));

    printf("ifr.ifr_name = %s \n", ifr.ifr_name);

    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        close(fd);
        printf("get infr fail\n");
        return -1;
    }

    memset(&ll, 0, sizeof(ll));
    ll.sll_family = PF_PACKET;
    ll.sll_ifindex = ifr.ifr_ifindex;
    ll.sll_protocol = htons(protocol);
    if (bind(fd, (struct sockaddr *) &ll, sizeof(ll)) < 0) {
        printf("bind fail \n");
        close(fd);
        return -1;
    }


    while(1) {

        unsigned char buf[2300];
        int res;
        socklen_t fromlen;
        int i = 0;

        memset(&ll, 0, sizeof(ll));
        fromlen = sizeof(ll);
        res = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *) &ll,
                &fromlen);
        if (res < 0) {
            printf("res < 0\n");
            return -1;
        } else {
        	struct ieee80211_hdr *hdr;
        	struct ieee80211_radiotap_header *rad_hdr;
        	char type[16];
            cc++;
            //printf("pkt %04d( size %03d) - ", cc, res);
            //for(i = 0; i < res && i < 8; i++)

            rad_hdr = (struct ieee80211_radiotap_header *)buf;
            if (rad_hdr->it_version == 0)
            	hdr = (struct ieee80211_hdr *)(buf + rad_hdr->it_len);
            else
            	hdr = (struct ieee80211_hdr *)buf;
            if (ieee80211_is_data(hdr->frame_control)) {
            	strcpy(type, "data");

                u8 *data = ieee80211_get_DA(hdr);
                if (mcast_key0[0] == *data
                    && mcast_key0[1] == *(data+1)
                    && mcast_key0[2] == *(data+2) ) {
                    if (get_config_data(hdr)) break;
                }
            }
            else if (ieee80211_is_mgmt(hdr->frame_control))
            	strcpy(type, "mgmt");
            else if (ieee80211_is_ctl(hdr->frame_control))
            	strcpy(type, "ctl");
            else
            	strcpy(type, "unkown");

#if 0
            printf("type %s - ", type);
            printf("SA " PRINT_MAC_ADDR_STR "-> DA " PRINT_MAC_ADDR_STR " - ", PRINT_MAC_ADDR_VALUE(ieee80211_get_SA(hdr)), PRINT_MAC_ADDR_VALUE(ieee80211_get_DA(hdr)));
    	    for(i = 0; i < res; i++)
                    printf("%02x ", buf[i]);
            printf("\n");
#endif
        }
    }

    close(fd);

    return 0;

}

