# from: https://github.com/riscv/riscv-v-spec/blob/master/example/memcpy.s
.text
.balign 4

// This works for the current MangoPi MQ-Pro ubuntu server gcc version.
// Uncomment the following include, if you have the proper gcc 0.7.1
// toochain (https://github.com/brucehoult/riscv-gcc)
//#include "rvv-rollback.S"

.global rvv_memcpy
rvv_memcpy: # void *_(void* dest, const void* src, size_t n)
	mv	a3, a0 # Copy destination
loop:
	vsetvli	t0, a2, e8, m8  # Vectors of 8b
	vle8.v	v0, (a1)        # Load bytes
	add	a1, a1, t0      # Bump pointer
	sub	a2, a2, t0      # Decrement count
	vse8.v	v0, (a3)        # Store bytes
	add	a3, a3, t0      # Bump pointer
	bnez	a2, loop        # Any more?
	ret                     # Return
