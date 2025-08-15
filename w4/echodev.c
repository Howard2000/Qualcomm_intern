#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/version.h>

// 兼容性宏定义
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
#define class_create(owner, name) class_create(owner, name)
#else
#define class_create(owner, name) class_create(name)
#endif

#define DEVICE_NAME "echodev"
#define BUFFER_SIZE 4096

static dev_t dev_num;
static struct cdev echodev_cdev;
static struct class *echodev_class;

struct echodev_data {
    char buffer[BUFFER_SIZE];
    size_t data_size;
    struct mutex lock;
};

static struct echodev_data *device_data;

static int echodev_open(struct inode *inode, struct file *filp)
{
    filp->private_data = device_data;
    return 0;
}

static ssize_t echodev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct echodev_data *dev = filp->private_data;
    ssize_t retval = 0;
    size_t bytes_to_read;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    if (*f_pos >= dev->data_size) {
        mutex_unlock(&dev->lock);
        return 0;
    }

    bytes_to_read = min(count, dev->data_size - (size_t)*f_pos);
    if (copy_to_user(buf, dev->buffer + *f_pos, bytes_to_read)) {
        retval = -EFAULT;
    } else {
        *f_pos += bytes_to_read;
        retval = bytes_to_read;
    }

    mutex_unlock(&dev->lock);
    return retval;
}

static ssize_t echodev_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct echodev_data *dev = filp->private_data;
    ssize_t retval = 0;
    size_t space_remaining;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    space_remaining = BUFFER_SIZE - (size_t)*f_pos;
    if (count > space_remaining)
        count = space_remaining;

    if (count == 0) {
        retval = -ENOSPC;
        goto out;
    }

    if (copy_from_user(dev->buffer + *f_pos, buf, count)) {
        retval = -EFAULT;
        goto out;
    }

    *f_pos += count;
    if (*f_pos > dev->data_size)
        dev->data_size = *f_pos;
    
    retval = count;

out:
    mutex_unlock(&dev->lock);
    return retval;
}

static const struct file_operations echodev_fops = {
    .owner = THIS_MODULE,
    .open = echodev_open,
    .read = echodev_read,
    .write = echodev_write,
};

static int __init echodev_init(void)
{
    int ret;

    // 分配设备号
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "Failed to allocate device number\n");
        return ret;
    }

    // 分配设备数据结构
    device_data = kzalloc(sizeof(struct echodev_data), GFP_KERNEL);
    if (!device_data) {
        ret = -ENOMEM;
        goto fail_alloc;
    }
    mutex_init(&device_data->lock);

    // 初始化字符设备
    cdev_init(&echodev_cdev, &echodev_fops);
    echodev_cdev.owner = THIS_MODULE;
    
    // 添加字符设备
    ret = cdev_add(&echodev_cdev, dev_num, 1);
    if (ret < 0) {
        printk(KERN_ERR "Failed to add character device\n");
        goto fail_cdev;
    }

    // 使用兼容性宏创建类
    echodev_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(echodev_class)) {
        ret = PTR_ERR(echodev_class);
        goto fail_class;
    }
    device_create(echodev_class, NULL, dev_num, NULL, DEVICE_NAME);

    printk(KERN_INFO "echodev: Initialized with buffer size %d\n", BUFFER_SIZE);
    return 0;

fail_class:
    cdev_del(&echodev_cdev);
fail_cdev:
    kfree(device_data);
fail_alloc:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

static void __exit echodev_exit(void)
{
    device_destroy(echodev_class, dev_num);
    class_destroy(echodev_class);
    cdev_del(&echodev_cdev);
    kfree(device_data);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "echodev: Module unloaded\n");
}

module_init(echodev_init);
module_exit(echodev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Simple echo character device driver");
