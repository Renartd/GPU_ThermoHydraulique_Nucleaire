#include <stdio.h>

#define N 4000000
#define RADIUS 40
#define THREADS_PER_BLOCK 512

__global__
void stencil_1d(int n, double *in, double *out)
{
 // calculate thread index in the array
 int thread_index = threadIdx.x + blockIdx.x * blockDim.x;
    
 // return if my thread index is larger than the array size
 if( thread_index < n )
 {
   double result = 0.0;
   for( int i = thread_index - RADIUS; i <= thread_index + RADIUS; i++ ) 
   {
     // Do nothing if you are outside the table in
     if (i > 0 && i < n)
     {
      result += in[i];
     }
   } 
   out[thread_index] = result;    
 }
}

__global__
void shared_stencil_1d(int n, double *in, double *out)
{
  // Add the code here to implement a solution using shared memory
}

int main()
{  
  double *in, *out_not_shared, *out_shared;
  double *d_in, *d_out;
  int size = N * sizeof( double );

 /* allocate space for host copies of in, out and setup input values */ 
 in = (double *)malloc( size );
 out_not_shared = (double *)malloc( size );
 out_shared = (double *)malloc( size );

 for( int i = 0; i < N; i++ )
 {
   in[i] = (double) i;
   out_not_shared[i] = 0.;
   out_shared[i] = 0.;
 }

 /* allocate space for device copies of in, out */ 
 cudaMalloc( (void **) &d_in, size );
 cudaMalloc( (void **) &d_out, size );

 /* copy inputs to device */ 
 cudaMemcpy( d_in, in, size, cudaMemcpyHostToDevice );
 cudaMemset( d_out, 0, size );

 /* calculate block and grid sizes */
 dim3 threads( THREADS_PER_BLOCK, 1, 1);
 /* insert code for proper number of blocks in X dimension */
 dim3 blocks( N / THREADS_PER_BLOCK+1, 1, 1);

 /* launch the kernel stencil_1d */
 stencil_1d<<< blocks, threads >>>( N, d_in, d_out );
 /* copy result back to host */
 cudaMemcpy(out_not_shared, d_out, size, cudaMemcpyDeviceToHost );

 /* Set to zero d_out */
 cudaMemset( d_out, 0, size ); 
 /* launch the kernel shared_stencil_1d */
 shared_stencil_1d<<< blocks, threads >>>( N, d_in, d_out );
 /* copy result back to host */
 cudaMemcpy(out_shared, d_out, size, cudaMemcpyDeviceToHost );

 // compares the calculation with shared memory with the calculation without
 for( int i = 0; i < N; i++ )
 {
   if( out_not_shared[i] != out_shared[i] ) 
   {
     printf("error in element %d out_not_shared = %f vs out_shared %f\n",i,out_not_shared[i],out_shared[i] );
     printf("Try it again, you'll end up doing it\n");
     exit(-1);
   }
 }
 printf("Congratulations on your incredible success! I always knew you could do it, and I'm incredibly proud of you\n");
  
 /* clean up */
 free(in);
 free(out_not_shared);
 free(out_shared);

 cudaFree( d_in );
 cudaFree( d_out );
 cudaDeviceReset();
 
 return 0;
}