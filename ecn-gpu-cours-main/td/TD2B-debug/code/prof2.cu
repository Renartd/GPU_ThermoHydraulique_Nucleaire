#include <cstdio>
#include <cstdlib>
#include "error.h"


#define IDX(row, col, LDA) ((row)*(LDA)+(col))

//computes c(i,j) = a(i,j) + b(i,j)
void add_v0(int *a, int *b, int *c, int N, int M) {
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < M; j++){
      int idx=IDX(i,j,M);
      c[idx] = a[idx] + b[idx];
    }
  }
}

//computes c(i,j) = a(i,j) + b(i,j)
//In this case i is the fastest changing thread dimension
__global__ void add_v1(int *a, int *b, int *c, int N, int M) {
  int i=blockIdx.x*blockDim.x+threadIdx.x;
  int j=blockIdx.y*blockDim.y+threadIdx.y;
  if(i<N && j<M) {
    int idx=IDX(i,j,M);
    c[idx] = a[idx] + b[idx];
  }
}

//computes c(i,j) = a(i,j) + b(i,j)
//In this case j is the fastest changing thread dimension
__global__ void add_v2(int *a, int *b, int *c, int N, int M) {
  int i=blockIdx.y*blockDim.y+threadIdx.y;
  int j=blockIdx.x*blockDim.x+threadIdx.x;
  if(i<N && j<M) {
    int idx=IDX(i,j,M);
    c[idx] = a[idx] + b[idx];
  }
}


int main() {
  int N=10*1024;
  int M=10*1024;
  dim3 threads(32,32);
  dim3 blocks(N/threads.x, M/threads.y);

  //////////////////////
  int *a, *b, *c;
	int *d_a, *d_b, *d_c;
	int size = N * M * sizeof( int );

  /* allocate space for device copies of a, b, c */

	CHECK_ERROR(cudaMalloc( (void **) &d_a, size ));
	CHECK_ERROR(cudaMalloc( (void **) &d_b, size ));
	CHECK_ERROR(cudaMalloc( (void **) &d_c, size ));

	/* allocate space for host copies of a, b, c and setup input values */

	a = (int *)malloc( size );
	b = (int *)malloc( size );
	c = (int *)malloc( size );

	for( int i = 0; i < N; i++ )
	{
		a[i] = b[i] = i;
		c[i] = 0;
	}

  add_v0(a, b, c, N, M);

	/* copy inputs to device */
	/* fix the parameters needed to copy data to the device */
	CHECK_ERROR(cudaMemcpy( d_a, a, size, cudaMemcpyHostToDevice ));
	CHECK_ERROR(cudaMemcpy( d_b, b, size, cudaMemcpyHostToDevice ));

  add_v1<<<blocks,threads>>>(d_a,d_b,d_c,N,M);
  CHECK_ERROR(cudaGetLastError());
  CHECK_ERROR(cudaDeviceSynchronize());

  add_v2<<<blocks,threads>>>(d_a,d_b,d_c,N,M);
  CHECK_ERROR(cudaGetLastError());
  CHECK_ERROR(cudaDeviceSynchronize());

  /* copy result back to host */
	/* fix the parameters needed to copy data back to the host */
	CHECK_ERROR(cudaMemcpy( c, d_c, size, cudaMemcpyDeviceToHost ));

  CHECK_ERROR(cudaFree(d_a));
  CHECK_ERROR(cudaFree(d_b));
  CHECK_ERROR(cudaFree(d_c));

  free(a);
  free(b);
  free(c);

  return 0;
}
