
obj-m := pcd_device_setup.o pcd_platform_driver.o

HOST_KERN_DIR = /lib/modules/$(shell uname -r)/build/

host:
	make -C $(HOST_KERN_DIR) M=$(PWD) modules

clean:
	make -C $(HOST_KERN_DIR) M=$(PWD) clean
