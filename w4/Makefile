obj-m := echodev.o
ARCH := arm
CROSS_COMPILE := arm-linux-gnueabi-
KERNELDIR ?= /home/zh/Desktop/Qualcomm_intern/linux-6.12.41

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
