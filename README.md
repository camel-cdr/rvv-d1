# Enabling rvv on Allwinner D1/C906 Linux


Thanks to brucehoult's comments on [my reddit post](https://old.reddit.com/r/RISCV/comments/13ik3xa/linux_image_for_allwinner_d1_with_vector/), I finally figured out how to enable the vector extension on the Allwinner D1.

It's actually really simple, you just need to set `sstatus.VS` to `1` in supervisor mode.
But what took me a while to figure out is, that the `sstatus.VS=1` mask doesn't have the value `0x600` on the D1, as it does in the ratified spec, but rather, `0x1800000`.
I only figured this out by the power of open source, and looking at the [C906 Verilog code](https://github.com/T-head-Semi/openc906/blob/bd92068b14321fc219a22d5c6108f9adc8315d54/C906_RTL_FACTORY/gen_rtl/cp0/rtl/aq_cp0_trap_csr.v#L478).


## Kernel Module

The easiest way to play around with rvv is to just execute the following code in a kernel module:

```asm
li	t0, 0x01806000 # SR_VS | SR_FS
csrs	0x100, t0  # CSR_STATUS
ret
```

A basic kernel module that implements this is included in [./module/](./module/), you can use it as follows:

```sh
cd module
make
sudo dmesg -WH # in seperate window, to view the output
sudo insmod rvv.ko # insert module
sudo rmmod rvv # remove module
```

## Binary patch linux 5.19

Using rvv in kernel space isn't all that great, and since I couldn't figure out how to compile a working kernel, monkey patching will have to do.

The idea is to find a place where `sstatus` is set in the kernel and add the `SR_VS|SR_FS` options to that.
Such a location isn't trivial to find, because you need to have enough redundant bytes, that you can insert a potentially slightly larger load immediate or other extra instructions.

Note, that this approach doesn't work with context switches across cpu cores (hearts). I don't have such hardware at hand, but I suspect, that you'd need to run your rvv application with `taskset -n x`.

So, after a lot of trial and error I found a suitable location:

```c
void start_thread(struct pt_regs *regs, unsigned long pc,
	unsigned long sp)
{
	regs->status = SR_PIE;
	if (has_fpu()) {
		regs->status |= SR_FS_INITIAL;
		/*
		 * Restore the initial value to the FP register
		 * before starting the user program.
		 */
		fstate_restore(current, regs);
	}
	regs->epc = pc;
	regs->sp = sp;

#ifdef CONFIG_64BIT
	regs->status &= ~SR_UXL;

	if (is_compat_task())
		regs->status |= SR_UXL_32;
	else
		regs->status |= SR_UXL_64;
#endif
}
```

This sets up the initial value of `regs->status`, which is later loaded by `restore_all`, which returns from supervisor to user mode.

Here is the end of the generated assembly, you can generate the dissasembly using `objdump -b binary -m riscv:rv64 -D`:

```asm
sd	a5,256(s1) # 23b0f410 (this stores into regs->status)
ld	ra,40(sp)  # a270
ld	s0,32(sp)  # 0274
ld	s1,24(sp)  # e264
ld	s2,16(sp)  # 4269
ld	s3,8(sp)   # a269
add	sp,sp,48   # 4561
li	a0,0       # 0145
li	a1,0       # 8145
li	a2,0       # 0146
li	a3,0       # 8146
li	a4,0       # 0147
li	a5,0       # 8147
ret                # 8280
li	a5,1       # 8547
sll	a5,a5,0x21 # 8617
add	a5,a5,32   # 93870702
j	0x 40d2    # c9bf
# 23b0f410a2700274e2644269a269456101458145014681460147814782808547861793870702c9bf
```

Let's see, if we can find this in our vmlinuz:

```sh
xxd -p -c 10000 /boot/vmlinuz-5.19.0-1009-allwinner | grep -no 23b0f410a2700274e2644269a269456101458145014681460147814782808547861793870702c9bf
```

You should only see one result.
Now is a good time to make a backup of the vmlinuz:

```sh
cp /boot/vmlinuz-5.19.0-1009-allwinner /boot/vmlinuz-5.19.0-1009-allwinner.bak
```

From reading the surrounding assembly, I expected the part after `ret` to be executed, but playing around with turning instructions into `nop` (`0100`), showed that this isn't the case.

So now we can simply add the following two instructions to the beginning of the code:

```asm
li	a4, 0x01806000 # 37678001
or	a5, a5, a4     # d98f
```

and remove the appropriated bytes from the unused code at the bottom.

So the final byte code we can replace the bytes code from above with is the following:

```asm
li	a4, 0x01806000 # 37678001
or	a5, a5, a4     # d98f
sd	a5,256(s1)     # 23b0f410
ld	ra,40(sp)      # a270
ld	s0,32(sp)      # 0274
ld	s1,24(sp)      # e264
ld	s2,16(sp)      # 4269
ld	s3,8(sp)       # a269
add	sp,sp,48       # 4561
li	a0,0           # 0145
li	a1,0           # 8145
li	a2,0           # 0146
li	a3,0           # 8146
li	a4,0           # 0147
li	a5,0           # 8147
ret                    # 8280
nop                    # 0100
nop                    # 0100
# 37678001d98f23b0f410a2700274e2644269a2694561014581450146814601478147828001000100
```

Now we just need to do the substitution and hope for the reboot to work:

```sh
xxd -p -c 10000 /boot/vmlinuz-5.19.0-1009-allwinner.bak | sed '0,/23b0f410a2700274e2644269a269456101458145014681460147814782808547861793870702c9bf/s//37678001d98f23b0f410a2700274e2644269a2694561014581450146814601478147828001000100/' | xxd -p -r > /boot/vmlinuz-5.19.0-1009-allwinner
```

I used the following to verify that everything was replaced correctly:

```sh
dhex /boot/vmlinuz-5.19.0-1009-allwinner*
```

## Extra, enable `rdcycle`

For some reason, `rdcycle` always returned the same value for me, so after reading up on it, I came to the conclusion that `mcountinhibit` probably disables it.
After a bit of digging in the partitions, I found that on my distro (ubuntu server) `/dev/mmcblk0p13` contains Risc-V code that sets `mcountinhibit`.

Luckily, all places where it is set, had the following pattern:

```asm
4 bytes setting a5       # XXXXXXXX
csrw	mcountinhibit,a5 # 73900732
```

So we want to replace the bytes that set a5 with the following:

```asm
li	a5, 0 # 8147
```

The substitution can be done as follows:

```sh
dd if=/dev/mmcblk0p13 of=./bin status=progress
xxd -p -c 10000 ./bin | sed 's/........73900732/8147814773900732/g' | xxd -p -r > tmp
dd if=./mod of=/dev/mmcblk0p13 status=progress
```
