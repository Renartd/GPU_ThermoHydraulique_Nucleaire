#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  int deviceCount;
  cudaGetDeviceCount(&deviceCount);
  int device;
  
  for (device = 0; device < deviceCount; ++device) {
    cudaDeviceProp deviceProp;
    cudaGetDeviceProperties(&deviceProp, device);
    printf("Device %d has compute capability %d.%d.\n", device,
           deviceProp.major, deviceProp.minor);
  }

  return 0;
}