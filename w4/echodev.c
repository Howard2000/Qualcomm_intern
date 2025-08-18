#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/ioctl.h>
#include <linux/types.h>

// 兼容性宏定义
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
#define class_create(owner, name) class_create(owner, name)
#else
#define class_create(owner, name) class_create(name)
#endif

#define DEVICE_NAME "echodev"
#define BUFFER_SIZE 4096
#define MAX_BUFFER_SIZE 1048576

// IOCTL命令定义
#define ECHO_DEV_MAGIC 'E'
#define ECHO_DEV_RESIZE _IOW(ECHO_DEV_MAGIC, 0, size_t)
#define ECHO_DEV_GET_SIZE _IOR(ECHO_DEV_MAGIC, 1, size_t)
#define ECHO_DEV_GET_DATA_SIZE _IOR(ECHO_DEV_MAGIC, 2, size_t)

static dev_t dev_num;
static struct cdev echodev_cdev;
static struct class *echodev_class;

struct echodev_data {
    char *buffer;
    size_t buffer_size;
    size_t data_size;
    struct mutex lock;
};

static struct echodev_data *device_data;

// 调整缓冲区大小
static int resize_buffer(struct echodev_data *dev, size_t new_size)
{
    char *new_buffer;
    
    if (new_size > MAX_BUFFER_SIZE) {
        printk(KERN_WARNING "echodev: Requested size %zu exceeds maximum %d\n", 
               new_size, MAX_BUFFER_SIZE);
        return -EINVAL;
    }
    
    if (new_size < 1) {
        printk(KERN_WARNING "echodev: Requested size %zu is too small\n", new_size);
        return -EINVAL;
    }
    
    new_buffer = krealloc(dev->buffer, new_size, GFP_KERNEL);
    if (!new_buffer) {
        printk(KERN_ERR "echodev: Failed to allocate %zu bytes\n", new_size);
        return -ENOMEM;
    }
    
    dev->buffer = new_buffer;
    dev->buffer_size = new_size;
    
    // 如果新缓冲区小于当前数据大小，截断数据
    if (dev->data_size > new_size) {
        dev->data_size = new_size;
        printk(KERN_INFO "echodev: Truncated data to fit new buffer size\n");
    }
    
    printk(KERN_INFO "echodev: Resized buffer to %zu bytes\n", new_size);
    return 0;
}

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

    if (*f_pos >= dev->buffer_size) {
        retval = -ENOSPC;
        goto out;
    }

    space_remaining = BUFFER_SIZE - (size_t)*f_pos;
    if (count > space_remaining){
        count = space_remaining;
	retval = -ENOSPC;
    }

    if (copy_from_user(dev->buffer + *f_pos, buf, count)) {
        retval = -EFAULT;
        goto out;
    }

    *f_pos += count;
    if (*f_pos > dev->data_size)
        dev->data_size = *f_pos;
    
    if (retval == 0) {
        retval = count; // 如果之前没有设置错误，返回写入字节数
    } else if (retval == -ENOSPC) {
        retval = count; // 部分写入成功，返回实际写入字节数
    }

out:
    mutex_unlock(&dev->lock);
    return retval;
}

static long echodev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct echodev_data *dev = filp->private_data;
    int ret = 0;
    size_t new_size;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    switch (cmd) {
        case ECHO_DEV_RESIZE:
            if (copy_from_user(&new_size, (size_t __user *)arg, sizeof(new_size))) {
                ret = -EFAULT;
                break;
            }
            ret = resize_buffer(dev, new_size);
            break;
            
        case ECHO_DEV_GET_SIZE:
            if (copy_to_user((size_t __user *)arg, &dev->buffer_size, sizeof(dev->buffer_size))) {
                ret = -EFAULT;
            }
            break;
            
        case ECHO_DEV_GET_DATA_SIZE:
            if (copy_to_user((size_t __user *)arg, &dev->data_size, sizeof(dev->data_size))) {
                ret = -EFAULT;
            }
            break;
            
        default:
            ret = -ENOTTY;
            break;
    }

    mutex_unlock(&dev->lock);
    return ret;
}

static const struct file_operations echodev_fops = {
    .owner = THIS_MODULE,
    .open = echodev_open,
    .read = echodev_read,
    .write = echodev_write,
    .unlocked_ioctl = echodev_ioctl,
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

    // 分配初始缓冲区
    device_data->buffer = kzalloc(INITIAL_BUFFER_SIZE, GFP_KERNEL);
    if (!device_data->buffer) {
        ret = -ENOMEM;
        goto fail_buffer;
    }
    device_data->buffer_size = INITIAL_BUFFER_SIZE;
    device_data->data_size = 0;

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
	printk(KERN_ERR "echodev: Failed to create device class\n");
        goto fail_class;
    }
    
    // 创建设备节点
    if (IS_ERR(device_create(echodev_class, NULL, dev_num, NULL, DEVICE_NAME))) {
        ret = PTR_ERR(device_create(echodev_class, NULL, dev_num, NULL, DEVICE_NAME));
        printk(KERN_ERR "echodev: Failed to create device node\n");
        goto fail_device;
    }

    printk(KERN_INFO "echodev: Initialized with buffer size %d\n", BUFFER_SIZE);
    return 0;

fail_device:
    class_destroy(echodev_class);
fail_class:
    cdev_del(&echodev_cdev);
fail_cdev:
    kfree(device_data->buffer);
fail_buffer:
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
    if (device_data) {
        kfree(device_data->buffer);
        kfree(device_data);
    }
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "echodev: Module unloaded\n");
}

module_init(echodev_init);
module_exit(echodev_exit);
