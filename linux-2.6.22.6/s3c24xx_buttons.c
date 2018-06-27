#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

#define DEVICE_NAME     "buttons"   /* ����ģʽ��ִ�С�cat /proc/devices����������豸���� */
#define BUTTON_MAJOR    232         /* ���豸�� */

static struct class * buttonsdrv_class;
static struct class_device *buttonsdrv_class_dev;

volatile unsigned int gpfcon = NULL;
volatile unsigned int gpfdat = NULL;

volatile unsigned int led1 = 1 << 4;
volatile unsigned int led2 = 1 << 5;
volatile unsigned int led3 = 1 << 6;

struct button_irq_desc {
    int irq;
    unsigned long flags;
    char *name;
};

/* ����ָ���������õ��ⲿ�ж����ż��жϴ�����ʽ, ���� */
static struct button_irq_desc button_irqs [] = {
    {IRQ_EINT19, IRQF_TRIGGER_FALLING, "KEY1"}, /* K1 */
    {IRQ_EINT11, IRQF_TRIGGER_FALLING, "KEY2"}, /* K2 */
    {IRQ_EINT2,  IRQF_TRIGGER_FALLING, "KEY3"}, /* K3 */
    {IRQ_EINT0,  IRQF_TRIGGER_FALLING, "KEY4"}, /* K4 */
};

/* ���������µĴ���(׼ȷ��˵���Ƿ����жϵĴ���) */
static volatile int press_cnt [] = {0, 0, 0, 0};

/* �ȴ�����: 
 * ��û�а���������ʱ������н��̵���s3c24xx_buttons_read������
 * ��������
 */
static DECLARE_WAIT_QUEUE_HEAD(button_waitq);

/* �ж��¼���־, �жϷ����������1��s3c24xx_buttons_read������0 */
static volatile int ev_press = 0;


static irqreturn_t buttons_interrupt(int irq, void *dev_id)
{
    volatile int *press_cnt = (volatile int *)dev_id;
    
    *press_cnt = *press_cnt + 1; /* ����������1 */
    ev_press = 1;                /* ��ʾ�жϷ����� */
    wake_up_interruptible(&button_waitq);   /* �������ߵĽ��� */
    
    return IRQ_RETVAL(IRQ_HANDLED);
}


/* Ӧ�ó�����豸�ļ�/dev/buttonsִ��open(...)ʱ��
 * �ͻ����s3c24xx_buttons_open����
 */
static int s3c24xx_buttons_open(struct inode *inode, struct file *file)
{
    int i;
    int err;
    
    for (i = 0; i < sizeof(button_irqs)/sizeof(button_irqs[0]); i++) {
        // ע���жϴ�����
        err = request_irq(button_irqs[i].irq, , button_irqs[i].flags, button_irqs[i].name, (void *)&press_cnt[i]);
        if (err)
            break;
    }

    if (err) {
        // �ͷ��Ѿ�ע����ж�
        i--;
        for (; i >= 0; i--)
            free_irq(button_irqs[i].irq, (void *)&press_cnt[i]);
        return -EBUSY;
    }
    
    return 0;
}


/* Ӧ�ó�����豸�ļ�/dev/buttonsִ��close(...)ʱ��
 * �ͻ����s3c24xx_buttons_close����
 */
static int s3c24xx_buttons_close(struct inode *inode, struct file *file)
{
    int i;
    
    for (i = 0; i < sizeof(button_irqs)/sizeof(button_irqs[0]); i++) {
        // �ͷ��Ѿ�ע����ж�
        free_irq(button_irqs[i].irq, (void *)&press_cnt[i]);
    }

    return 0;
}


/* Ӧ�ó�����豸�ļ�/dev/buttonsִ��read(...)ʱ��
 * �ͻ����s3c24xx_buttons_read����
 */
static int s3c24xx_buttons_read(struct file *filp, char __user *buff, 
                                         size_t count, loff_t *offp)
{
    unsigned long err;
    int i = 0;

    /* ���ev_press����0������ */
    wait_event_interruptible(button_waitq, ev_press);
    for (i = 0; i < 3; i++)
    {
        switch (i)
        {
            case 1: if (press_cnt[1]) {led1 = ~led1; break;}
            case 2: if (press_cnt[2]) {led2 = ~led2; break;}
            case 3: if (press_cnt[3]) {led3 = ~led3; break;}
            case 4: if (press_cnt[4]) {led1 = ~led1; led2 = ~led2; led3 = ~led3; break;}

        }
    }    

    *gpfdat &= ~(led1 | led2 | led3);


    /* ִ�е�����ʱ��ev_press����1��������0 */
    ev_press = 0;

    /* ������״̬���Ƹ��û�������0 */
    err = copy_to_user(buff, (const void *)press_cnt, min(sizeof(press_cnt), count));
    memset((void *)press_cnt, 0, sizeof(press_cnt));

    return err ? -EFAULT : 0;
}






/* ����ṹ���ַ��豸��������ĺ���
 * ��Ӧ�ó�������豸�ļ�ʱ�����õ�open��read��write�Ⱥ�����
 * ���ջ��������ṹ�еĶ�Ӧ����
 */
static struct file_operations s3c24xx_buttons_fops = {
    .owner   =   THIS_MODULE,    /* ����һ���ָ꣬�����ģ��ʱ�Զ�������__this_module���� */
    .open    =   s3c24xx_buttons_open,
    .release =   s3c24xx_buttons_close, 
    .read    =   s3c24xx_buttons_read,
};








/*
 * ִ�С�insmod s3c24xx_buttons.ko������ʱ�ͻ�����������
 */
static int __init s3c24xx_buttons_init(void)
{
    int ret;

    /* ע���ַ��豸��������
     * ����Ϊ���豸�š��豸���֡�file_operations�ṹ��
     * ���������豸�žͺ;����file_operations�ṹ��ϵ�����ˣ�
     * �������豸ΪBUTTON_MAJOR���豸�ļ�ʱ���ͻ����s3c24xx_buttons_fops�е���س�Ա����
     * BUTTON_MAJOR������Ϊ0����ʾ���ں��Զ��������豸��
     */
    ret = register_chrdev(BUTTON_MAJOR, DEVICE_NAME, &s3c24xx_buttons_fops);

    buttonsdrv_class = class_create(THIS_MODULE, "buttons");
    buttonsdrv_class_dev = class_device_create(buttonsdrv_class, NULL, MKDEV(BUTTON_MAJOR,O), NULL, "buttons");

    gpfcon = (volatile unsigned long *)ioremap(0x56000050, 16);
    gpfdat = gpfcon + 1;

    if (ret < 0) {
      printk(DEVICE_NAME " can't register major number\n");
      return ret;
    }
    
    printk(DEVICE_NAME " initialized\n");
    return 0;
}

/*
 * ִ�С�rmmod s3c24xx_buttons.ko������ʱ�ͻ����������� 
 */
static void __exit s3c24xx_buttons_exit(void)
{
    /* Log out the major device number*/
    unregister_chrdev(BUTTON_MAJOR, DEVICE_NAME);
    class_device_unregister(buttonsdrv_class_dec);
    class_destroy(buttonsdrv_class);
}

/* ������ָ����������ĳ�ʼ��������ж�غ��� */
module_init(s3c24xx_buttons_init);
module_exit(s3c24xx_buttons_exit);

/* �������������һЩ��Ϣ�����Ǳ���� */
MODULE_AUTHOR("Wang Fangfei");             // �������������
MODULE_DESCRIPTION("S3C2410/S3C2440 BUTTON Driver");   // һЩ������Ϣ
MODULE_LICENSE("GPL");                              // ��ѭ��Э��

