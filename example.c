// from: https://github.com/riscv-non-isa/rvv-intrinsic-doc/blob/master/examples/rvv_memcpy.c
#include <string.h>

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void gen_rand_1d(double *a, int n) {
	for (int i = 0; i < n; ++i)
		a[i] = (double)rand() / (double)RAND_MAX + (double)(rand() % 1000);
}

bool double_eq(double golden, double actual, double relErr) {
	  return (fabs(actual - golden) < relErr);
}

bool compare_1d(double *golden, double *actual, int n) {
	  for (int i = 0; i < n; ++i)
		      if (!double_eq(golden[i], actual[i], 1e-6))
			            return false;
	    return true;
}

extern void *rvv_memcpy(void* dest, const void* src, size_t n);

int main() {
	const int N = 127;
	const uint32_t seed = 0xdeadbeef;
	srand(seed);

	// data gen
	double A[N];
	gen_rand_1d(A, N);

	// compute
	double golden[N], actual[N];
	memcpy(golden, A, sizeof(A));
	rvv_memcpy(actual, A, sizeof(A));
	puts(compare_1d(golden, actual, N) ? "pass" : "fail");
	return 0;
}

