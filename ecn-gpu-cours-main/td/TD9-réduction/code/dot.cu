 #include <stdio.h>
 #include <math.h>
 #include "error.h"
 
 const int VECTOR_SIZE = 1024 * 1024;
 const int THREAD_PER_BLOCK = 512;
 const int BLOCKS_PER_GRID = ceil(VECTOR_SIZE/ (2*THREAD_PER_BLOCK));
 
 double dot_cpu(double *a, double *b, int size)
 {
    double res = 0.0;
    for (int i = 0; i < size; i++)
    {
        res += a[i]*b[i];
    }
    return res;
 }

 __global__
 void dot_gpu(double *a, double *b, double *c, int size)
 {
    __shared__ double cache[2*THREAD_PER_BLOCK];

    int offset = 2 * blockIdx.x * blockDim.x;
    
    // set the cache values
    if ( offset + 2 * threadIdx.x < size)
    {
        cache[2*threadIdx.x] = a[offset + 2*threadIdx.x] * b[offset + 2*threadIdx.x];
        cache[2*threadIdx.x+1] = a[offset + 2*threadIdx.x + 1] * b[offset + 2*threadIdx.x + 1];
    }
    else
    {
        cache[2*threadIdx.x] = 0.0;
        cache[2*threadIdx.x+1] = 0.0;
    }

    /* Add your code to compute the sum of values in cache, 
    the result has to be stored in cache[0] */
     
    if (threadIdx.x == 0) // A single thread writes the result
        c[blockIdx.x] = cache[0];
 }

 __global__
 void dot_gpu_warp(double *a, double *b, double *c, int size)
 {
    __shared__ double cache[2*THREAD_PER_BLOCK];

    int offset = 2 * blockIdx.x * blockDim.x;
    
    // set the cache values
    if ( offset + 2 * threadIdx.x < size)
    {
        cache[2*threadIdx.x] = a[offset + 2*threadIdx.x] * b[offset + 2*threadIdx.x];
        cache[2*threadIdx.x+1] = a[offset + 2*threadIdx.x + 1] * b[offset + 2*threadIdx.x + 1];
    }
    else
    {
        cache[2*threadIdx.x] = 0.0;
        cache[2*threadIdx.x+1] = 0.0;
    }

    /* Add your code to compute the sum of values in cache, 
    the result has to be stored in cache[0] */
     
    if (threadIdx.x == 0) // A single thread writes the result
        c[blockIdx.x] = cache[0];
 }
 
 
 int main( void ) {
    printf("Vector size %d\nNumber of threads per blocks : %d\nNumber of blocks %d\n", VECTOR_SIZE, THREAD_PER_BLOCK, BLOCKS_PER_GRID);

     double   *h_a, *h_b, *partial_c;
     double   *d_a, *d_b, *d_partial_c;
     double   res;
     
     // allocate memory on the cpu side
     h_a = (double*)malloc( VECTOR_SIZE*sizeof(double) );
     h_b = (double*)malloc( VECTOR_SIZE*sizeof(double) );
     partial_c = (double*)malloc( BLOCKS_PER_GRID*sizeof(double) );
 
     // fill in the host memory with data
     for (int i = 0; i < VECTOR_SIZE; i++) {
         h_a[i] = sin(i);
         h_b[i] = cos(i);
     }

     // Dot calculation on the CPU
    double res_cpu = dot_cpu(h_a, h_b, VECTOR_SIZE);

    // allocate the memory on the GPU
    CHECK_ERROR(cudaMalloc( (void**)&d_a, VECTOR_SIZE*sizeof(double) ));
    CHECK_ERROR(cudaMalloc( (void**)&d_b, VECTOR_SIZE*sizeof(double) ));
    CHECK_ERROR(cudaMalloc( (void**)&d_partial_c, BLOCKS_PER_GRID*sizeof(double) ));

    // copy the arrays 'a' and 'b' to the GPU
    CHECK_ERROR(cudaMemcpy( d_a, h_a, VECTOR_SIZE*sizeof(double), cudaMemcpyHostToDevice ));
    CHECK_ERROR(cudaMemcpy( d_b, h_b, VECTOR_SIZE*sizeof(double), cudaMemcpyHostToDevice ));
    CHECK_ERROR(cudaMemset( d_partial_c, 0, BLOCKS_PER_GRID*sizeof(double) ));
 
    dot_gpu<<<BLOCKS_PER_GRID,THREAD_PER_BLOCK>>>( d_a, d_b, d_partial_c, VECTOR_SIZE);
    CHECK_ERROR(cudaGetLastError());
    CHECK_ERROR(cudaDeviceSynchronize());
 
    // copy the array 'c' back from the GPU to the CPU
    CHECK_ERROR(cudaMemcpy( partial_c, d_partial_c, BLOCKS_PER_GRID*sizeof(double), cudaMemcpyDeviceToHost ));
 
    // finish up on the CPU side
    res = 0;
    for (int i=0; i<BLOCKS_PER_GRID; i++) {
        res += partial_c[i];
    }

    if (abs(res - res_cpu) < 0.00001)
        printf( "GPU calculation (%.6g) matches CPU calculation (%.6g) [FPU error of %.6g]\n", res, res_cpu, abs(res-res_cpu) );
    else
        printf( "GPU calculation (%.6g) does not match CPU calculation (%.6g)\n", res, res_cpu );
    
    // Reset c
    cudaMemset( d_partial_c, 0, BLOCKS_PER_GRID*sizeof(double) );

    dot_gpu_warp<<<BLOCKS_PER_GRID,THREAD_PER_BLOCK>>>( d_a, d_b, d_partial_c, VECTOR_SIZE);
    CHECK_ERROR(cudaGetLastError());
    CHECK_ERROR(cudaDeviceSynchronize());   
        
    // copy the array 'c' back from the GPU to the CPU
    CHECK_ERROR(cudaMemcpy( partial_c, d_partial_c, BLOCKS_PER_GRID*sizeof(double), cudaMemcpyDeviceToHost ));
 
     // finish up on the CPU side
     res = 0;
     for (int i=0; i<BLOCKS_PER_GRID; i++) {
         res += partial_c[i];
     }
    
    if (abs(res - res_cpu) < 0.00001)
        printf( "GPU calculation (%.6g) matches CPU calculation (%.6g) [FPU error of %.6g]\n", res, res_cpu, abs(res-res_cpu) );
    else
        printf( "GPU calculation (%.6g) does not match CPU calculation (%.6g)\n", res, res_cpu );
 
     // free memory on the gpu side
     cudaFree( d_a );
     cudaFree( d_b );
     cudaFree( d_partial_c );
 
     // free memory on the cpu side
     free( h_a );
     free( h_b );
     free( partial_c );
 }
 