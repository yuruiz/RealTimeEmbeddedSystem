#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

#define SUCCESS 0
#define DEVICE_NAME "psdev"
#define BUF_LEN 200

static int Major;
static int Device_Open = 0;
static char msg[BUF_LEN];
static char *msg_Ptr;
static int process_cnt;

static struct file_operations fops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release
};


int init_module(void)
{
        Major = register_chrdev(99, DEVICE_NAME, &fops);

	if (Major < 0) {
	  printk(KERN_ALERT "Device Initialization failed.\n");
	  return Major;
	}

	printk(KERN_INFO "Device Initialization Success at major number %d\n", Major);

	return SUCCESS;
}

void cleanup_module(void)
{
	unregister_chrdev(99, DEVICE_NAME);
	printk(KERN_INFO "The device rmoved Successfully\n");
}

static int device_open(struct inode *inode, struct file *file)
{
	if (Device_Open)
	{
		printk(KERN_INFO "Device already opened by other process, access failed!\n");
		return -EBUSY;
	}

	Device_Open++;
	
	process_cnt = 0;
	msg_Ptr = msg;
	try_module_get(THIS_MODULE);
	printk(KERN_INFO "device opened successfully!\n");

	return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file)
{
	Device_Open--;

	module_put(THIS_MODULE);
	printk(KERN_INFO "device released successfully!\n");
	return 0;
}


static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t * offset)
{
	int bytes_read = 0;
	int bytes_print = 0;
	int temp_pro_cnt = 0;
	struct task_struct *p;

	printk(KERN_INFO "start reading the device!\n");


//	bytes_print = sprintf(msg_Ptr, "tid\tpid\tpr\tname\n");

	if(process_cnt == 0){
		while(bytes_print > 0 && length > 0) {
		    put_user(*(msg_Ptr++), buffer++);
		    length--;
		    bytes_print--;
		    bytes_read++;
		}
	}

	for_each_process(p)
	{
		temp_pro_cnt++;
		if(temp_pro_cnt <= process_cnt && process_cnt > 0){
			continue;
		}
		msg_Ptr = msg;
		if (p->policy == SCHED_FIFO || p->policy == SCHED_RR){
			bytes_print = sprintf(msg_Ptr, "%d\t%d\t%d\t%s\n", p->pid, p->tgid, p->rt_priority, p->comm);
			while(bytes_print > 0 && length > 0) {
			    put_user(*(msg_Ptr++), buffer++);
			    length--;
	 		    bytes_print--;
	    		    bytes_read++;
			}
		}
	}

	if(temp_pro_cnt > process_cnt){
		process_cnt = temp_pro_cnt;
	}
	return bytes_read;
}

static ssize_t device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
	return -EINVAL;
}
