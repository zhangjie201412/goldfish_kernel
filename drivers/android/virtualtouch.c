/*
 *	This driver is named virtual touch, which can send some message into user space from kernel space,
 *	for this driver just for study Linux device driver...
 *	Jay Zhang 
 *	mail: zhangjie201412@live.com
 */
#include <linux/module.h>

#include <linux/err.h>
#include <linux/input.h>
#include <linux/hwmon.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
struct virtual_dev {
	struct platform_device *vt_dev;
	struct task_struct *run_thread;
	struct input_dev *input;
	struct semaphore sem;
	int x,y;		//point position
	int color;		//line color
	int bcolor;	//background color
	int size;		//line size
	int running;
};

struct virtual_dev *vdev = NULL;

/* position read/write function */
static ssize_t read_position (struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "(%d, %d)\n", vdev->x, vdev->y);
}

static ssize_t write_position(struct device *dev,
		struct device_attribute *attr, const char *buffer, ssize_t count)
{
	int x,y;
	sscanf(buffer, "%d%d", &x, &y);
	vdev->x = x;
	vdev->y = y;
	//do something with x and y ===> poll the data;
	up(&vdev->sem);
	return count;
}

/* color read/write function */
static ssize_t read_color(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "line color is %d\n", vdev->color);
}

static ssize_t write_color(struct device *dev,
		struct device_attribute *attr, const char *buffer, ssize_t count)
{
	int color;
	sscanf(buffer, "%d", &color);
	vdev->color = color;
	return count;
}

/* bcolor read/write function */
static ssize_t read_bcolor(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "background color is %d\n", vdev->bcolor);
}

static ssize_t write_bcolor(struct device *dev,
		struct device_attribute *attr, const char *buffer, ssize_t count)
{
	int bcolor;
	sscanf(buffer, "%d", &bcolor);
	vdev->bcolor = bcolor;
	return count;
}

/* attach the sysfs */
DEVICE_ATTR(position, 0666, read_position, write_position);
DEVICE_ATTR(color, 0666, read_color, write_color);
DEVICE_ATTR(bcolor, 0666, read_bcolor, write_bcolor);
//DEVICE_ATTR(size, 0666, read_size, write_size);

/* attribute description */
static struct attribute *vdev_attrs[] = {
	&dev_attr_position.attr,
	&dev_attr_color.attr,
	&dev_attr_bcolor.attr,
//	&dev_attr,size,
	NULL
};

/* attribute group */
static struct attribute_group vdev_attr_group = {
	.attrs = vdev_attrs,
};

static int work_thread(void *data)
{
	int x, y, ret;
	struct virtual_dev *pvdev = (struct virtual_dev *)data;
	struct semaphore sema = pvdev->sem;
	// poll the data into user space
	printk(KERN_INFO "work thread running!!");
	
	while(pvdev->running) {
		do{
			ret = down_interruptible(&sema);
		} while(ret == -EINTR);
		//printk("done!\n");
		//poll the x and y data into user space
		x = pvdev->x;
		y = pvdev->y;
		input_report_abs(pvdev->input, ABS_X, x);
		input_report_abs(pvdev->input, ABS_Y, y);
		input_sync(pvdev->input);
		printk("position: %d | %d\n", x, y);	
	}
	return 0;
}

static int virtual_probe(struct platform_device *pdev)
{
	int ret;
	//malloc for vdev
	vdev = kzalloc(sizeof(struct virtual_dev), GFP_KERNEL);
	if(!vdev) {
		vdev = NULL;
		printk(KERN_INFO "kzalloc for vdev failed.\n");
		ret = -ENOMEM;
		goto kzalloc_failed;
	}
	//initialized for semaphore
	sema_init(&(vdev->sem), 0);
	//initialized for input subsystem 
	vdev->input = input_allocate_device();
	if(!(vdev->input)) {
		vdev->input = NULL;
		printk(KERN_INFO "allocate input device failed.\n");
		ret = -ENOMEM;
		goto allocate_input_failed;
	}
	set_bit(EV_ABS, vdev->input->evbit);
	//for x
	input_set_abs_params(vdev->input, ABS_X, -1024, 1024, 0, 0);
	//for y
	input_set_abs_params(vdev->input, ABS_Y, -1024, 1024, 0, 0);
	//set name
	vdev->input->name = "virtual-touch";
	ret = input_register_device(vdev->input);
	if(ret) {
		printk(KERN_ERR "%s: Unable to register input device: %s\n",__func__, vdev->input->name);
		goto input_register_failed;
		//return ret;
	}
	//initialized for sysfs of our virtual driver
	vdev->vt_dev = pdev;
	sysfs_create_group(&vdev->vt_dev->dev.kobj, &vdev_attr_group);
	//run thread to poll data
	vdev->run_thread = kthread_run(work_thread, vdev, "vt_thread");
	vdev->running = 1;
	platform_set_drvdata(pdev, vdev);
	printk(KERN_INFO "virtual touch device probe successful.\n");
	return 0;
input_register_failed:
	input_free_device(vdev->input);
allocate_input_failed:
	kfree(vdev);
kzalloc_failed:
	return ret;
}

static struct platform_driver virtual_driver = {
	.probe = virtual_probe,
	.driver = {
		.name = "virtual touch",
	},
};

static int virtual_init(void)
{
	return platform_driver_register(&virtual_driver);
}

static void virtual_exit(void)
{
}

module_init(virtual_init);
module_exit(virtual_exit);
