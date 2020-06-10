/*
 * A V4L2 driver for nvp6234_mipi Raw cameras.
 *
 * Copyright (c) 2017 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Chen Liang <michaelchen@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <linux/io.h>
#include "camera.h"
#include "sensor_helper.h"



MODULE_AUTHOR("chenliang");
MODULE_DESCRIPTION("A low-level driver for ds90ub964 mipi chip for yuv sensor");
MODULE_LICENSE("GPL");

/*define module timing*/
#define MCLK              (25*1000*1000)
#define VREF_POL          V4L2_MBUS_VSYNC_ACTIVE_LOW
#define HREF_POL          V4L2_MBUS_HSYNC_ACTIVE_HIGH
#define CLK_POL           V4L2_MBUS_PCLK_SAMPLE_RISING
#define V4L2_IDENT_SENSOR  0x6131

/*static struct delayed_work sensor_s_ae_ratio_work;*/
#define SENSOR_NAME "mipi_964"
#define SENSOR_NAME2 "mipi_964_2"

#define DS90UB964_INIT_UB946_IN_PROBE
//#define BSPUTILS_USE_PATTERN

#define BSPUTILS_UB964_VERSION_ID       (0x34U)
#define SER_ADDR (0x58)
#define REG21_FWD_CTL2 (0x3c)

#define REG70_RX0_DT MIPI_YUV422
#if (0x03 == REG21_FWD_CTL2) || (0x14 == REG21_FWD_CTL2)
#define REG70_RX1_DT (0x40|REG70_RX0_DT)
#define REG70_RX2_DT (REG70_RX0_DT)
#define REG70_RX3_DT (0x40|REG70_RX0_DT)
#else
#define REG70_RX1_DT (REG70_RX0_DT)
#define REG70_RX2_DT (REG70_RX0_DT)
#define REG70_RX3_DT (REG70_RX0_DT)
#endif

#if (0x3C == REG21_FWD_CTL2)
#define SENSOR_WIDTH (HD720_WIDTH*2)
#define SENSOR_HEIGHT (HD720_HEIGHT)
#else
#define SENSOR_WIDTH (HD720_WIDTH)
#define SENSOR_HEIGHT (HD720_HEIGHT)
#endif

typedef enum {
    PORT0_SER = 0x68,
    PORT1_SER = 0x69,
    PORT2_SER = 0x6a,
    PORT3_SER = 0x6b,
} AliasSER_addr_t;

#define poll_time (2000) //ms

struct {
    struct i2c_client *client;
    struct delayed_work work;
} mipi964_data;

struct cfg_array {      /* coming later */
    struct regval_list *regs;
    int size;
};

typedef struct {
    u8 addr;
    u8 val;
    unsigned int delay;
} BspUtils_Ub960I2cParams;

static BspUtils_Ub960I2cParams gUb960Cfg_reg_list[] =
{
#ifndef BSPUTILS_USE_PATTERN
    {0x01, 0x01, 0xFFF},    /* Digital Reset 0 */
    {0x1F, 0x02, 0x0},  //800Mhz
    {0x10, 0x81, 0x0},    /* gpio 0 lock port 0*/
    {0x11, 0x85, 0x0},    /* gpio 1 lock port 1*/
    {0x12, 0x89, 0x0},    /* gpio 2 lock port 2*/
    {0x13, 0x8D, 0x0},    /* gpio 3 lock port 3*/
    {0x14, 0xe1, 0x0},    /* gpio 4: port 0 has line valid singal */
    {0x15, 0xe5, 0x0},    /* gpio 5: port 1 has line valid singal */
    {0x16, 0xe9, 0x0},    /* gpio 6: port 2 has line valid singal */
    {0x17, 0xeD, 0x0},    /* gpio 7: port 3 has line valid singal */
    //{0x10, 0x91, 0x0}, // FrameSync signal; Device Status; Enabled

    {0x32, 0x03, 0x0}, // Write CSI 0/1 selection
    {0x33, 0x01, 0x0},
    {0x34, 0x01, 0x0},

    /* Port 0 */
    {0x4C, 0x01, 0x0},
    {0x5c, PORT0_SER<<1 ,0x0},
    {0x70, REG70_RX0_DT, 0x0}, /* VPS_ISS_CAL_CSI2_YUV422_8B */

    /* Port 1 */
    {0x4C, 0x12, 0x0},
    {0x5c, PORT1_SER<<1 ,0x0},
    {0x70, REG70_RX1_DT, 0x0}, /* VPS_ISS_CAL_CSI2_YUV422_8B */

    /* Port 2 */
    {0x4C, 0x24, 0x0},
    {0x5c, PORT2_SER<<1 ,0x0},
    {0x70, REG70_RX2_DT, 0x0}, /* VPS_ISS_CAL_CSI2_YUV422_8B */

    /* Port 3 */
    {0x4C, 0x38, 0x0},
    {0x5c, PORT3_SER<<1 ,0x0},
    {0x70, REG70_RX3_DT, 0x0}, /* VPS_ISS_CAL_CSI2_YUV422_8B */

    // broadcast port0/1/2/3
    {0x4C, 0x0f, 0x0},
    {0x58, 0x58, 0x0}, // # BC FREQ SELECT: 2.5 Mbps
    {0x6D, 0x7f, 0x0}, /* 0x7E RAW 12, 0x7F RAW 10 */
    {0x7c, 0xe8, 0x0}, // FV polarity
    {0x7d, 0x80, 0x0},
    {0x6E, 0x8A, 0x100}, // reset
    {0x6E, 0x9A, 0x0}, // Framesync GPIO0?

    {0x21, REG21_FWD_CTL2, 0x0},
    {0x18, 0x00, 0x0}, // disable FrameSync
    /* framesync setup */
    {0x19, 0x00, 0x0}, // Framesync High Time 1
    {0x1A, 0x02, 0x0}, // Framesync High time 0
    {0x1B, 0x0A, 0x0}, // Framesync Low Time 1
    {0x1c, 0xD6, 0x0}, // Framesync Low Time 0
    /* framesync */
    {0x18, 0x01, 0x0}, // Enable FrameSync
    {0x20, 0x0c, 0x0}, //map rx port to csi port
#else

    {0x32, 0x01, 0x0},
    {0x33, 0x01, 0x0},

    {0xB0, 0x00, 0x0},
    {0xB1, 0x01, 0x0},
    {0xB2, 0x01, 0x0},

    {0xB1, 0x02, 0x0},
    {0xB2, 0x33, 0x0},

    {0xB1, 0x03, 0x0},
    {0xB2, 0x1E, 0x0},

    {0xB1, 0x04, 0x0},
    {0xB2, 0x0A, 0x0},

    {0xB1, 0x05, 0x0},
    {0xB2, 0x00, 0x0},

    {0xB1, 0x06, 0x0},
    {0xB2, 0x01, 0x0},

    {0xB1, 0x07, 0x0},
    {0xB2, 0x40, 0x0},

    {0xB1, 0x08, 0x0},
    {0xB2, 0x02, 0x0},

    {0xB1, 0x09, 0x0},
    {0xB2, 0xD0, 0x0},

    {0xB1, 0x0A, 0x0},
    {0xB2, 0x04, 0x0},

    {0xB1, 0x0B, 0x0},
    {0xB2, 0x1A, 0x0},

    {0xB1, 0x0C, 0x0},
    {0xB2, 0x0C, 0x0},

    {0xB1, 0x0D, 0x0},
    {0xB2, 0x67, 0x0},

    {0xB1, 0x0E, 0x0},
    {0xB2, 0x21, 0x0},

    {0xB1, 0x0F, 0x0},
    {0xB2, 0x0A, 0x0},
#endif

    {0x00, 0x00, 0xFFF}/*terminated flag */
};

/*
 * The default register settings
 *
 */
static struct regval_list sensor_default_regs[] = {

};

struct v4l2_subdev *nvp6234_sd;

static void mipi964_work_func(struct work_struct *work)
{
#if (0x3c == REG21_FWD_CTL2)
    int wRx = 0;

    int sync = i2c_smbus_read_word_data(mipi964_data.client, 0x22) & 0x03;
    //usleep_range(10000, 12000);
    //sync = i2c_smbus_read_word_data(mipi964_data.client, 0x22) & sync; // double check
    if (0x03 != sync) sensor_err("[hardy] sync:0x%x\n", sync);

    if (0 == (sync & 0x01)) wRx = 0x03;
    if (0 == (sync & 0x02)) wRx |= 0x0c;

    if (0 != wRx) {
        sensor_err("[hardy] wRx:0x%x\n", wRx);
        // reset rx
        i2c_smbus_write_byte_data(mipi964_data.client, 0x4c, wRx);
        i2c_smbus_write_byte_data(mipi964_data.client, 0x6E, 0x8a);
        usleep_range(10000, 12000);
        i2c_smbus_write_byte_data(mipi964_data.client, 0x6E, 0x9a);
        usleep_range(10000000, 12000000);
    }

    schedule_delayed_work(&mipi964_data.work, msecs_to_jiffies(poll_time)/2);
#endif
}

static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
    int ret = 0;
    return ret;
}

/*
 * Stuff that knows about the sensor.
 */

static int sensor_power(struct v4l2_subdev *sd, int on)
{
    int ret;
    ret = 0;

#ifdef SENSOR_NAME2
    if(0 == strcmp(sd->name, SENSOR_NAME2)) return 0;
#endif

    switch (on) {
    case STBY_ON:
        sensor_dbg("STBY_ON!\n");
        cci_lock(sd);
        cancel_delayed_work_sync(&mipi964_data.work);
        vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
        vin_set_mclk(sd, OFF);
        cci_unlock(sd);
        break;
    case STBY_OFF:
        sensor_dbg("STBY_OFF!\n");
        cci_lock(sd);
        vin_set_mclk_freq(sd, MCLK);
        vin_set_mclk(sd, ON);
        usleep_range(10000, 12000);
        vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
        usleep_range(10000, 12000);
        cci_unlock(sd);
        ret = sensor_s_sw_stby(sd, CSI_GPIO_LOW);
        if (ret < 0)
            sensor_err("soft stby off falied!\n");
        usleep_range(10000, 12000);
        schedule_delayed_work(&mipi964_data.work, msecs_to_jiffies(poll_time));

        break;
    case PWR_ON:
        sensor_print("PWR_ON!\n");
        cci_lock(sd);
        vin_set_mclk_freq(sd, MCLK);
        vin_set_mclk(sd, ON);
        vin_set_pmu_channel(sd, IOVDD, ON);
        vin_set_pmu_channel(sd, DVDD, ON);
        usleep_range(10000, 12000);
        vin_gpio_set_status(sd, VDD3V3_EN, 1);
        vin_gpio_set_status(sd, V5_EN, 1);
        vin_gpio_set_status(sd, CCDVDD_EN, 1);
        vin_gpio_set_status(sd, RESET, 1);
        vin_gpio_set_status(sd, POWER_EN, 1);
        vin_gpio_write(sd, VDD3V3_EN, CSI_GPIO_HIGH);
        vin_gpio_write(sd, V5_EN, CSI_GPIO_HIGH);
        vin_gpio_write(sd, CCDVDD_EN, CSI_GPIO_HIGH);
        vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);
        usleep_range(10000, 12000);
        vin_gpio_write(sd, POWER_EN, CSI_GPIO_HIGH);
        usleep_range(10000, 12000);
        vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
        usleep_range(10000, 12000);
        vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
        usleep_range(5000, 10000);
        vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
        cci_unlock(sd);
        schedule_delayed_work(&mipi964_data.work, msecs_to_jiffies(poll_time));
        break;

    case PWR_OFF:
        sensor_print("PWR_OFF!\n");
        cci_lock(sd);
        cancel_delayed_work_sync(&mipi964_data.work);
        vin_set_mclk(sd, OFF);
        usleep_range(10000, 12000);
        vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
        vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);
        usleep_range(10000, 12000);
        vin_gpio_write(sd, VDD3V3_EN, CSI_GPIO_LOW);
        vin_gpio_write(sd, V5_EN, CSI_GPIO_LOW);
        vin_gpio_write(sd, CCDVDD_EN, CSI_GPIO_LOW);
        vin_set_pmu_channel(sd, IOVDD, OFF);
        vin_set_pmu_channel(sd, DVDD, OFF);
        cci_unlock(sd);
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{
    switch (val) {
    case 0:
        vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
        usleep_range(10000, 12000);
        break;
    case 1:
        vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
        usleep_range(10000, 12000);
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static int sensor_detect(struct v4l2_subdev *sd)
{
    data_type rdval;

#ifdef SENSOR_NAME2
    if(0 == strcmp(sd->name, SENSOR_NAME2)) return 0;
#endif

    sensor_read(sd, 0x00, &rdval);
    sensor_print("%s read addr 0x00 value 0x%x\n", __func__, rdval);
    sensor_read(sd, 0x5D, &rdval);
    sensor_print("%s read addr 0x5D value 0x%x\n", __func__, rdval);
    sensor_read(sd, 0x5E, &rdval);
    sensor_print("%s read addr 0x5E value 0x%x\n", __func__, rdval);
    sensor_read(sd, 0xF0, &rdval);
    sensor_print("%s read addr 0xF0 value is 0x%x\n", __func__, rdval);
    sensor_read(sd, 0xF1, &rdval);
    sensor_print("%s read addr 0xF1 value is 0x%x\n", __func__, rdval);
    sensor_read(sd, 0xF2, &rdval);
    sensor_print("%s read addr 0xF2 value is 0x%x\n", __func__, rdval);
    sensor_read(sd, 0xF3, &rdval);
    sensor_print("%s read addr 0xF3 value is 0x%x\n", __func__, rdval);
    sensor_read(sd, 0xF4, &rdval);
    sensor_print("%s read addr 0xF4 value is 0x%x\n", __func__, rdval);
    sensor_read(sd, 0xF5, &rdval);
    sensor_print("%s read addr 0xF5 value is 0x%x\n", __func__, rdval);
    sensor_read(sd, 0xF8, &rdval);
    sensor_print("%s read addr 0xF8 value is 0x%x\n", __func__, rdval);
    sensor_read(sd, 0xF9, &rdval);
    sensor_print("%s read addr 0xF9 value is 0x%x\n", __func__, rdval);
    sensor_read(sd, 0xFA, &rdval);
    sensor_print("%s read addr 0xFA value is 0x%x\n", __func__, rdval);
    sensor_read(sd, 0xFB, &rdval);
    sensor_print("%s read addr 0xFB value is 0x%x\n", __func__, rdval);

    return 0;
}

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
    int ret;
    struct sensor_info *info = to_state(sd);

    sensor_dbg("sensor_init\n");

    /*Make sure it is a target sensor */
    ret = sensor_detect(sd);
    if (ret) {
        sensor_err("chip found is not an target chip.\n");
        return ret;
    }

    info->focus_status = 0;
    info->low_speed = 0;
    info->width = SENSOR_WIDTH;
    info->height = SENSOR_HEIGHT;
    info->hflip = 0;
    info->vflip = 0;
    info->gain = 0;

    info->tpf.numerator = 1;
    info->tpf.denominator = 30;

    //info->preview_first_flag = 1;

    return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
    int ret = 0;
    struct sensor_info *info = to_state(sd);
    switch (cmd) {
    case GET_CURRENT_WIN_CFG:
        if (info->current_wins != NULL) {
            memcpy(arg, info->current_wins,
                   sizeof(struct sensor_win_size));
            ret = 0;
        } else {
            sensor_err("empty wins!\n");
            ret = -1;
        }
        break;
    case SET_FPS:
        break;
    case VIDIOC_VIN_SENSOR_EXP_GAIN:
        break;
    case VIDIOC_VIN_SENSOR_CFG_REQ:
        sensor_cfg_req(sd, (struct sensor_config *)arg);
        break;
    default:
        return -EINVAL;
    }
    return ret;
}

static struct sensor_format_struct sensor_formats[] = {
    {
        .desc       = "YUYV 4:2:2",
        .mbus_code  = V4L2_MBUS_FMT_UYVY8_2X8,
        .regs       = sensor_default_regs,
        .regs_size = ARRAY_SIZE(sensor_default_regs),
        .bpp        = 2,
    }
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */
static struct sensor_win_size sensor_win_sizes[] = {
    {
     .width = SENSOR_WIDTH,
     .height = SENSOR_HEIGHT,
     .hoffset = 0,
     .voffset = 0,
     .hts = 0,
     .vts = 0,
     .pclk = 72*1000*1000,
     .mipi_bps = 450*1000*1000,
     .fps_fixed = 30,
     .bin_factor = 1,
     .intg_min = 4 << 4,
     .intg_max = (844 - 12) << 4,
     .gain_min = 1 << 4,
     .gain_max = 700,
     .regs = sensor_default_regs,
     .regs_size = ARRAY_SIZE(sensor_default_regs),
     .set_size = NULL,
     },
};
#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
                struct v4l2_mbus_config *cfg)
{
    struct sensor_info *info = to_state(sd);

    printk("%s isp_wdr_mode = %d\n", __FUNCTION__, info->isp_wdr_mode);

    cfg->type = V4L2_MBUS_CSI2;
#if (0x3C == REG21_FWD_CTL2)
    cfg->flags = V4L2_MBUS_CSI2_4_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
#else
    cfg->flags = V4L2_MBUS_CSI2_4_LANE | V4L2_MBUS_CSI2_CHANNEL_0 | V4L2_MBUS_CSI2_CHANNEL_1;
#endif

    return 0;
}

int Tids90ub964_init(struct v4l2_subdev *sd)
{
    int reg_len = sizeof(gUb960Cfg_reg_list)/sizeof(gUb960Cfg_reg_list[0]);
    int i;
    printk("%s +\n", __FUNCTION__);

#ifdef SENSOR_NAME2
    if(0 == strcmp(sd->name, SENSOR_NAME2)) return 0;
#endif

    for (i = 0; i < reg_len; i++) {
        if (sensor_write(sd, gUb960Cfg_reg_list[i].addr, gUb960Cfg_reg_list[i].val) < 0) {
            printk("<<<<<<<<<<<<<< write 964 fail. >>>>>>>>>>>>>>>>>>>\n");
            return -1;
        }
        usleep_range(gUb960Cfg_reg_list[i].delay, gUb960Cfg_reg_list[i].delay);
    }

    printk("%s -\n", __FUNCTION__);
    return 0;
}

static int sensor_reg_init(struct sensor_info *info)
{
    struct v4l2_subdev *sd = &info->sd;
    struct sensor_win_size *wsize = info->current_wins;

    sensor_dbg("sensor_reg_init\n");
    Tids90ub964_init(sd);

    //ret = sensor_write(sd, 0x18, 0x01);
    //usleep_range(1000, 1000);

    info->width = wsize->width;
    info->height = wsize->height;
    sensor_print("s_fmt set width = %d, height = %d\n", wsize->width,
              wsize->height);

    return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
    struct sensor_info *info = to_state(sd);
    sensor_print("%s on = %d, %d*%d %x\n", __func__, enable,
          info->current_wins->width,
          info->current_wins->height, info->fmt->mbus_code);

    if (!enable)
        return 0;
    return sensor_reg_init(info);
}


/* ----------------------------------------------------------------------- */

static const struct v4l2_subdev_core_ops sensor_core_ops = {
    .reset = sensor_reset,
    .init = sensor_init,
    .s_power = sensor_power,
    .ioctl = sensor_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl32 = sensor_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops sensor_video_ops = {
    .s_parm = sensor_s_parm,
    .g_parm = sensor_g_parm,
    .s_stream = sensor_s_stream,
    .g_mbus_config = sensor_g_mbus_config,
};

static const struct v4l2_subdev_pad_ops sensor_pad_ops = {
    .enum_mbus_code = sensor_enum_mbus_code,
    .enum_frame_size = sensor_enum_frame_size,
    .get_fmt = sensor_get_fmt,
    .set_fmt = sensor_set_fmt,
};

static const struct v4l2_subdev_ops sensor_ops = {
    .core = &sensor_core_ops,
    .video = &sensor_video_ops,
    .pad = &sensor_pad_ops,
};


/* ----------------------------------------------------------------------- */
static struct cci_driver cci_drv[] = {
    {
        .name = SENSOR_NAME,
        .addr_width = CCI_BITS_8,
        .data_width = CCI_BITS_8,
    },
#ifdef SENSOR_NAME2
    {
        .name = SENSOR_NAME2,
        .addr_width = CCI_BITS_8,
        .data_width = CCI_BITS_8,
    }
#endif
};

static int sensor_probe(struct i2c_client *client,
            const struct i2c_device_id *id)
{
    int i = 0;
    struct v4l2_subdev *sd;
    struct sensor_info *info;
    struct i2c_adapter *i2c_adap = client->adapter;

    sensor_err("[hardy] %s:%d name:%s\n", __FUNCTION__, __LINE__, client->name);

    info = kzalloc(sizeof(struct sensor_info), GFP_KERNEL);
    if (info == NULL)
        return -ENOMEM;

    sd = &info->sd;
    for (i=0; i<sizeof(cci_drv)/sizeof(cci_drv[0]); i++) {
        if (0 == strcmp(cci_drv[i].name, client->name) ) {
            cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv[i]);
        }
    }
    mutex_init(&info->lock);

    info->fmt = &sensor_formats[0];
    info->fmt_pt = &sensor_formats[0];
    info->win_pt = &sensor_win_sizes[0];
    info->fmt_num = N_FMTS;
    info->win_size_num = N_WIN_SIZES;
    info->sensor_field = V4L2_FIELD_NONE;
    info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET1 | MIPI_NORMAL_MODE;
    info->stream_seq = MIPI_BEFORE_SENSOR;
    info->af_first_flag = 1;
    info->exp = 0;
    info->gain = 0;

    if(0 == strcmp(client->name, SENSOR_NAME) ) {
        mipi964_data.client = client;
        INIT_DELAYED_WORK(&mipi964_data.work, mipi964_work_func);
    }

    return 0;
}

static int sensor_remove(struct i2c_client *client)
{
    struct v4l2_subdev *sd;
    int i = 0;

    cancel_delayed_work_sync(&mipi964_data.work);

    for (i=0; i<sizeof(cci_drv)/sizeof(cci_drv[0]); i++) {
        if (0 == strcmp(cci_drv[i].name, client->name) ) {
            sd = cci_dev_remove_helper(client, &cci_drv[i]);
        }
    }
    kfree(to_state(sd));

    return 0;
}

static const struct i2c_device_id sensor_id[] = {
    {SENSOR_NAME, 0},
#ifdef SENSOR_NAME2
    {SENSOR_NAME2, 0},
#endif
    {}
};

MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
    .driver = {
           .owner = THIS_MODULE,
           .name = SENSOR_NAME,
           },
    .probe = sensor_probe,
    .remove = sensor_remove,
    .id_table = sensor_id,
};

static __init int init_sensor(void)
{
    int ret = 0;

    ret = cci_dev_init_helper(&sensor_driver);

    return ret;
}

static __exit void exit_sensor(void)
{
    cci_dev_exit_helper(&sensor_driver);
}

#ifdef CONFIG_ARCH_SUN8IW17P1
subsys_initcall(init_sensor);
#else
module_init(init_sensor);
#endif
module_exit(exit_sensor);
