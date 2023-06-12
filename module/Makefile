obj-m := rvv.o
rvv-y := main.o asm.o

PWD := $(CURDIR)

ccflags-y := -march=rv64gcv0p7
asflags-y := -march=rv64gcv0p7

modules:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

