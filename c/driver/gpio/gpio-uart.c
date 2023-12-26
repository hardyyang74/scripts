#include <linux/cdev.h>
//#include <linux/device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/ctype.h>
#include <asm/unistd.h>

#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/device.h>

#include <linux/gpio.h>

#include "atlan.h"

#define ATLAN_COMM_NAME "atlanuart"

#define ATLAN_RX_NAME "atlan-rx"
#define ATLAN_TX_NAME "atlan-tx"

#define ELEVEL_TIME_UNIT 750 // us
#define ELEVEL_TIME_DEVIATION 75

#define PRE_CODE_HIGH ELEVEL_TIME_UNIT
#define PRE_CODE_LOW 6750 // ELEVEL_TIME_UNIT * 9

#define CODE_1_HIGH ELEVEL_TIME_UNIT
#define CODE_1_LOW 1500

#define CODE_0_HIGH 1500
#define CODE_0_LOW ELEVEL_TIME_UNIT

struct atlan_comm_priv {
    struct platform_device *pdev;

    dev_t dev;
    struct class *dev_class;
    struct cdev uart;

    u32 rx; // gpio
    u32 tx; // gpio

    struct work_struct work;

    u8 gotcmd[5];
    struct mutex cmd_mutex;
};

static void send_precode(u32 tx)
{
	gpio_direction_output(tx, 1);
    udelay(PRE_CODE_HIGH);

	gpio_direction_output(tx, 0);
    if (PRE_CODE_LOW / 1000) {
        mdelay(PRE_CODE_LOW / 1000);
    }
    udelay(PRE_CODE_LOW % 1000);
}

static void send_cmd(u32 tx, u32 cmd)
{
    u32 data = cmd;
    int i = 0;

    for (i=0; i<32; i++) {
        if (data & 0x01) {
        	gpio_direction_output(tx, 1);
            udelay(CODE_1_HIGH);
        	gpio_direction_output(tx, 0);
            udelay(CODE_1_LOW);
        } else {
        	gpio_direction_output(tx, 1);
            udelay(CODE_0_HIGH);
        	gpio_direction_output(tx, 0);
            udelay(CODE_0_HIGH);
        }

        data >>= 1;
    }
}

static ssize_t atlan_gpio_fread(struct device *dev,
										struct device_attribute *attr, char *buf)
{
    struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct atlan_comm_priv *priv = platform_get_drvdata(pdev);

    mutex_lock(&priv->cmd_mutex);
    if (!priv->gotcmd[4]) {
        mutex_unlock(&priv->cmd_mutex);
        return 0;
    }

    memcpy(buf, priv->gotcmd, 4);
    priv->gotcmd[4] = 0;
    mutex_unlock(&priv->cmd_mutex);

	return 4;
}

static ssize_t atlan_gpio_fwrite(struct device *dev,
										 struct device_attribute *attr,
										 const char *buf, size_t count)
{
    struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct atlan_comm_priv *priv = platform_get_drvdata(pdev);

	pr_info("%s %s\n", __FUNCTION__, buf);
    if (4 != count ) {
    	pr_info("command len is invalid!\n");
        return count;
    }

    send_precode(priv->tx);
    send_cmd(priv->tx, *(u32*)buf);

	return count;
}

static DEVICE_ATTR(atlanuart, 0664, atlan_gpio_fread, atlan_gpio_fwrite);

static struct attribute *atlan_attributes[] = {
	&dev_attr_atlanuart.attr,
	NULL
};

static struct attribute_group atlan_attribute_group = {
	.attrs = atlan_attributes
};

/*
函数功能：获取高电平持续的时间
返 回 值：高电平持续的时间
*/
u32 GetRxH(u32 rx)
{
  ktime_t time_val;
  unsigned int tim_cnt1=0,tim_cnt2=0;
  time_val=ktime_get();       //获取当前时间
  tim_cnt1=ktime_to_us(time_val); //转us
  while(gpio_get_value(rx)){udelay(1);}
  time_val=ktime_get();       //获取当前时间
  tim_cnt2=ktime_to_us(time_val); //转us
  return tim_cnt2-tim_cnt1;
}

/*
函数功能：获取低电平持续的时间
返 回 值：低电平持续的时间
*/
static u32 GetRxL(u32 rx)
{
  ktime_t time_val;
  unsigned int tim_cnt1=0,tim_cnt2=0;
  time_val=ktime_get();       //获取当前时间
  tim_cnt1=ktime_to_us(time_val); //转us
  while(!gpio_get_value(rx)){udelay(1);}
  time_val=ktime_get();       //获取当前时间
  tim_cnt2=ktime_to_us(time_val); //转us
  return tim_cnt2-tim_cnt1;
}

/*
函数功能：工作列队处理函数
1. 先接收引导码：750us高电平+6.750ms低电平
2. 引导码之后，是连续的32位数据
3. 数据‘0’ ：1500us高电平+750us低电平
4. 数据‘1’ ：750us高电平+1500us低电平
*/
static void rx_work_func(struct work_struct * dat)
{
    struct atlan_comm_priv *priv = container_of(dat, struct atlan_comm_priv, work);
    u8 CmdBuff[4]={0}; //存放红外线接收的数据值，其中[4]表示标志位。=0失败，=1成功
    u32 time,j,i;

    /*1. 判断引导码*/
    time=GetRxH(priv->rx); //获取高电平的时间 750us
    if(time<(PRE_CODE_HIGH-ELEVEL_TIME_DEVIATION)||time>(PRE_CODE_HIGH+ELEVEL_TIME_DEVIATION)) {
        goto out;
    }

    time=GetRxL(priv->rx); // 750*9 us
    if(time<(PRE_CODE_LOW-ELEVEL_TIME_DEVIATION)||time>(PRE_CODE_LOW+ELEVEL_TIME_DEVIATION)) {
        goto out;
    }

    /*2. 接收用户码和按键码*/
    for(i=0;i<4;i++)
    {
        u8 data=0;
        for(j=0;j<8;j++)
        {
            int gotbit = 0;

            time=GetRxH(priv->rx);
            if(time<(CODE_1_HIGH-ELEVEL_TIME_DEVIATION)||time>(CODE_1_HIGH+ELEVEL_TIME_DEVIATION)) {
                time=GetRxL(priv->rx);
                if (time>(CODE_1_LOW-ELEVEL_TIME_DEVIATION)) {
                    data = (data<<1) & 0x1;
                    gotbit = 1;
                }
            } else if (time<(CODE_0_HIGH-ELEVEL_TIME_DEVIATION)||time>(CODE_0_HIGH+ELEVEL_TIME_DEVIATION)) {
                time=GetRxL(priv->rx);
                if (time>(CODE_0_LOW-ELEVEL_TIME_DEVIATION)) {
                    data <<= 1;
                    gotbit = 1;
                }
            }

            if (!gotbit) {
                goto out;
            }
        }
        CmdBuff[i]=data;
    }

    //打印解码数据
    printk("got cmd:0x%02x%02x%02x%02x\n",CmdBuff[0],CmdBuff[1],CmdBuff[2],CmdBuff[3]);

    mutex_lock(&priv->cmd_mutex);
    memcpy(priv->gotcmd, CmdBuff, 4);
    priv->gotcmd[4] = 1;
    mutex_unlock(&priv->cmd_mutex);

out:
    //使能中断
    enable_irq(gpio_to_irq(priv->rx) );
}

static irqreturn_t atlan_interrupt(int irq, void *dev_id)
{
	struct atlan_comm_priv *priv = dev_id;

	pr_info("%s 0x%p\n", __FUNCTION__, priv);

    /* close irq */
    disable_irq_nosync(irq);

    /*工作队列调度函数*/
    schedule_work(&priv->work);

	return IRQ_HANDLED;
}

/*
** This fuction will be called when we open the Device file
*/
static int uart_open(struct inode *inode, struct file *file)
{
    struct atlan_comm_priv *priv = container_of(inode->i_cdev, struct atlan_comm_priv, uart);
    file->private_data = priv;

    printk(KERN_INFO "Driver Open Function Called...!!!\n");
    return 0;
}
/*
* This fuction will be called when we close the Device file
*/
static int uart_release(struct inode *inode, struct file *file)
{
        printk(KERN_INFO "Driver Release Function Called...!!!\n");
        return 0;
}

/*
* This fuction will be called when we read the Device file
*/
static ssize_t uart_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    struct atlan_comm_priv *priv = (struct atlan_comm_priv *) filp->private_data;

    if (len % 4 ) {
        pr_info("command len:%d is invalid!\n", len);
        return 0;
    }

    mutex_lock(&priv->cmd_mutex);
    if (!priv->gotcmd[4]) {
        mutex_unlock(&priv->cmd_mutex);
        return 0;
    }

    if (copy_to_user(buf, priv->gotcmd, 4) ){
    }
    priv->gotcmd[4] = 0;
    mutex_unlock(&priv->cmd_mutex);

    return 4;
}

/*
* This fuction will be called when we write the Device file
*/
static ssize_t uart_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
    struct atlan_comm_priv *priv = (struct atlan_comm_priv *) filp->private_data;
    int data = 0;

    if (4 != len ) {
        pr_info("command len is invalid!\n");
        return len;
    }

    if (copy_from_user(&data,buf,len) ) {
    }

    send_precode(priv->tx);
    send_cmd(priv->tx, data);

    return len;
}

static struct file_operations uart_fops =
{
    .owner      = THIS_MODULE,
    .read       = uart_read,
    .write      = uart_write,
    .open       = uart_open,
    .release    = uart_release,
};

static int atlan_probe(struct platform_device *pdev)
{
	struct atlan_comm_priv *priv;
	int err;

	priv = kzalloc(sizeof(struct atlan_comm_priv), GFP_KERNEL);
	if (!priv) {
		return -ENOMEM;
	}
    memset(priv, 0, sizeof(struct atlan_comm_priv));

	platform_set_drvdata(pdev, priv);
    mutex_init(&priv->cmd_mutex);

	if (0 != of_property_read_u32(pdev->dev.of_node, ATLAN_RX_NAME, &priv->rx)) {
		dev_err(&pdev->dev, "ERROR: get rx fail!\n");
		goto err_readconfig;
	}
	if (0 != of_property_read_u32(pdev->dev.of_node, ATLAN_TX_NAME, &priv->tx)) {
		dev_err(&pdev->dev, "ERROR: get tx fail!\n");
		goto err_readconfig;
	}
	pr_info("%s 0x%p rx:%d tx:%d\n", __FUNCTION__, priv, priv->rx, priv->tx);

	gpio_request(priv->tx, ATLAN_TX_NAME);
	gpio_request(priv->rx, ATLAN_RX_NAME);
	gpio_direction_output(priv->tx, 0);
	gpio_direction_input(priv->rx);

    err = request_irq(gpio_to_irq(priv->rx), atlan_interrupt, IRQF_TRIGGER_RISING | IRQF_ONESHOT,
        pdev->dev.driver->name, priv);
    if (err < 0)
    {
        dev_err(&pdev->dev, "atlan_probe: request irq failed code:%d\n", err);
        goto err_readconfig;
    }

    err = sysfs_create_group(&pdev->dev.kobj, &atlan_attribute_group);
	if (0 != err) {
		dev_err(&pdev->dev, "%s() - ERROR: sysfs_create_group() failed.error code: %d\n", __FUNCTION__, err);
        goto err_readconfig;
	}

    INIT_WORK(&priv->work, rx_work_func);

    /*Allocating Major number*/
    if((alloc_chrdev_region(&priv->dev, 0, 1, "test_tools")) <0){
        dev_err(&pdev->dev, "Cannot allocate major number\n");
        goto del_att;
    }
    printk(KERN_INFO "Major = %d Minor = %d \n",MAJOR(priv->dev), MINOR(priv->dev));

    /*Creating cdev structure*/
    cdev_init(&priv->uart,&uart_fops);

    /*Adding character device to the system*/
    if((cdev_add(&priv->uart,priv->dev,1)) < 0){
        dev_err(&pdev->dev, "Cannot add the device to the system\n");
        goto del_chrdev;
    }

    /*Creating struct class*/
    if((priv->dev_class = class_create(THIS_MODULE,"test_class")) == NULL){
        dev_err(&pdev->dev, "Cannot create the struct class\n");
        goto del_cdev;
    }

    /*Creating device*/
    if((device_create(priv->dev_class,NULL,priv->dev,NULL,ATLAN_COMM_NAME)) == NULL){
        dev_err(&pdev->dev, "Cannot create the Device 1\n");
        goto del_class;
    }
    printk(KERN_INFO "Device Driver Insert...Done!!!\n");

    return 0;

del_class:
    class_destroy(priv->dev_class);

del_cdev:
    cdev_del(&priv->uart);

del_chrdev:
    unregister_chrdev_region(priv->dev,1);

del_att:
    sysfs_remove_group(&pdev->dev.kobj, &atlan_attribute_group);

err_readconfig:
    kfree(priv);
    return -EINVAL;
}

static int atlan_remove(struct platform_device *pdev)
{
	struct atlan_comm_priv *priv = platform_get_drvdata(pdev);

    device_destroy(priv->dev_class,priv->dev);
    class_destroy(priv->dev_class);
    cdev_del(&priv->uart);
    unregister_chrdev_region(priv->dev, 1);

    disable_irq_nosync(gpio_to_irq(priv->rx));
    free_irq(gpio_to_irq(priv->rx), priv);
    sysfs_remove_group(&pdev->dev.kobj, &atlan_attribute_group);
    mutex_destroy(&priv->cmd_mutex);

    if (priv) {
        kfree(priv);
    }
    return 0;
}

#if defined (CONFIG_OF)
static const struct of_device_id atlan_of_match[] = {
	{.compatible = "atlan,comm", },
	{ },
};
MODULE_DEVICE_TABLE(of, atlan_of_match);
#endif

static struct platform_driver atlan_comm_driver = {
	.probe = atlan_probe,
    .remove = atlan_remove,
	.driver = {
		.name	= ATLAN_COMM_NAME,
#if defined (CONFIG_OF)
		.of_match_table = of_match_ptr(atlan_of_match),
#endif
	},
};

module_platform_driver(atlan_comm_driver);

MODULE_DESCRIPTION("altan communication driver");
MODULE_AUTHOR("sanbeon");
MODULE_LICENSE("GPL v2");

