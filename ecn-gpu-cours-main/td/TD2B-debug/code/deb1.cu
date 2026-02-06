#include <cstdio>
#include <cstdlib>
#include "error.h"

__global__ void foo(int *a, int N) {
  int i=blockIdx.x*blockDim.x+threadIdx.x;
  a[i]=i;
}

int main() {
  
  int N=60;
  int threads=32;
  int blocks=(N+threads-1)/threads;
  int * h_a;
  int * d_a;

  h_a = (int *)malloc(N*sizeof(int));
  CHECK_ERROR(cudaMalloc( (void **) &d_a, N*sizeof(int)));
  foo<<<blocks,threads>>>(d_a, N);
  
  CHECK_ERROR(cudaGetLastError());
  CHECK_ERROR(cudaDeviceSynchronize());

  CHECK_ERROR(cudaMemcpy(h_a, d_a, N*sizeof(int), cudaMemcpyDeviceToHost ));

  for(int i=0;i<10;i++)
    printf("%d\n",h_a[i]);

  cudaFree(d_a);
  free(h_a);

  return 0;
}
