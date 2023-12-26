
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>

#include <linux/console.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clk/sunxi.h>

#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dma/sunxi-dma.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>

#define EXUART_DEV_NAME "um8004"
#define UM8004_UART_DEV_NAME "exuart"

#define MAX_EXUART 3
#define MAX_MSG_SIZE 256
#define UM8004_BASE_MISC_MINOR 250
#define EXUART_TX_BUF_SIZE (CONFIG_UM8004_EXUART_TX_BUF_SIZE*MAX_EXUART)

struct exuart_dev
{
    uint8_t rx_buf[CONFIG_UM8004_EXUART_RX_BUF_SIZE];
    uint32_t rx_rd;
    uint32_t rx_wt;
    wait_queue_head_t wait;

    struct mutex rx_sem;
};

struct  {
    const char *reluart;
    struct file *rfd;
    struct file *wfd;
    char opentimes;

    uint8_t tx_buf[EXUART_TX_BUF_SIZE];
    uint32_t tx_rd;
    uint32_t tx_wt;
    struct mutex tx_sem;

    struct exuart_dev uart[MAX_EXUART];
}exuart_data;

enum {
    PKG_HEAD0,
    PKG_HEAD1,
    PKG_UARTNO,
    PKG_DATALEN,
    PKG_HEADLEN
};

#define MCU_SEND_HEAD0 0x55
#define MCU_SEND_HEAD1 0xAA

#define B200_SEND_HEAD0 0xAA
#define B200_SEND_HEAD1 0x55

static void unwrapper(unsigned char c)
{
    static unsigned char rcv_ind = 0;
    static unsigned char rcv_buf[CONFIG_UM8004_EXUART_RX_BUF_SIZE] = {MCU_SEND_HEAD0, MCU_SEND_HEAD1};

    switch (rcv_ind) {
        case PKG_HEAD0:
            if (MCU_SEND_HEAD0 == c) rcv_ind++;
            break;
        case PKG_HEAD1:
            if (MCU_SEND_HEAD1 == c) {
                rcv_ind++;
            } else {
                rcv_ind = MCU_SEND_HEAD0;
            }
            break;
        case PKG_UARTNO:
            if (c<MAX_EXUART) {
                rcv_buf[PKG_UARTNO] = c;
                rcv_ind++;
            } else {
                rcv_ind = MCU_SEND_HEAD0;
            }
            break;
        case PKG_DATALEN:
            rcv_buf[PKG_DATALEN] = c;
            rcv_ind++;
            break;

        default: // data
            rcv_buf[rcv_ind++] = c;
            if ((rcv_ind - PKG_HEADLEN) >= rcv_buf[PKG_DATALEN]) {
                struct exuart_dev *uart = &exuart_data.uart[rcv_buf[PKG_UARTNO]];

#if 0
{
                int i=0;
                pr_debug("rcv uart%d len[%d] from mcu:", rcv_buf[PKG_UARTNO], rcv_buf[PKG_DATALEN]);
                for (i=0; i<rcv_buf[PKG_DATALEN]; i++) {
                    pr_debug("0x%02x ", rcv_buf[PKG_HEADLEN+i]);
                }
                pr_debug("\n");
}
#endif

                mutex_lock(&uart->rx_sem);

                if ((CONFIG_UM8004_EXUART_TX_BUF_SIZE - uart->rx_wt) < rcv_buf[PKG_DATALEN] ) {
                    int len = CONFIG_UM8004_EXUART_TX_BUF_SIZE - uart->rx_wt;

                    memcpy(uart->rx_buf+uart->rx_wt, rcv_buf+PKG_HEADLEN, len);
                    memcpy(uart->rx_buf, rcv_buf+PKG_HEADLEN+len, rcv_buf[PKG_DATALEN]-len);
                } else {
                    memcpy(uart->rx_buf+uart->rx_wt, rcv_buf+PKG_HEADLEN, rcv_buf[PKG_DATALEN]);
                }
                uart->rx_wt += rcv_buf[PKG_DATALEN];
                uart->rx_wt %= CONFIG_UM8004_EXUART_TX_BUF_SIZE;

                mutex_unlock(&uart->rx_sem);
                wake_up(&uart->wait);

                rcv_ind = PKG_HEAD0;
            }
            break;
    }
}

static int wrapper(const struct exuart_dev *uart, const char *buf, size_t size)
{
    mutex_lock(&exuart_data.tx_sem);

    exuart_data.tx_buf[exuart_data.tx_wt++] = B200_SEND_HEAD0;
    exuart_data.tx_wt %= EXUART_TX_BUF_SIZE;

    exuart_data.tx_buf[exuart_data.tx_wt++] = B200_SEND_HEAD1;
    exuart_data.tx_wt %= EXUART_TX_BUF_SIZE;

    if (uart == &exuart_data.uart[0]) {
        exuart_data.tx_buf[exuart_data.tx_wt++] = 0;
    } else if (uart == &exuart_data.uart[1]) {
        exuart_data.tx_buf[exuart_data.tx_wt++] = 1;
    } else {
        exuart_data.tx_buf[exuart_data.tx_wt++] = 2;
    }
    exuart_data.tx_wt %= EXUART_TX_BUF_SIZE;

    exuart_data.tx_buf[exuart_data.tx_wt++] = size;
    exuart_data.tx_wt %= EXUART_TX_BUF_SIZE;

    if ((EXUART_TX_BUF_SIZE - exuart_data.tx_wt) < size ) {
        int len = EXUART_TX_BUF_SIZE - exuart_data.tx_wt;

        copy_from_user(exuart_data.tx_buf+exuart_data.tx_wt, (void __user *)buf, len);
        copy_from_user(exuart_data.tx_buf, (void __user *)(buf+len), size-len);
    } else {
        //memcpy(exuart_data.tx_buf+exuart_data.tx_wt, buf, size);
        copy_from_user(exuart_data.tx_buf+exuart_data.tx_wt, (void __user *)buf, size);
    }
    exuart_data.tx_wt += size;
    exuart_data.tx_wt %= EXUART_TX_BUF_SIZE;

    mutex_unlock(&exuart_data.tx_sem);

    return 0;
}

static int exuart_proxy_read(void *p)
{
    mm_segment_t old_fs;
    loff_t pos = 0;
    __s32 read_len = 0;
    unsigned char byte = 0;

    while (1) {
        if (NULL == exuart_data.rfd) {
            msleep(5);
            continue;
        }

        old_fs = get_fs();
        set_fs(KERNEL_DS);
        read_len = vfs_read(exuart_data.rfd, (char *)&byte, 1, &pos);
        set_fs(old_fs);

        if (read_len != 1) {
            //pr_debug("%s:%d\n", __func__, __LINE__);
            msleep(1);
            continue;
        }

        //pr_err("0x%02x\n", byte);
        unwrapper(byte);
    }

    return 0;
}

static int exuart_proxy_write(void *p)
{
    mm_segment_t old_fs;
    struct file *fd = NULL;

    // initialize reluart
    msleep(1500);
    old_fs = get_fs();
    fd = filp_open(exuart_data.reluart,O_WRONLY, 0666);
    if (NULL != fd) {
        vfs_write(fd, (const char __user *)"init", 4, &fd->f_pos);
        msleep(100);
        filp_close(fd, NULL);
    }
    set_fs(old_fs);

    while (1) {
        if (exuart_data.tx_rd == exuart_data.tx_wt) {
            msleep(1);
            continue;
        }

        mutex_lock(&exuart_data.tx_sem);

        //pr_debug("b200 back:0x%02x\n", exuart_data.tx_buf[exuart_data.tx_rd]);

        old_fs = get_fs();
        set_fs(KERNEL_DS);
        vfs_write(exuart_data.wfd, &exuart_data.tx_buf[exuart_data.tx_rd++], 1, &exuart_data.wfd->f_pos);
        set_fs(old_fs);

        //write(exuart_data.wfd, &exuart_data.tx_buf[exuart_data.tx_rd++], 1);
        exuart_data.tx_rd %= EXUART_TX_BUF_SIZE;

        mutex_unlock(&exuart_data.tx_sem);
        usleep_range(1, 2);
    }

    return 0;
}

static int exuart_open(struct inode *inode, struct file *filp)
{
    mm_segment_t old_fs;

    filp->private_data = &exuart_data.uart[MINOR(inode->i_rdev)-UM8004_BASE_MISC_MINOR];

    if (NULL == exuart_data.wfd) {
        old_fs = get_fs();
        set_fs(KERNEL_DS);
        exuart_data.rfd = filp_open(exuart_data.reluart,O_RDONLY, 0666);
        exuart_data.wfd = filp_open(exuart_data.reluart,O_WRONLY, 0666);
        set_fs(old_fs);
        //exuart_data.wfd = filp_open(exuart_data.reluart,O_WRONLY,0);
    }

    if (NULL == exuart_data.wfd) {
        pr_debug("%s: open %s fd:%d fail!\n", __func__, exuart_data.reluart, exuart_data.wfd);
        return -EINVAL;
    } else {
        exuart_data.opentimes ++;
    }

    return 0;
}

static int exuart_close(struct inode *inode, struct file *filp)
{
    if (exuart_data.opentimes > 0) exuart_data.opentimes--;
    if (exuart_data.opentimes < 0) exuart_data.opentimes = 0;

    if (exuart_data.opentimes == 0) {
        if (exuart_data.wfd > 0) {

            while (1) {
                mutex_lock(&exuart_data.tx_sem);
                if (exuart_data.tx_rd == exuart_data.tx_wt) {
                    mutex_unlock(&exuart_data.tx_sem);
                    break;
                }

                mutex_unlock(&exuart_data.tx_sem);
                msleep(1);
                continue;
            }
            filp_close(exuart_data.wfd, NULL);
            filp_close(exuart_data.rfd, NULL);
            exuart_data.wfd = exuart_data.rfd = NULL;
        }
    }

    return 0;
}

static ssize_t exuart_read(struct file *filep, char __user *buf, size_t size, loff_t *ppos)
{
    size_t i = 0;
    struct exuart_dev *uart = filep->private_data;

    mutex_lock(&uart->rx_sem);

    for (i = 0; i < size; i++) {
        if (uart->rx_rd == uart->rx_wt)
            break;
        buf[i] = uart->rx_buf[uart->rx_rd++];
        uart->rx_rd %= CONFIG_UM8004_EXUART_RX_BUF_SIZE;
    }

    mutex_unlock(&uart->rx_sem);

    return i;
}

static ssize_t exuart_write(struct file *filep, const char __user *buf, size_t size, loff_t *ppos)
{
    struct exuart_dev *uart = filep->private_data;

    //pr_err("enter %s buf:%s\n", __func__, buf);

    wrapper(uart, buf, size);

    return size;
}

static unsigned int exuart_poll(struct file *filep, poll_table *wait)
{
    int mask = 0;
    struct exuart_dev *uart = filep->private_data;

    poll_wait(filep, &uart->wait, wait);

    if (uart->rx_rd != uart->rx_wt)
        mask |= POLLIN | POLLRDNORM;

    return mask;
}

static const struct file_operations exuart_fops = {
    .open = exuart_open,
    .release = exuart_close,
    .read = exuart_read,
    .write = exuart_write,
    .poll = exuart_poll,
};

static struct miscdevice exuart_port[MAX_EXUART] = {
    {UM8004_BASE_MISC_MINOR, "ttyX0", &exuart_fops},
    {UM8004_BASE_MISC_MINOR+1, "ttyX1", &exuart_fops},
    {UM8004_BASE_MISC_MINOR+2, "ttyX2", &exuart_fops},
};

static int sw_uart_probe(struct platform_device *pdev)
{
    int i;

    pr_err("enter %s\n", __func__);

    memset(&exuart_data, 0, sizeof (exuart_data));
    exuart_data.wfd = exuart_data.rfd = NULL;

    of_property_read_string(pdev->dev.of_node, "reluartdev", &exuart_data.reluart);

    pr_info("expand %s\n", exuart_data.reluart);

    for (i=0; i<MAX_EXUART; i++) {
        struct exuart_dev *uart = &exuart_data.uart[i];

        uart->rx_rd = 0;
        uart->rx_wt = 0;
        mutex_init(&uart->rx_sem);
        init_waitqueue_head(&uart->wait);

        misc_register(&exuart_port[i]);
    }
    mutex_init(&exuart_data.tx_sem);

    kthread_run(exuart_proxy_read, NULL, "exuart_read");
    kthread_run(exuart_proxy_write, NULL, "exuart_write");

    platform_set_drvdata(pdev, &exuart_data);

    pr_debug("quit %s\n", __func__);

    return 0;
}

static int sw_uart_remove(struct platform_device *pdev)
{
    int i = 0;

    for (i=0; i<MAX_EXUART; i++) {
        misc_deregister(&exuart_port[i]);
    }

    return 0;
}

static const struct of_device_id unicmicro_uart_match[] = {
    { .compatible = "unicmicro,um8004", },
    {},
};
MODULE_DEVICE_TABLE(of, sunxi_uart_match);


static struct platform_driver unicmicro_uport_platform_driver = {
    .probe  = sw_uart_probe,
    .remove = sw_uart_remove,
    .driver = {
        .name  = EXUART_DEV_NAME,
        .owner = THIS_MODULE,
        .of_match_table = unicmicro_uart_match,
    },
};

static int __init exuart_driver_init(void)
{
    pr_err("enter %s\n", __func__);

    return platform_driver_register(&unicmicro_uport_platform_driver);
}

static void __exit exuart_driver_exit(void)
{
    pr_info("%s exit\n", __func__);
    platform_driver_unregister(&unicmicro_uport_platform_driver);
}

module_init(exuart_driver_init);
module_exit(exuart_driver_exit);

MODULE_AUTHOR("hardy<yangjz@sanboen.com>");
MODULE_DESCRIPTION("Driver for sanboen exUART controller");
MODULE_LICENSE("GPL");

