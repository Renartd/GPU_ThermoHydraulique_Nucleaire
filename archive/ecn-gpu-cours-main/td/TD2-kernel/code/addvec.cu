#include <stdio.h>

#define N (2048*2048) // number of vector values (for question 4, change this value to 666*42)
#define THREADS_PER_BLOCK 512 // Number of threads per blocks

__global__
void vector_add(int *a, int *b, int *c)
{
    /* insert code to calculate the index properly using blockIdx.x, blockDim.x, threadIdx.x */
	int index = /* FIXME */
	c[index] = a[index] + b[index];
}

int main()
{
  int *a, *b, *c, *ugly; // ugly is only use to show the effect of the second part
	int *d_a, *d_b, *d_c, *d_ugly;

	int size = N * sizeof( int ); // vector size in bytes

	/* allocate space for device copies of a, b, c */
	cudaMalloc( (void **) &d_a, size );
	cudaMalloc( (void **) &d_b, size );
	cudaMalloc( (void **) &d_c, size );
	cudaMalloc( (void **) &d_ugly, size );
	cudaMemset(d_ugly, 0x1223, N);

	/* allocate space for host copies of a, b, c and setup input values */
	a = (int *)malloc( size );
	b = (int *)malloc( size );
	c = (int *)malloc( size );
	ugly = (int *)malloc( size );

	for( int i = 0; i < N; i++ )
	{
		a[i] = b[i] = i;
		c[i] = 0;
	}

	/* copy inputs to device */
	/* fix the parameters needed to copy data to the device */
	cudaMemcpy( /* FIXME */ );
	cudaMemcpy( /* FIXME */ );

	/* launch the kernel on the GPU */
	/* insert the launch parameters to launch the kernel properly using blocks and threads */ 
	vector_add<<< /* FIXME */, /* FIXME */ >>>( d_a, d_b, d_c );

	/* copy result back to host */
	/* fix the parameters needed to copy data back to the host */
	cudaMemcpy( /* FIXME */ );
	cudaMemcpy( ugly, d_ugly, size, cudaMemcpyDeviceToHost );


	printf( "c[%d] = %d\n",0, c[0] );
	printf( "c[%d] = %d\n",N-1, c[N-1] );
	printf( "ugly[%d] = %d should be 589505315 \n",0, ugly[0] );

	/* clean up */
	free(a);
	free(b);
	free(c);
	cudaFree( d_a );
	cudaFree( d_b );
	cudaFree( d_c );
	
	return 0;
} /* end main */
