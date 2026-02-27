#include <stdio.h>
 
__global__ void kernelA(int * globalArray){
    int globalThreadId = blockIdx.x * blockDim.x + threadIdx.x;
    globalArray[globalThreadId] = globalThreadId;
}

 
int main()
{
    int elementCount = 32;
    int dataSize = elementCount * sizeof(int);
     
    cudaSetDevice(5);
     
    int * managedArray;
    cudaMallocManaged(&managedArray, dataSize * 1000000000);
    
    kernelA <<<4,8>>>(managedArray);
     
    cudaFree(managedArray);    
    cudaDeviceReset();
    
    return 0;
}