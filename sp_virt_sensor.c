#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include "sp_virt_sensor_uapi.h"

#define DEVICE_NAME "sp_virt_sensor"
#define CLASS_NAME "sp_sensor"
#define RING_SIZE 32

struct sp_sensor_dev {
	struct cdev cdev;
	dev_t devt;
	struct class *class;
	struct device *device;
	struct mutex lock;
	wait_queue_head_t readq;
	struct sp_sensor_sample ring[RING_SIZE];
	unsigned int head;
	unsigned int tail;
	unsigned int count;
};

static struct sp_sensor_dev *sensor;

static int sp_open(struct inode *inode, struct file *file)
{
	file->private_data = sensor;
	return 0;
}

static ssize_t sp_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{
	struct sp_sensor_dev *dev = file->private_data;
	struct sp_sensor_sample sample;
	int ret;

	if (len < sizeof(sample))
		return -EINVAL;

	ret = wait_event_interruptible(dev->readq, dev->count > 0);
	if (ret)
		return ret;

	mutex_lock(&dev->lock);
	if (dev->count == 0) {
		mutex_unlock(&dev->lock);
		return -EAGAIN;
	}

	sample = dev->ring[dev->tail];
	dev->tail = (dev->tail + 1) % RING_SIZE;
	dev->count--;
	mutex_unlock(&dev->lock);

	if (copy_to_user(buf, &sample, sizeof(sample)))
		return -EFAULT;

	return sizeof(sample);
}

static ssize_t sp_write(struct file *file, const char __user *buf, size_t len, loff_t *off)
{
	struct sp_sensor_dev *dev = file->private_data;
	struct sp_sensor_sample sample;

	if (len < sizeof(sample))
		return -EINVAL;

	if (copy_from_user(&sample, buf, sizeof(sample)))
		return -EFAULT;

	mutex_lock(&dev->lock);
	if (dev->count == RING_SIZE) {
		dev->tail = (dev->tail + 1) % RING_SIZE;
		dev->count--;
	}

	dev->ring[dev->head] = sample;
	dev->head = (dev->head + 1) % RING_SIZE;
	dev->count++;
	mutex_unlock(&dev->lock);

	wake_up_interruptible(&dev->readq);
	return sizeof(sample);
}

static long sp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct sp_sensor_dev *dev = file->private_data;
	int count;

	switch (cmd) {
	case SP_SENSOR_GET_COUNT:
		mutex_lock(&dev->lock);
		count = dev->count;
		mutex_unlock(&dev->lock);

		if (copy_to_user((int __user *)arg, &count, sizeof(count)))
			return -EFAULT;
		return 0;
	default:
		return -ENOTTY;
	}
}

static __poll_t sp_poll(struct file *file, poll_table *wait)
{
	struct sp_sensor_dev *dev = file->private_data;
	__poll_t mask = 0;

	poll_wait(file, &dev->readq, wait);

	mutex_lock(&dev->lock);
	if (dev->count > 0)
		mask |= POLLIN | POLLRDNORM;
	mutex_unlock(&dev->lock);

	return mask;
}

static const struct file_operations sp_fops = {
	.owner = THIS_MODULE,
	.open = sp_open,
	.read = sp_read,
	.write = sp_write,
	.unlocked_ioctl = sp_ioctl,
	.poll = sp_poll,
};

static int __init sp_init(void)
{
	int ret;

	sensor = kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;

	mutex_init(&sensor->lock);
	init_waitqueue_head(&sensor->readq);

	ret = alloc_chrdev_region(&sensor->devt, 0, 1, DEVICE_NAME);
	if (ret)
		goto err_free;

	cdev_init(&sensor->cdev, &sp_fops);
	sensor->cdev.owner = THIS_MODULE;

	ret = cdev_add(&sensor->cdev, sensor->devt, 1);
	if (ret)
		goto err_unregister;

	sensor->class = class_create(CLASS_NAME);
	if (IS_ERR(sensor->class)) {
		ret = PTR_ERR(sensor->class);
		goto err_cdev;
	}

	sensor->device = device_create(sensor->class, NULL, sensor->devt, NULL, DEVICE_NAME);
	if (IS_ERR(sensor->device)) {
		ret = PTR_ERR(sensor->device);
		goto err_class;
	}

	pr_info("%s: loaded major=%d minor=%d\n", DEVICE_NAME, MAJOR(sensor->devt), MINOR(sensor->devt));
	return 0;

err_class:
	class_destroy(sensor->class);
err_cdev:
	cdev_del(&sensor->cdev);
err_unregister:
	unregister_chrdev_region(sensor->devt, 1);
err_free:
	kfree(sensor);
	return ret;
}

static void __exit sp_exit(void)
{
	device_destroy(sensor->class, sensor->devt);
	class_destroy(sensor->class);
	cdev_del(&sensor->cdev);
	unregister_chrdev_region(sensor->devt, 1);
	kfree(sensor);
	pr_info("%s: unloaded\n", DEVICE_NAME);
}

module_init(sp_init);
module_exit(sp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Interview Practice");
MODULE_DESCRIPTION("Virtual sensor character device driver for embedded Linux practice");

