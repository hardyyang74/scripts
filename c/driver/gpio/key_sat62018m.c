/*
*
*/
#include <linux/interrupt.h>
#include <kernel/module.h>
#include <sys/unistd.h>
#include <errno.h>
#include <kernel/lib/fdt_api.h>
#include <kernel/ld.h>
#include <kernel/drivers/input.h>
#include <hcuapi/input-event-codes.h>
#include <hcuapi/input.h>
#include <hcuapi/gpio.h>
#include <kernel/delay.h>
#include <semaphore.h>
#include <linux/delay.h>

#include <stdio.h>

#define printf(...)

#define MODEL_NAME "sat62018m"
#define sat62018m_gpioA "gpio-A"
#define sat62018m_gpioB "gpio-B"
#define sat62018m_gpioKEY "gpio-KEY"

struct sat62018m_data {
    struct input_dev *input_dev;
    u32 gpiodA;
    u32 gpiodB;
    u32 gpiodKey;

    sem_t keySem;
    u8 gpioBHigh;
    TaskHandle_t key_thread;
};

static void key_kthread(void *pvParameters)
{
    struct sat62018m_data *sat62018m = (struct sat62018m_data *)pvParameters;

    while (1) {
        sem_wait(&sat62018m->keySem);

        if (gpio_get_input(sat62018m->gpiodA) == sat62018m->gpioBHigh) {
            input_report_key(sat62018m->input_dev, KEY_LEFT, 1);
            input_sync(sat62018m->input_dev);
            input_report_key(sat62018m->input_dev, KEY_LEFT, 0);
            input_sync(sat62018m->input_dev);
        } else {
            input_report_key(sat62018m->input_dev, KEY_RIGHT, 1);
            input_sync(sat62018m->input_dev);
            input_report_key(sat62018m->input_dev, KEY_RIGHT, 0);
            input_sync(sat62018m->input_dev);

            while (gpio_get_input(sat62018m->gpiodB) != gpio_get_input(sat62018m->gpiodA) ) {
                usleep_range(2000, 2100);
            }
        }

        sat62018m->gpioBHigh = gpio_get_input(sat62018m->gpiodB);

        if (sat62018m->gpioBHigh) {
            gpio_config_irq(sat62018m->gpiodB,GPIO_DIR_INPUT | GPIO_IRQ_FALLING);
        } else {
            gpio_config_irq(sat62018m->gpiodB,GPIO_DIR_INPUT | GPIO_IRQ_RISING);
        }
        gpio_irq_enable(sat62018m->gpiodB);
    }
}

static void sat62018m_B_int(uint32_t param)
{
    struct sat62018m_data *sat62018m = (struct sat62018m_data *)param;

    gpio_irq_disable(sat62018m->gpiodB);
    sem_post(&sat62018m->keySem);
}

static void sat62018m_key_int(uint32_t param)
{
    gpio_pinset_t irq_falling = GPIO_DIR_INPUT | GPIO_IRQ_FALLING;
    gpio_pinset_t irq_rising = GPIO_DIR_INPUT | GPIO_IRQ_RISING;
    static char keydown = 1;
    struct sat62018m_data *sat62018m = (struct sat62018m_data *)param;

    if (keydown) {
        //printf("%s %d\n", __func__, __LINE__);
        gpio_config_irq(sat62018m->gpiodKey,irq_rising);

        input_report_key(sat62018m->input_dev, KEY_ENTER, 1);
        input_sync(sat62018m->input_dev);
    } else {
        //printf("%s %d\n", __func__, __LINE__);
        gpio_config_irq(sat62018m->gpiodKey,irq_falling);

        input_report_key(sat62018m->input_dev, KEY_ENTER, 0);
        input_sync(sat62018m->input_dev);
    }

    keydown = !keydown;
}

static int sat62018m_probe(const char *node)
{
    int err = 0;
    int np;
    gpio_pinset_t irq_pinset = GPIO_DIR_INPUT | GPIO_IRQ_FALLING;
    struct input_dev *input_dev;
    struct sat62018m_data *sat62018m = NULL;

    np = fdt_node_probe_by_path(node);
    if (np < 0)
        return 0;

    sat62018m = malloc(sizeof(struct sat62018m_data));

    if (!sat62018m)
    {
        printf("no memory!\n");
        return -ENOMEM;
    }
    memset(sat62018m, 0, sizeof(struct sat62018m_data));

    if (fdt_get_property_u_32_index(np, "ena-gpios", 0, (u32 *)&sat62018m->gpiodA)) {
        sat62018m->gpiodA = PINPAD_INVALID;
    }
    if (fdt_get_property_u_32_index(np, "enb-gpios", 0, (u32 *)&sat62018m->gpiodB)) {
        sat62018m->gpiodB = PINPAD_INVALID;
    }
    if (fdt_get_property_u_32_index(np, "key-gpios", 0, (u32 *)&sat62018m->gpiodKey)) {
        sat62018m->gpiodKey = PINPAD_INVALID;
    }
    printf("%s gpioA:%d gpioB:%d gpiokey:%d\n", __func__, sat62018m->gpiodA, sat62018m->gpiodB, sat62018m->gpiodKey);

    input_dev = input_allocate_device();
    if (!input_dev)
    {
        err = -ENOMEM;
        printf("failed to allocate input device\n");
        goto exit_input_dev_alloc_failed;
    }

    __set_bit(EV_KEY, input_dev->evbit);
    __set_bit(KEY_ENTER, input_dev->keybit);
    __set_bit(KEY_LEFT, input_dev->keybit);
    __set_bit(KEY_RIGHT, input_dev->keybit);

    err = input_register_device(input_dev);
    if (err)
    {
        printf("sat62018m_probe: failed to register input device\n");
        goto exit_input_register_device_failed;
    }

    sat62018m->input_dev = input_dev;
    input_set_drvdata(sat62018m->input_dev, sat62018m);

    gpio_configure(sat62018m->gpiodB,irq_pinset);
    sat62018m->gpioBHigh = gpio_get_input(sat62018m->gpiodB);
    if (!sat62018m->gpioBHigh) {
        gpio_config_irq(sat62018m->gpiodB,GPIO_DIR_INPUT | GPIO_IRQ_RISING);
    }
    err = gpio_irq_request(sat62018m->gpiodB,sat62018m_B_int,(uint32_t)sat62018m);
    if (err < 0) {
        printf("ERROR: gpio_irq_request() failed: %d\n", err);
    }
    //gpio_irq_disable(sat62018m->gpiodB);

    gpio_configure(sat62018m->gpiodA,GPIO_DIR_INPUT);

    gpio_configure(sat62018m->gpiodKey,irq_pinset);
    err = gpio_irq_request(sat62018m->gpiodKey,sat62018m_key_int,(uint32_t)sat62018m);
    if (err < 0) {
        printf("ERROR: gpio_irq_request() failed: %d\n", err);
    }

    sem_init(&sat62018m->keySem, 0, 0);
    xTaskCreate(key_kthread, (const char *)"rotary_key_kthread", configTASK_STACK_DEPTH,
            sat62018m, portPRI_TASK_NORMAL, &sat62018m->key_thread);

    return 0;

exit_input_register_device_failed:
    input_free_device(input_dev);

exit_input_dev_alloc_failed:
    free(sat62018m);

    return err;
}

static int sat62018m_init(void)
{
    int rc = 0;

    rc |= sat62018m_probe("/hcrtos/rotary_key");

    return rc;
}

module_driver(hc_sat62018m_drv, sat62018m_init, NULL, 1)

