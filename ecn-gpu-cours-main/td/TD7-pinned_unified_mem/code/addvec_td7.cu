#include <stdio.h>

#define CREATE_CUDAEVENT cudaEvent_t start, stop; \
cudaEventCreate(&start); \
cudaEventCreate(&stop);

#define START_CUDAEVENT cudaEventRecord(start, 0);

#define STOP_AND_PRINT_CUDAEVENT(txt) cudaEventRecord(stop, 0);\
cudaEventSynchronize(stop);\
{float elapsedTime;\
cudaEventElapsedTime(&elapsedTime, start, stop);\
printf("Time to %s %3.1f ms\n", #txt, elapsedTime);}


__global__
void vector_add(int *a, int *b, int *c, int n)
{
    int index = blockIdx.x * blockDim.x + threadIdx.x;
	if (index < n)
		c[index] = a[index] + b[index];
}

/* experiment with N */
#define N (2048*2048)
#define THREADS_PER_BLOCK 512

int main()
{
	CREATE_CUDAEVENT;
	int size = N * sizeof( int );

	/*@@@@@@@@@@@@@ CpMem Classical Section @@@@@@@@@@@@@*/
	printf(">>> Results for MemCopy\n");
	int *h_a, *h_b, *h_c;
	int *d_a, *d_b, *d_c;

	START_CUDAEVENT;
	// Memory allocation on the host
	h_a = (int *)malloc( size );
	h_b = (int *)malloc( size );
	h_c = (int *)malloc( size );
	STOP_AND_PRINT_CUDAEVENT([Classical] host allocation);

	START_CUDAEVENT;
	for( int i = 0; i < N; i++ )
	{
		h_a[i] = h_b[i] = i;
		h_c[i] = 0;
	}
	STOP_AND_PRINT_CUDAEVENT([Classical] Initialize);

	START_CUDAEVENT;
	// Add the code to allocate the memory on the device
	STOP_AND_PRINT_CUDAEVENT([Classical] device allocation);

	START_CUDAEVENT;
	// Add the code to copy memory from host to device
	STOP_AND_PRINT_CUDAEVENT([Classical] MemCopy Host to Device);

	START_CUDAEVENT;
	vector_add<<< (N + (THREADS_PER_BLOCK-1)) / THREADS_PER_BLOCK, THREADS_PER_BLOCK >>>( d_a, d_b, d_c, N);
	STOP_AND_PRINT_CUDAEVENT([Classical] execution);

	START_CUDAEVENT;
	// Add the code to copy memory from device to host
	STOP_AND_PRINT_CUDAEVENT([Classical] MemCopy Device to Host);

	free(h_a);
	free(h_b);
	free(h_c);
	cudaFree( d_a );
	cudaFree( d_b );
	cudaFree( d_c );
	/*@@@@@@@@@@@@@ End CpMem Classical Section @@@@@@@@@@@@@*/


	/*@@@@@@@@@@@@@ CpMem Pinned Section @@@@@@@@@@@@@*/
	printf("\n>>> Results for Pinned Memory\n");
	int *h_ap, *h_bp, *h_cp;
	int *d_ap, *d_bp, *d_cp;

	START_CUDAEVENT;
	// Add the code to allocate Pinned memory on the device
	STOP_AND_PRINT_CUDAEVENT([Pinned] host allocation);
	
	START_CUDAEVENT;
	// UNcomment this part after have allocated the memory
	/*for( int i = 0; i < N; i++ )
	{
		h_ap[i] = h_bp[i] = i;
		h_cp[i] = 0;
	}*/
	STOP_AND_PRINT_CUDAEVENT([Pinned] initialize);

	START_CUDAEVENT;
	// Add the code to allocate the memory on the device
	STOP_AND_PRINT_CUDAEVENT([Pinned] device allocation);

	START_CUDAEVENT;
	// Add the code to copy memory from host to device
	STOP_AND_PRINT_CUDAEVENT([Pinned] MemCopy Host to Device);

	START_CUDAEVENT;
	vector_add<<< (N + (THREADS_PER_BLOCK-1)) / THREADS_PER_BLOCK, THREADS_PER_BLOCK >>>( d_ap, d_bp, d_cp, N );
	STOP_AND_PRINT_CUDAEVENT([Pinned] execution);

	START_CUDAEVENT;
	// Add the code to copy memory from device to host
	STOP_AND_PRINT_CUDAEVENT([Pinned] MemCopy Device to Host);

	cudaFreeHost(h_ap);
	cudaFreeHost(h_bp);
	cudaFreeHost(h_cp);
	cudaFree( d_ap );
	cudaFree( d_bp );
	cudaFree( d_cp );
	/*@@@@@@@@@@@@@ End CpMem Pinned Section @@@@@@@@@@@@@*/

	/*@@@@@@@@@@@@@ Unified Memory Section @@@@@@@@@@@@@*/
	printf("\n>>> Results for Unified Memory\n");
    int *a_um, *b_um, *c_um;	

	START_CUDAEVENT;
	// Add code to allocate Unified Memory
	STOP_AND_PRINT_CUDAEVENT([Unified Memory] memory allocation);

	START_CUDAEVENT;
	// Uncomment this part after have allocated the memory
	/*for( int i = 0; i < N; i++ )
	{
		a_um[i] = b_um[i] = i;
		c_um[i] = 0;
	}*/
	STOP_AND_PRINT_CUDAEVENT([Unified Memory] initialize);

	START_CUDAEVENT;
	/* MemCopy Part : nothing to do */
	STOP_AND_PRINT_CUDAEVENT([Unified Memory] MemCopy Host to Device);

	START_CUDAEVENT;
	vector_add<<< (N + (THREADS_PER_BLOCK-1)) / THREADS_PER_BLOCK, THREADS_PER_BLOCK >>>( a_um, b_um, c_um, N );
	cudaDeviceSynchronize();
	STOP_AND_PRINT_CUDAEVENT([Unified Memory] execution);

	START_CUDAEVENT;
	/* MemCopy Part Device To Host : nothing to do */
	STOP_AND_PRINT_CUDAEVENT([Unified Memory] MemCopy Device to Host);

	cudaFree( a_um );
	cudaFree( b_um );
	cudaFree( c_um );
	/*@@@@@@@@@@@@@ End Unified Memory Section @@@@@@@@@@@@@*/	

	cudaEventDestroy(start);
	cudaEventDestroy(stop);

	return 0;
} /* end main */
