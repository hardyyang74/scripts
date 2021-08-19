#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

/*
struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
};
#define EV_KEY                  0x01
*/
int reportkey(int fd, uint16_t type, uint16_t keycode, int32_t value)
{
    ssize_t ret = 0;
	struct input_event event;

	event.type = type;
	event.code = keycode;
	event.value = value;

	gettimeofday(&event.time, 0);

	if (write(fd, &event, sizeof(struct input_event)) < 0) {
		printf("report key error!\n");
		return -1;
	}

    memset(&event, 0, sizeof(event));
	gettimeofday(&event.time, 0);
    ret = write(fd, &event, sizeof(struct input_event));

	return 0;
}

void showHelp() {
    printf("sendevent key [down/up]\n\n");
    printf("  key: power(p)\n");
    printf("       ok\n");
    printf("       menu(m)\n");
    printf("       plus(+)\n");
    printf("       minus(-)\n");
    printf("       esc(e)\n");
    printf("       an integer\n\n");
}

//#define DEVNAME "/dev/input/event4"

#define KEYDOWN	1
#define KEYUP	0

int main(int argc, char *argv[])
{
	uint16_t keycode;
	int k_fd;
    char *keyevent = NULL;

    keyevent = getenv("KEYEVENT");
    if (NULL == keyevent) {
        printf("please set envirment \"KEYEVENT\"\n");
        printf("  for example: export KEYEVENT=/dev/input/event2\n");
        return 1;
    }

    if (argc < 2) {
        showHelp();
        return 2;
    }

	k_fd = open(keyevent, O_WRONLY);

	if (k_fd < 0) {
		printf("open error! %s\n", strerror(errno));
		return k_fd;
	}

    keycode = atoi(argv[1]);
    if (0 == keycode) {
        if (0 == strcmp("m", argv[1]) || 0 == strcmp("menu", argv[1])) {
        	keycode = KEY_MENU;
        } else if (0 == strcmp("+", argv[1]) ||0 == strcmp("plus", argv[1]) ) {
        	keycode = KEY_KPPLUS;
        } else if (0 == strcmp("-", argv[1]) ||0 == strcmp("minus", argv[1]) ) {
        	keycode = KEY_KPMINUS;
        } else if (0 == strcmp("p", argv[1]) ||0 == strcmp("power", argv[1]) ) {
        	keycode = KEY_POWER;
        } else if (0 == strcmp("e", argv[1]) ||0 == strcmp("esc", argv[1]) ) {
        	keycode = KEY_ESC;
        } else { // ok
        	keycode = KEY_ENTER;
        }
    }

    if (3 == argc) {
        if (0 == strcmp("down", argv[2])) {
        	reportkey(k_fd, EV_KEY, keycode, KEYDOWN);
        } else {
        	reportkey(k_fd, EV_KEY, keycode, KEYUP);
        }
    } else {
    	reportkey(k_fd, EV_KEY, keycode, KEYDOWN);
        usleep(50*1000);
    	reportkey(k_fd, EV_KEY, keycode, KEYUP);
    }

	close(k_fd);

	return 0;
}
