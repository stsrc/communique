ccflags-y += -DDEBUG -std=gnu99 -Wall -Wno-declaration-after-statement
ifneq ($(KERNELRELEASE),)
	obj-m := communique.o

else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

endif
