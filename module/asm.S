.text
.balign 4

.global rvv_enable
rvv_enable:
	li	t0, 0x01806000 # SR_VS
	csrs	0x100, t0  # CSR_STATUS
	vsetvli a0, zero, e8, m1
	ret

