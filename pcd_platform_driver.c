#include<linux/module.h>
#include<linux/device.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/kdev_t.h>
#include<linux/uaccess.h>
#include<linux/platform_device.h>
#include<linux/slab.h>
#include<linux/mod_devicetable.h>
#include"platform.h"
#undef pr_fmt
#define pr_fmt(fmt)  "%s: " fmt  , __func__

struct device_config 
{
       int config_item1;
       int config_item2;
};

enum pcdev_names 
{
	PCDEVA1X,
	PCDEVB1X,
	PCDEVC1X,
	PCDEVD1X	
};

struct device_config pcdev_config[] =
{	
	[PCDEVA1X] = {.config_item1 = 60,.config_item2 = 21},
        [PCDEVB1X] = {.config_item1 = 50,.config_item2 = 22},
        [PCDEVC1X] = {.config_item1 = 40,.config_item2 = 23},
        [PCDEVD1X] = {.config_item1 = 30,.config_item2 = 24},

};

struct pcdev_private_data 
{
       struct pcdev_platform_data pdata;
       char* buffer;
       dev_t dev_num;
       struct cdev cdev;
};

struct pcdrv_private_data 
{
       int total_devices;
       dev_t device_num_base;
       struct class* class_pcd;
       struct device* device_pcd;
};

struct pcdrv_private_data pcdrv_data;

loff_t pcd_lseek(struct file *filp,loff_t  offset , int whence)
{
	return 0;
}

ssize_t pcd_read(struct file *filp, char __user *buff, size_t count, loff_t *f_pos)
{
	struct pcdev_private_data* pcdev_data = (struct pcdev_private_data*)filp->private_data;
	int max_size = pcdev_data->pdata.size;
	pr_info("read requested for %zu bytes\n",count);
	pr_info("current file position = %lld\n",*f_pos);

	if((*f_pos + count) > max_size){
	       count = max_size - *f_pos;
	}

	if(copy_to_user(buff,pcdev_data->buffer+(*f_pos),count)){
	         return -EFAULT;
	}

	*f_pos += count;

	     pr_info("Number of bytes successfully read = %zu\n",count);
        pr_info("Updated file position = %lld\n",*f_pos);

	return count;
}

ssize_t pcd_write(struct file *filp, const char __user *buff, size_t count, loff_t  *f_pos)
{	 struct pcdev_private_data* pcdev_data = (struct pcdev_private_data*)filp->private_data;

        int max_size = pcdev_data->pdata.size;

        pr_info("Write requested for %zu bytes\n",count);
        pr_info("Current file position = %lld\n",*f_pos);

        /* Adjust the 'count' */
        if((*f_pos + count) > max_size)
                count = max_size - *f_pos;

        if(!count){
                pr_err("No space left on the device \n");
                return -ENOMEM;
        }

        /*copy from user */
        if(copy_from_user(pcdev_data->buffer+(*f_pos),buff,count)){
                return -EFAULT;
        }
	*f_pos += count;

        pr_info("Number of bytes successfully written = %zu\n",count);
        pr_info("Updated file position = %lld\n",*f_pos);

        /*Return number of bytes which have been successfully written */
        return count;

}

int check_permission(int dev_perm,int acc_mode)
{
        if(dev_perm == RDWR)
               return 0;
        if((dev_perm == RDONLY) && (( acc_mode & FMODE_READ ) && !( acc_mode & FMODE_WRITE )))
                return 0;
        if((dev_perm == WRONLY) && (!( acc_mode & FMODE_READ ) && ( acc_mode & FMODE_WRITE )))
                return 0;

        return -EPERM;

}

int pcd_open(struct inode* inode,struct file* filp)
{
	int ret;
	int minor_n;
	struct pcdev_private_data *pcdev_data;
	
	minor_n = MINOR(inode->i_rdev);
	
	pr_info("minor number = %d\n",minor_n);
        
	pcdev_data = container_of(inode->i_cdev,struct pcdev_private_data,cdev);
	
	filp->private_data = pcdev_data;
	
	ret = check_permission(pcdev_data->pdata.perm,filp->f_mode);
	
	(!ret)?pr_info("open was successful\n"):pr_info("open was unsuccessfull ");


        return ret;
}

int pcd_release(struct inode* inode,struct file* filp)
{
  return 0;
}

struct file_operations pcd_fops = {
	.open = pcd_open,
        .release = pcd_release,
        .read = pcd_read,
        .write = pcd_write,
        .llseek = pcd_lseek,
        .owner = THIS_MODULE
};


int  pcd_platform_driver_remove(struct platform_device *pdev)
{
	struct pcdev_private_data *dev_data = dev_get_drvdata(&pdev->dev);
	device_destroy(pcdrv_data.class_pcd,dev_data->dev_num);

	cdev_del(&dev_data->cdev);
	
	pcdrv_data.total_devices--;
	pr_info("A device is removed\n");
	return 0;
}

int pcd_platform_driver_probe(struct platform_device *pdev)
{
	int ret;

	struct pcdev_private_data *dev_data;
	struct pcdev_platform_data *pdata;
	pr_info("device detected successfull\n");

	pdata = (struct pcdev_platform_data*)dev_get_platdata(&pdev->dev);
	if(!pdata){
	    pr_info("no platform data available\n");
	    return -EINVAL;
	}
	 
	dev_data = devm_kzalloc(&pdev->dev,sizeof(*dev_data),GFP_KERNEL);
	if(!dev_data){
	    pr_info("cannot alloc memory\n");
	    return -ENOMEM;
	}
 	
	/*pdev->dev.driver_data = dev_data;  OR */ 
	dev_set_drvdata(&pdev->dev,dev_data);

	dev_data->pdata.size = pdata->size;
	dev_data->pdata.perm = pdata->perm;
	dev_data->pdata.serial_number = pdata->serial_number;
	pr_info("deivce serial number = %s\n",dev_data->pdata.serial_number);
	pr_info("device size = %d\n",dev_data->pdata.size);
	pr_info("device permission = %d\n",dev_data->pdata.perm);
	pr_info("Config item1 = %d\n",pcdev_config[pdev->id_entry->driver_data].config_item1);
	pr_info("Config item2 = %d\n",pcdev_config[pdev->id_entry->driver_data].config_item2);


	dev_data->buffer = devm_kzalloc(&pdev->dev,dev_data->pdata.size,GFP_KERNEL);
        if(!dev_data->buffer){
	     pr_info("buffer mem alloc failed\n");
	     return -ENOMEM;
	     
	}

	dev_data->dev_num = pcdrv_data.device_num_base + pdev->id;
 
	cdev_init(&dev_data->cdev,&pcd_fops);
	dev_data->cdev.owner = THIS_MODULE;
	ret = cdev_add(&dev_data->cdev,dev_data->dev_num,1);
	if(ret<0){
		pr_info("cdev add failed\n");
		return ret;
	}

	pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd,NULL,dev_data->dev_num,NULL,"pcdev-%d",pdev->id);
	if(IS_ERR(pcdrv_data.device_pcd)){
		pr_err("device create failed\n");
		ret = PTR_ERR(pcdrv_data.device_pcd);
		cdev_del(&dev_data->cdev);
		return ret;
	}
	pcdrv_data.total_devices++;
	pr_info("A device probe was successsful\n");
	return 0;
}

struct platform_device_id pcdev_ids[] = {
	[0] = {.name = "pcdev-A1x",.driver_data = PCDEVA1X},
	[1] = {.name = "pcdev-B1x",.driver_data = PCDEVB1X},
	[2] = {.name = "pcdev-C1x",.driver_data = PCDEVC1X},
	[3] = {.name = "pcdev-D1x",.driver_data = PCDEVD1X}
};

struct platform_driver pcd_platform_driver = {
         .probe = pcd_platform_driver_probe,
	 .remove = pcd_platform_driver_remove,
	 .id_table = pcdev_ids,
	 .driver = {
	 	.name = "pseudo-char-device"
	 }
};

#define MAX_DEVICES 10
static int __init pcd_platform_driver_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&pcdrv_data.device_num_base,0,MAX_DEVICES,"pcdevs");

	if(ret < 0){
		pr_err("Alloc chardev failed\n");
	  	return ret;
	}	
	pcdrv_data.class_pcd = class_create(THIS_MODULE,"pcd_class");
	if(IS_ERR(pcdrv_data.class_pcd)){
		pr_err("class creation failed\n");
		ret = PTR_ERR(pcdrv_data.class_pcd);
	        unregister_chrdev_region(pcdrv_data.device_num_base,MAX_DEVICES);
		return ret;
	}
	platform_driver_register(&pcd_platform_driver);
	pr_info("pcd platform driver loaded\n");
	return 0;
}

static void __exit pcd_platform_driver_cleanup(void)
{
	platform_driver_unregister(&pcd_platform_driver);
 	class_destroy(pcdrv_data.class_pcd);
	unregister_chrdev_region(pcdrv_data.device_num_base,MAX_DEVICES);
	pr_info("pcd platform driver unloaded\n");
}


module_init(pcd_platform_driver_init);
module_exit(pcd_platform_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SP");
MODULE_DESCRIPTION("pcd platform driver");
