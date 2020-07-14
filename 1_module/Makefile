obj-m += temp.o param.o

param-objs := mod_param.o
all:
	make -C /lib/modules/${shell uname -r}/build M=${PWD} modules
clean:
	make -C /lib/modules/${shell uname -r}/build M=${PWD} clean
