/*
 * Copyright (C) 2017 NXP Semiconductor, Inc. All Rights Reserved.
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
//#include <linux/of_gpio.h>
//#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/fsl_devices.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <linux/regulator/consumer.h>
#include <linux/fsl_devices.h>
#include <mach/mipi_csi2.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-int-device.h>
#include "mxc_v4l2_capture.h"


#define MAX_SENSOR_NUM	4

unsigned int g_tp2854_width = 1280;
unsigned int g_tp2854_height = 720;

unsigned int g_sensor_num = 4;

/*!
 * Maintains the information on the current state of the sesor.
 */
static struct sensor_data tp2854_data[MAX_SENSOR_NUM];

static int tp2854_probe(struct i2c_client *adapter,
				const struct i2c_device_id *device_id);
static int tp2854_remove(struct i2c_client *client);

static int ioctl_dev_init(struct v4l2_int_device *s);

static const struct i2c_device_id tp2854_id[] = {
	{"tp2854_mipi", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, tp2854_id);

static struct i2c_driver tp2854_i2c_driver = {
	.driver = {
		  .owner = THIS_MODULE,
		  .name  = "tp2854_mipi",
		  },
	.probe  = tp2854_probe,
	.remove = tp2854_remove,
	.id_table = tp2854_id,
};

/*! Read one register from a TP2854 i2c slave device.
 *
 *  @param *reg		register in the device we wish to access.
 *
 *  @return		       0 if success, an error code otherwise.
 */
static inline int tp2854_read_reg(u8 reg)
{
	int val;

	val = i2c_smbus_read_byte_data(tp2854_data[0].i2c_client, reg);
	if (val < 0) {
		dev_info(&tp2854_data[0].i2c_client->dev,
			"%s:read reg error: reg=%2x\n", __func__, reg);
		return -1;
	}
	return val;
}

/*! Write one register of a TP2854 i2c slave device.
 *
 *  @param *reg		register in the device we wish to access.
 *
 *  @return		       0 if success, an error code otherwise.
 */
static int tp2854_write_reg(u8 reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(tp2854_data[0].i2c_client, reg, val);
	if (ret < 0) {
		dev_info(&tp2854_data[0].i2c_client->dev,
			"%s:write reg error:reg=%2x,val=%2x\n", __func__,
			reg, val);
		return -1;
	}
	return 0;
}
/*
static void tp2854_dump_registers(void)
{
	unsigned char i;
	for (i=0; i<0x72; i++)
		printk("TP2854 Reg 0x%02x = 0x%x.\r\n", i, tp2854_read_reg(i));
}
*/

#define TVI_25FPS 	0
#define TVI_30FPS 	0

#define AHD_25FPS 	1
#define AHD_30FPS 	0

static int tp2854_hardware_init(struct sensor_data *sensor)
{
	int retval = 0;
	void *mipi_csi2_info;
	u32 mipi_reg;
	int i, lanes;

	pr_info("tp2854_mipi: sensor number = %d.\n", g_sensor_num);

	if (g_sensor_num == 0) {
		pr_err("%s: no camera connected.\n", __func__);
		return -1;
	}
#if  TVI_25FPS
	tp2854_write_reg(0x40, 0x04);
	tp2854_write_reg(0x02, 0xc2);
	//tp2854_write_reg(0x1c, 0x06);  //1280*720, 30fps
	//tp2854_write_reg(0x1d, 0x72);  //1280*720, 30fps
	tp2854_write_reg(0x1c, 0x07);  //1280*720, 25fps
	tp2854_write_reg(0x1d, 0xbc);  //1280*720, 25fps
	tp2854_write_reg(0x4e, 0x00);
	tp2854_write_reg(0xf5, 0xf0);
#endif	

#if  TVI_30FPS
	tp2854_write_reg(0x40, 0x04);
	tp2854_write_reg(0x02, 0xc2);
	tp2854_write_reg(0x1c, 0x06);  //1280*720, 30fps
	tp2854_write_reg(0x1d, 0x72);  //1280*720, 30fps
	//tp2854_write_reg(0x1c, 0x07);  //1280*720, 25fps
	//tp2854_write_reg(0x1d, 0xbc);  //1280*720, 25fps
	tp2854_write_reg(0x4e, 0x00);
	tp2854_write_reg(0xf5, 0xf0);
#endif	



#if AHD_25FPS
	tp2854_write_reg(0x40, 0x04);
	tp2854_write_reg(0x02, 0xc2);
	//tp2854_write_reg(0x1c, 0x06);  //1280*720, 30fps
	//tp2854_write_reg(0x1d, 0x72);  //1280*720, 30fps
	tp2854_write_reg(0x1c, 0x07); //1280*720, 25fps 
	tp2854_write_reg(0x1d, 0xbc); //1280*720, 25fps 
	tp2854_write_reg(0x4e, 0x00);
	tp2854_write_reg(0xf5, 0xf0);


	tp2854_write_reg(0x14, 0x40);
	tp2854_write_reg(0x16, 0x16);
	tp2854_write_reg(0x21, 0x46);  
	tp2854_write_reg(0x2d, 0x48);  
	tp2854_write_reg(0x2e, 0x40);  
	tp2854_write_reg(0x30, 0x27);  
	tp2854_write_reg(0x31, 0x88);
	tp2854_write_reg(0x32, 0x04);
	tp2854_write_reg(0x33, 0x20);
	
#endif

#if AHD_30FPS
		tp2854_write_reg(0x40, 0x04);
		tp2854_write_reg(0x02, 0xc2);
		tp2854_write_reg(0x1c, 0x06);  //1280*720, 30fps
		tp2854_write_reg(0x1d, 0x72);  //1280*720, 30fps
		//tp2854_write_reg(0x1c, 0x07);  //1280*720, 25fps
		//tp2854_write_reg(0x1d, 0xbc);  //1280*720, 25fps
		tp2854_write_reg(0x4e, 0x00);
		tp2854_write_reg(0xf5, 0xf0);
	
	
		tp2854_write_reg(0x14, 0x40);
		tp2854_write_reg(0x16, 0x16);
		tp2854_write_reg(0x21, 0x46);  
		tp2854_write_reg(0x2d, 0x48);  
		tp2854_write_reg(0x2e, 0x40);  
		tp2854_write_reg(0x30, 0x27);  
		tp2854_write_reg(0x31, 0x72);
		tp2854_write_reg(0x32, 0x80);
		tp2854_write_reg(0x33, 0x70);		
#endif



	tp2854_write_reg(0x2a, 0x34);

	tp2854_write_reg(0x40, 0x0c);
	tp2854_write_reg(0x01, 0xf8);
	tp2854_write_reg(0x02, 0x01);
	tp2854_write_reg(0x08, 0x0f);
	tp2854_write_reg(0x13, 0x24);
	tp2854_write_reg(0x14, 0x04);
	tp2854_write_reg(0x15, 0x00);
	tp2854_write_reg(0x20, 0x44);
	tp2854_write_reg(0x34, 0x1b);

	/* Disable MIPI CSI2 output */
	tp2854_write_reg(0x23, 0x02);

	mipi_csi2_info = mipi_csi2_get_info();

	/* initial mipi dphy */
	if (!mipi_csi2_info) {
		printk(KERN_ERR "%s() in %s: Fail to get s_mipi_csi2_info!\n",
		       __func__, __FILE__);
		return -1;
	}

	if (!mipi_csi2_get_status(mipi_csi2_info))
		mipi_csi2_enable(mipi_csi2_info);

	if (!mipi_csi2_get_status(mipi_csi2_info)) {
		pr_err("Can not enable mipi csi2 driver!\n");
		return -1;
	}

	lanes = mipi_csi2_set_lanes(mipi_csi2_info);

	/* Only reset MIPI CSI2 HW at sensor initialize */
	/* 37.125MHz pixel clock (1280*720@30fps) * 16 bits per pixel (YUV422) = 594Mbps mipi data rate for each camera */
	mipi_csi2_reset(mipi_csi2_info, (594 * g_sensor_num) / (lanes + 1));

	if (sensor->pix.pixelformat == V4L2_PIX_FMT_UYVY) {
		for (i=0; i<MAX_SENSOR_NUM; i++)
			mipi_csi2_set_datatype(mipi_csi2_info, i, MIPI_DT_YUV422);
	} else
		pr_err("currently this sensor format can not be supported!\n");

/*
	printk("Dump TP2854 registers:\r\n");
	tp2854_dump_registers();
*/

	/* Enable MIPI CSI2 output */
	tp2854_write_reg(0x23, 0x00);
	msleep(100);

	if (mipi_csi2_info) {
		i = 0;

		/* wait for mipi sensor ready */
		mipi_reg = mipi_csi2_dphy_status(mipi_csi2_info);
		while (((mipi_reg & 0x700) != 0x300) && (i < 10)) {
//		while (((mipi_reg & 0x700) != 0x300) && (i < 10000)) {
			mipi_reg = mipi_csi2_dphy_status(mipi_csi2_info);
			i++;
			msleep(50);
		}

		if (i >= 10) {
//		if (i >= 10000) {
			pr_err("mipi csi2 can not receive sensor clk! MIPI_CSI_PHY_STATE = 0x%x.\n", mipi_reg);
			return -1;
		}

		i = 0;

		/* wait for mipi stable */
		mipi_reg = mipi_csi2_get_error1(mipi_csi2_info);
		while ((mipi_reg != 0x0) && (i < 10)) {
			mipi_reg = mipi_csi2_get_error1(mipi_csi2_info);
			i++;
			msleep(10);
		}

		if (i >= 10) {
			pr_err("mipi csi2 can not reveive data correctly! MIPI_CSI_ERR1 = 0x%x.\n", mipi_reg);
			return -1;
		}
	}

	return retval;
}

/* --------------- IOCTL functions from v4l2_int_ioctl_desc --------------- */

static int ioctl_g_ifparm(struct v4l2_int_device *s, struct v4l2_ifparm *p)
{
	struct sensor_data *sensor;

	if (s == NULL) {
		pr_err("   ERROR!! no slave device set!\n");
		return -1;
	}
	sensor = s->priv;

	memset(p, 0, sizeof(*p));
	p->u.bt656.clock_curr = sensor->mclk;
	pr_debug("   clock_curr=mclk=%d\n", sensor->mclk);
	p->if_type = V4L2_IF_TYPE_BT656;
	p->u.bt656.mode = V4L2_IF_TYPE_BT656_MODE_NOBT_8BIT;
	p->u.bt656.bt_sync_correct = 1;  /* Indicate external vsync */

	return 0;
}

/*!
 * ioctl_s_power - V4L2 sensor interface handler for VIDIOC_S_POWER ioctl
 * @s: pointer to standard V4L2 device structure
 * @on: indicates power mode (on or off)
 *
 * Turns the power on or off, depending on the value of on and returns the
 * appropriate error code.
 */
static int ioctl_s_power(struct v4l2_int_device *s, int on)
{
	struct sensor_data *sensor = s->priv;

	sensor->on = on;

	return 0;
}

/*!
 * ioctl_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ioctl_g_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct sensor_data *sensor = s->priv;
	struct v4l2_captureparm *cparm = &a->parm.capture;
	int ret = 0;

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		memset(a, 0, sizeof(*a));
		a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cparm->capability = sensor->streamcap.capability;
		cparm->timeperframe = sensor->streamcap.timeperframe;
		cparm->capturemode = sensor->streamcap.capturemode;
		ret = 0;
		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		ret = -EINVAL;
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*!
 * ioctl_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int ioctl_s_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	int ret = 0;

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		pr_debug("   type is not " \
			"V4L2_BUF_TYPE_VIDEO_CAPTURE but %d\n",
			a->type);
		ret = -EINVAL;
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*!
 * ioctl_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int ioctl_g_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	struct sensor_data *sensor = s->priv;

	f->fmt.pix = sensor->pix;

	return 0;
}

static int ioctl_try_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	struct sensor_data *sensor = s->priv;

	if (sensor->i2c_client != NULL) {
		g_tp2854_width =  f->fmt.pix.width;
		g_tp2854_height =  f->fmt.pix.height;
	}
	sensor->pix.width = g_tp2854_width;
	sensor->pix.height = g_tp2854_height;

	ioctl_dev_init(s);
	return 0;
}

/*!
 * ioctl_g_ctrl - V4L2 sensor interface handler for VIDIOC_G_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_G_CTRL ioctl structure
 *
 * If the requested control is supported, returns the control's current
 * value from the video_control[] array.  Otherwise, returns -EINVAL
 * if the control is not supported.
 */
static int ioctl_g_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int ret = 0;

	ret = -EINVAL;

	return ret;
}

/*!
 * ioctl_s_ctrl - V4L2 sensor interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW (and updates the video_control[] array).  Otherwise,
 * returns -EINVAL if the control is not supported.
 */
static int ioctl_s_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int ret = 0;
	return ret;
}

/*!
 * ioctl_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fsize: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ioctl_enum_framesizes(struct v4l2_int_device *s,
				 struct v4l2_frmsizeenum *fsize)
{
	struct sensor_data *sensor = s->priv;

	if (fsize->index > 0)
		return -EINVAL;

	fsize->pixel_format = sensor->pix.pixelformat;
	fsize->discrete.width = sensor->pix.width;
	fsize->discrete.height = sensor->pix.height;
	return 0;
}

/*!
 * ioctl_enum_frameintervals - V4L2 sensor interface handler for
 *			       VIDIOC_ENUM_FRAMEINTERVALS ioctl
 * @s: pointer to standard V4L2 device structure
 * @fival: standard V4L2 VIDIOC_ENUM_FRAMEINTERVALS ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ioctl_enum_frameintervals(struct v4l2_int_device *s,
					 struct v4l2_frmivalenum *fival)
{
	if (fival->index > 0)
		return -EINVAL;

	if (fival->pixel_format == 0 || fival->width == 0 ||
			fival->height == 0) {
		pr_warning("Please assign pixelformat, width and height.\n");
		return -EINVAL;
	}

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = 30;

	return 0;
}

/*!
 * ioctl_g_chip_ident - V4L2 sensor interface handler for
 *			VIDIOC_DBG_G_CHIP_IDENT ioctl
 * @s: pointer to standard V4L2 device structure
 * @id: pointer to int
 *
 * Return 0.
 */
static int ioctl_g_chip_ident(struct v4l2_int_device *s, int *id)
{
	((struct v4l2_dbg_chip_ident *)id)->match.type =
					V4L2_CHIP_MATCH_I2C_DRIVER;
	strcpy(((struct v4l2_dbg_chip_ident *)id)->match.name,
		"ovtp2854_mipi_deseiralizer");

	return 0;
}

/*!
 * ioctl_init - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 */
static int ioctl_init(struct v4l2_int_device *s)
{
	return 0;
}

/*!
 * ioctl_enum_fmt_cap - V4L2 sensor interface handler for VIDIOC_ENUM_FMT
 * @s: pointer to standard V4L2 device structure
 * @fmt: pointer to standard V4L2 fmt description structure
 *
 * Return 0.
 */
static int ioctl_enum_fmt_cap(struct v4l2_int_device *s,
			      struct v4l2_fmtdesc *fmt)
{
	struct sensor_data *sensor = s->priv;

	if (fmt->index > 0) /* only 1 pixelformat support so far */
		return -EINVAL;

	fmt->pixelformat = sensor->pix.pixelformat;

	return 0;
}

/*!
 * ioctl_dev_init - V4L2 sensor interface handler for vidioc_int_dev_init_num
 * @s: pointer to standard V4L2 device structure
 *
 * Initialise the device when slave attaches to the master.
 */
static int ioctl_dev_init(struct v4l2_int_device *s)
{
	struct sensor_data *sensor = s->priv;
	int ret = 0;
	void *mipi_csi2_info;

	sensor->on = true;

	if (sensor->i2c_client != NULL) {
		mipi_csi2_info = mipi_csi2_get_info();

		/* enable mipi csi2 */
		if (mipi_csi2_info)
			mipi_csi2_enable(mipi_csi2_info);
		else {
			printk(KERN_ERR "%s() in %s: Fail to get mipi_csi2_info!\n",
			       __func__, __FILE__);
			return -EPERM;
		}

		//ret = tp2854_hardware_init(sensor);
		tp2854_hardware_init(sensor);
	}

	return ret;
}

/*!
 * ioctl_dev_exit - V4L2 sensor interface handler for vidioc_int_dev_exit_num
 * @s: pointer to standard V4L2 device structure
 *
 * Delinitialise the device when slave detaches to the master.
 */
static int ioctl_dev_exit(struct v4l2_int_device *s)
{
	struct sensor_data *sensor = s->priv;
	void *mipi_csi2_info;

	if (sensor->i2c_client != NULL) {
		mipi_csi2_info = mipi_csi2_get_info();

		/* disable mipi csi2 */
		if (mipi_csi2_info)
			if (mipi_csi2_get_status(mipi_csi2_info))
				mipi_csi2_disable(mipi_csi2_info);
	}

	return 0;
}

/*!
 * This structure defines all the ioctls for this module and links them to the
 * enumeration.
 */
static struct v4l2_int_ioctl_desc tp2854_ioctl_desc[] = {
	{vidioc_int_dev_init_num, (v4l2_int_ioctl_func *) ioctl_dev_init},
	{vidioc_int_dev_exit_num, ioctl_dev_exit},
	{vidioc_int_s_power_num, (v4l2_int_ioctl_func *) ioctl_s_power},
	{vidioc_int_g_ifparm_num, (v4l2_int_ioctl_func *) ioctl_g_ifparm},
/*	{vidioc_int_g_needs_reset_num,
				(v4l2_int_ioctl_func *)ioctl_g_needs_reset}, */
/*	{vidioc_int_reset_num, (v4l2_int_ioctl_func *)ioctl_reset}, */
	{vidioc_int_init_num, (v4l2_int_ioctl_func *) ioctl_init},
	{vidioc_int_enum_fmt_cap_num,
				(v4l2_int_ioctl_func *) ioctl_enum_fmt_cap},
	{vidioc_int_try_fmt_cap_num,
				(v4l2_int_ioctl_func *)ioctl_try_fmt_cap},
	{vidioc_int_g_fmt_cap_num, (v4l2_int_ioctl_func *) ioctl_g_fmt_cap},
/*	{vidioc_int_s_fmt_cap_num, (v4l2_int_ioctl_func *) ioctl_s_fmt_cap}, */
	{vidioc_int_g_parm_num, (v4l2_int_ioctl_func *) ioctl_g_parm},
	{vidioc_int_s_parm_num, (v4l2_int_ioctl_func *) ioctl_s_parm},
/*	{vidioc_int_queryctrl_num, (v4l2_int_ioctl_func *)ioctl_queryctrl}, */
	{vidioc_int_g_ctrl_num, (v4l2_int_ioctl_func *) ioctl_g_ctrl},
	{vidioc_int_s_ctrl_num, (v4l2_int_ioctl_func *) ioctl_s_ctrl},
	{vidioc_int_enum_framesizes_num,
				(v4l2_int_ioctl_func *) ioctl_enum_framesizes},
	{vidioc_int_enum_frameintervals_num,
				(v4l2_int_ioctl_func *) ioctl_enum_frameintervals},
	{vidioc_int_g_chip_ident_num,
				(v4l2_int_ioctl_func *) ioctl_g_chip_ident},
};

static struct v4l2_int_slave tp2854_slave[MAX_SENSOR_NUM] = {
	{
	.ioctls = tp2854_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(tp2854_ioctl_desc),
	},

	{
	.ioctls = tp2854_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(tp2854_ioctl_desc),
	},

	{
	.ioctls = tp2854_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(tp2854_ioctl_desc),
	},

	{
	.ioctls = tp2854_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(tp2854_ioctl_desc),
	}
};

static struct v4l2_int_device tp2854_int_device[MAX_SENSOR_NUM] = {
	{
		.module = THIS_MODULE,
		.name = "tp2854",
		.type = v4l2_int_type_slave,
		.u = {
			.slave = &tp2854_slave[0],
		},
	}, 

	{
		.module = THIS_MODULE,
		.name = "tp2854",
		.type = v4l2_int_type_slave,
		.u = {
			.slave = &tp2854_slave[1],
		},
	}, 

	{
		.module = THIS_MODULE,
		.name = "tp2854",
		.type = v4l2_int_type_slave,
		.u = {
			.slave = &tp2854_slave[2],
		},
	}, 

	{
		.module = THIS_MODULE,
		.name = "tp2854",
		.type = v4l2_int_type_slave,
		.u = {
			.slave = &tp2854_slave[3],
		},
	}
};

/*!
 * tp2854 I2C probe function
 *
 * @param adapter            struct i2c_adapter *
 * @return  Error code indicating success or failure
 */
 #if 0
static int tp2854_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	int retval;

	/* Set initial values for the sensor struct. */
	memset(&tp2854_data[0], 0, sizeof(tp2854_data[0]));
	tp2854_data[0].sensor_clk = devm_clk_get(dev, "csi_mclk");
	if (IS_ERR(tp2854_data[0].sensor_clk)) {
		/* assuming clock enabled by default */
		tp2854_data[0].sensor_clk = NULL;
		dev_err(dev, "clock-frequency missing or invalid\n");
		return PTR_ERR(tp2854_data[0].sensor_clk);
	}

	retval = of_property_read_u32(dev->of_node, "mclk",
					&(tp2854_data[0].mclk));
	if (retval) {
		dev_err(dev, "mclk missing or invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "mclk_source",
					(u32 *) &(tp2854_data[0].mclk_source));
	if (retval) {
		dev_err(dev, "mclk_source missing or invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "csi_id",
					&(tp2854_data[0].csi));
	if (retval) {
		dev_err(dev, "csi id missing or invalid\n");
		return retval;
	}

	clk_prepare_enable(tp2854_data[0].sensor_clk);

	tp2854_data[0].i2c_client = client;
	tp2854_data[0].pix.pixelformat = V4L2_PIX_FMT_UYVY;
	tp2854_data[0].pix.width = g_tp2854_width;
	tp2854_data[0].pix.height = g_tp2854_height;
	tp2854_data[0].streamcap.capturemode = 0;
	tp2854_data[0].streamcap.timeperframe.denominator = 30;
	tp2854_data[0].streamcap.timeperframe.numerator = 1;
	tp2854_data[0].is_mipi = 1;

	retval = tp2854_read_reg(0xfe);
	printk("REG[0xFE] = 0x%x.\r\n", retval);
	retval = tp2854_read_reg(0xff);
	printk("REG[0xFF] = 0x%x.\r\n", retval);
/*
	retval = tp2854_read_reg(0x1e);
	if (retval != 0x40) {
		pr_warning("tp2854 is not found, chip id reg 0x1e = 0x%x.\n", retval);
		clk_disable_unprepare(tp2854_data[0].sensor_clk);
		return -ENODEV;
	}
*/
	memcpy(&tp2854_data[1], &tp2854_data[0], sizeof(struct sensor_data));
	memcpy(&tp2854_data[2], &tp2854_data[0], sizeof(struct sensor_data));
	memcpy(&tp2854_data[3], &tp2854_data[0], sizeof(struct sensor_data));

	tp2854_data[1].i2c_client = NULL;
	tp2854_data[2].i2c_client = NULL;
	tp2854_data[3].i2c_client = NULL;

	tp2854_data[0].ipu_id = 0;
	tp2854_data[0].csi = 0;
	tp2854_data[0].v_channel = 0;

	tp2854_data[1].ipu_id = 0;
	tp2854_data[1].csi = 1;
	tp2854_data[1].v_channel = 1;

	tp2854_data[2].ipu_id = 1;
	tp2854_data[2].csi = 0;
	tp2854_data[2].v_channel = 2;

	tp2854_data[3].ipu_id = 1;
	tp2854_data[3].csi = 1;
	tp2854_data[3].v_channel = 3;

	tp2854_int_device[0].priv = &tp2854_data[0];
	tp2854_int_device[1].priv = &tp2854_data[1];
	tp2854_int_device[2].priv = &tp2854_data[2];
	tp2854_int_device[3].priv = &tp2854_data[3];
	v4l2_int_device_register(&tp2854_int_device[0]);
	v4l2_int_device_register(&tp2854_int_device[1]);
	v4l2_int_device_register(&tp2854_int_device[2]);
	retval = v4l2_int_device_register(&tp2854_int_device[3]);

	clk_disable_unprepare(tp2854_data[0].sensor_clk);

	pr_info("tp2854_mipi is found\n");
	return retval;
}
#endif

static int tp2854_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct fsl_mxc_camera_platform_data *plat_data = client->dev.platform_data;
	int retval;


	/* Set initial values for the sensor struct. */
	memset(&tp2854_data[0], 0, sizeof(tp2854_data[0]));

#if 0
	tp2854_data[0].sensor_clk = devm_clk_get(dev, "csi_mclk");
	if (IS_ERR(tp2854_data[0].sensor_clk)) {
		/* assuming clock enabled by default */
		tp2854_data[0].sensor_clk = NULL;
		dev_err(dev, "clock-frequency missing or invalid\n");
		return PTR_ERR(tp2854_data[0].sensor_clk);
	}
#endif	

	//retval = of_property_read_u32(dev->of_node, "mclk",
			//		&(tp2854_data[0].mclk));
	//if (retval) {
	//	dev_err(dev, "mclk missing or invalid\n");
	//	return retval;
	//}
	tp2854_data[0].mclk=plat_data->mclk;
	
	

	//retval = of_property_read_u32(dev->of_node, "mclk_source",
		//			(u32 *) &(tp2854_data[0].mclk_source));
	//if (retval) {
	//	dev_err(dev, "mclk_source missing or invalid\n");
	//	return retval;
	//}
	tp2854_data[0].mclk_source = plat_data->mclk_source;

	

	//retval = of_property_read_u32(dev->of_node, "csi_id",
	//				&(tp2854_data[0].csi));
	//if (retval) {
	//	dev_err(dev, "csi id missing or invalid\n");
	//	return retval;
	//}



//sensor_clk ???
	//clk_prepare_enable(tp2854_data[0].sensor_clk);

	tp2854_data[0].i2c_client = client;
	tp2854_data[0].pix.pixelformat = V4L2_PIX_FMT_UYVY;
	tp2854_data[0].pix.width = g_tp2854_width;
	tp2854_data[0].pix.height = g_tp2854_height;
	tp2854_data[0].streamcap.capturemode = 0;
	tp2854_data[0].streamcap.timeperframe.denominator = 30;
	tp2854_data[0].streamcap.timeperframe.numerator = 1;
	tp2854_data[0].is_mipi = 1;

	retval = tp2854_read_reg(0xfe);

	retval = tp2854_read_reg(0xff);
	printk("REG[0xFF] = 0x%x.\r\n", retval);

	retval = tp2854_read_reg(0x1e);
	if (retval <0 ) { // != 0x40) {
		pr_warning("tp2854 is not found, chip id reg 0x1e = 0x%x.\n", retval);
		//clk_disable_unprepare(tp2854_data[0].sensor_clk);
		return -ENODEV;
	}

	memcpy(&tp2854_data[1], &tp2854_data[0], sizeof(struct sensor_data));
	memcpy(&tp2854_data[2], &tp2854_data[0], sizeof(struct sensor_data));
	memcpy(&tp2854_data[3], &tp2854_data[0], sizeof(struct sensor_data));

	tp2854_data[1].i2c_client = NULL;
	tp2854_data[2].i2c_client = NULL;
	tp2854_data[3].i2c_client = NULL;

	tp2854_data[0].ipu_id = 0;
	tp2854_data[0].csi = 0;
	tp2854_data[0].v_channel = 0;

	tp2854_data[1].ipu_id = 0;
	tp2854_data[1].csi = 1;
	tp2854_data[1].v_channel = 1;

	tp2854_data[2].ipu_id = 1;
	tp2854_data[2].csi = 0;
	tp2854_data[2].v_channel = 2;

	tp2854_data[3].ipu_id = 1;
	tp2854_data[3].csi = 1;
	tp2854_data[3].v_channel = 3;

	tp2854_int_device[0].priv = &tp2854_data[0];
	tp2854_int_device[1].priv = &tp2854_data[1];
	tp2854_int_device[2].priv = &tp2854_data[2];
	tp2854_int_device[3].priv = &tp2854_data[3];
	v4l2_int_device_register(&tp2854_int_device[0]);
	v4l2_int_device_register(&tp2854_int_device[1]);
	v4l2_int_device_register(&tp2854_int_device[2]);
	retval = v4l2_int_device_register(&tp2854_int_device[3]);

	//clk_disable_unprepare(tp2854_data[0].sensor_clk);

	pr_info("tp2854_mipi is found\n");
	return retval;
}


/*!
 * tp2854 I2C detach function
 *
 * @param client            struct i2c_client *
 * @return  Error code indicating success or failure
 */
static int tp2854_remove(struct i2c_client *client)
{
	v4l2_int_device_unregister(&tp2854_int_device[3]);
	v4l2_int_device_unregister(&tp2854_int_device[2]);
	v4l2_int_device_unregister(&tp2854_int_device[1]);
	v4l2_int_device_unregister(&tp2854_int_device[0]);

	return 0;
}

/*!
 * tp2854 init function
 *
 * @return  Error code indicating success or failure
 */
static __init int tp2854_init(void)
{
	u8 err;

	printk("KKKLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL\n");
	err = i2c_add_driver(&tp2854_i2c_driver);
	if (err != 0)
		pr_err("%s:driver registration failed, error=%d\n",
			__func__, err);

	return err;
}

/*!
 * TP2854 cleanup function
 *
 * @return  Error code indicating success or failure
 */
static void __exit tp2854_clean(void)
{
	i2c_del_driver(&tp2854_i2c_driver);
}

module_init(tp2854_init);
module_exit(tp2854_clean);

MODULE_AUTHOR("NXP Semiconductor, Inc.");
MODULE_DESCRIPTION("TP2854 HD CVBS Encoder Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_ALIAS("CSI");
