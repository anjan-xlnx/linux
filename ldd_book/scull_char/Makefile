obj-m := scull.o

#KDIR=/root/linux-hwe-4.15.0
KDIR=/lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean 
