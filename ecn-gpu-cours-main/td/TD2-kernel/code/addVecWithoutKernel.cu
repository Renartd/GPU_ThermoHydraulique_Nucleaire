#include <stdio.h>

/* experiment with N */
/* how large can it be? */
#define N (2048*2048)
#define THREADS_PER_BLOCK 512

int main()
{
	/* declare and create CUDA events */
	cudaEvent_t start, stop;
	cudaEventCreate(&start);
	cudaEventCreate(&stop);

    int *a, *b, *c;
	int size = N * sizeof( int );

	/* allocate space for host copies of a, b, c and setup input values */
	a = (int *)malloc( size );
	b = (int *)malloc( size );
	c = (int *)malloc( size );

	for( int i = 0; i < N; i++ )
	{
		a[i] = b[i] = i;
		c[i] = 0;
	}

	/* record start time */
	cudaEventRecord(start, 0);
	
	/* insert the launch parameters to launch the kernel properly using blocks and threads */ 
	for (int index = 0; index < N; index++) 
		c[index] = a[index] + b[index];

	/* record finish time */
	cudaEventRecord(stop, 0);
	/* wait GPU event */
	cudaEventSynchronize(stop);

	/* compute and print ellapsed time between start and stop */
	float elapsedTime;
	cudaEventElapsedTime(&elapsedTime, start, stop);
	printf("Time to execute %3.1f ms\n", elapsedTime);
	cudaEventDestroy(start);
	cudaEventDestroy(stop);

	/* clean up */
	free(a);
	free(b);
	free(c);
	
	return 0;
} /* end main */
