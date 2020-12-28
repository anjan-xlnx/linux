obj-m := pcd_dev_setup.o pcd_platform_drv.o
#make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-  -C  /home/anji/ldd/sources/bbb_linux_4.14/ M=$PWD
ARCH=arm
CRS_CMPL=arm-linux-gnueabihf-
KERN_DIR=/home/anji/ldd/sources/bbb_linux_4.14/

HOST_KERN_DIR=/lib/modules/$(shell uname -r)/build

all:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CRS_CMPL) -C $(KERN_DIR) M=$(PWD) modules
clean:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CRS_CMPL) -C $(KERN_DIR) M=$(PWD) clean
help:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CRS_CMPL) -C $(KERN_DIR) M=$(PWD) help
host:
	make -C $(HOST_KERN_DIR) M=$(PWD) modules
host_clean:
	make -C $(HOST_KERN_DIR) M=$(PWD) clean
