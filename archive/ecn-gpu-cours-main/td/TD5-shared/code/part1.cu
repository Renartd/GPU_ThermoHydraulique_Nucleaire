/*
 * Copyright 1993-2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and 
 * proprietary rights in and to this software and related documentation. 
 * Any use, reproduction, disclosure, or distribution of this software 
 * and related documentation without an express license agreement from
 * NVIDIA Corporation is strictly prohibited.
 *
 * Please refer to the applicable NVIDIA end user license agreement (EULA) 
 * associated with this source code for terms and conditions that govern 
 * your use of this NVIDIA software.
 * 
 */
 
 #include <iostream>
 #include "bitmap_image.hpp"

 #define DIM 1024
 #define PI 3.1415926535897932f
 
 __global__
 void kernel( unsigned char *ptr ) {
     // map from threadIdx/BlockIdx to pixel position
     int x = threadIdx.x + blockIdx.x * blockDim.x;
     int y = threadIdx.y + blockIdx.y * blockDim.y;
     int offset = x + y * blockDim.x * gridDim.x;
 
     __shared__ float shared[16][16];
 
     // now calculate the value at that position
     const float period = 128.0f;
 
     shared[threadIdx.x][threadIdx.y] =
             255 * (sinf(x*2.0f*PI/ period) + 1.0f) *
                   (sinf(y*2.0f*PI/ period) + 1.0f) / 4.0f;
 
     ptr[offset*3 + 0] = shared[15-threadIdx.x][15-threadIdx.y];
     ptr[offset*3 + 1] = 0;
     ptr[offset*3 + 2] = 255;

 }

 int main( void ) {
    int size = DIM*DIM*3*sizeof(unsigned char);
    unsigned char *h_ptr = (unsigned char *)malloc(size);
    unsigned char *d_ptr;

    cudaMalloc( (void**)&d_ptr, size );
 
    dim3    grids(DIM/16,DIM/16);
    dim3    threads(16,16);
    kernel<<<grids,threads>>>( d_ptr );

    cudaMemcpy( h_ptr, d_ptr, size, cudaMemcpyDeviceToHost );

    bitmap_image img(DIM,DIM);
    img.clear();
    for (int y = DIM-1; y >= 0; y--)
	{
		for (int x = DIM-1; x >= 0; x--)
		{
            int offset = x + y * DIM;
            img.set_pixel(x, y, h_ptr[offset*3], h_ptr[offset*3+1], h_ptr[offset*3+2]);            
        }
    }
    img.save_image("test.bmp");
    return 0;
 }
 
 
 