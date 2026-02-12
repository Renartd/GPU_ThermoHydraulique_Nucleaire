#include <iostream>
#include <math.h>
#include <assert.h>
#include "error.h"

using namespace std;


void initMatrix(float *m, int numRows, int numCols);
void computeMatrixMulCPU(float *A, float *B, float *C, int numARows, int numAColumns, int numBRows, int numBColumns);
__global__ void computeMatrixMulGPU(float *A, float *B, float *C, int numARows, int numAColumns, int numBRows, int numBColumns);
bool compareMatrix(float *A, float *B, int numRows, int numColumns);
void testMatrixMulCPU();
void testMatrixMulGPU();

void computeMatrixMulCPU
(
   float *A, float *B, float *C,
   int numARows, int numAColumns,
   int numBRows, int numBColumns
)
{
   /* Complete this part to achieve the matrix multiplication on the CPU */
}

__global__ 
void computeMatrixMulGPU
(
   float *A, float *B, float *C,
   int numARows, int numAColumns,
   int numBRows, int numBColumns
)
{
   /* Complete this part to achieve the matrix multiplication on the GPU */
}

int main(int argc, char *argv[])
{
   
   int numARows; // number of rows in the matrix A
   int numAColumns; // number of columns in the matrix A
   int numBRows; // number of rows in the matrix B
   int numBColumns; // number of columns in the matrix B
   int numCRows; // number of rows in the matrix C
   int numCColumns; // number of columns in the matrix C 

   if (argc != 1)
   {
       numARows = atoi(argv[1]); // number of rows in the matrix A
       numAColumns = atoi(argv[2]); // number of columns in the matrix A
       numBRows = atoi(argv[3]); // number of rows in the matrix B
       numBColumns = atoi(argv[4]); // number of columns in the matrix B
       numCRows = numARows; // number of rows in the matrix C
       numCColumns = numBColumns; // number of columns in the matrix C 
       assert(numAColumns == numBRows);
   }
   else 
   {
       printf("Warning : Default values\n");
       numARows = 100; // number of rows in the matrix A
       numAColumns = 200; // number of columns in the matrix A
       numBRows = 200; // number of rows in the matrix B
       numBColumns = 400; // number of columns in the matrix B
       numCRows = numARows; // number of rows in the matrix C
       numCColumns = numBColumns; // number of columns in the matrix C 
   }

   // Test the multiplication of matrices with the CPU and GPU method
   testMatrixMulCPU();
   testMatrixMulGPU();

   /******************************************************/
   /*                   CPU PART                         */
   /******************************************************/
   float *A = (float *)malloc(numARows*numAColumns*sizeof(float));
   float *B = (float *)malloc(numBRows*numBColumns*sizeof(float));
   float *C = (float *)malloc(numCRows*numCColumns*sizeof(float));

   // Initialize matrices on the host
   initMatrix(A, numARows, numAColumns);
   initMatrix(B, numBRows, numBColumns);

   computeMatrixMulCPU(A, B, C, numARows, numAColumns, numBRows, numBColumns);

   /******************************************************/
   /*                   GPU PART                         */
   /******************************************************/    
   float *deviceA;
   float *deviceB;
   float *deviceC;

   float *hostC = (float *)malloc(numCRows*numCColumns*sizeof(float));

   // Memory allocation on the GPU
   CHECK_ERROR(cudaMalloc((void **)&deviceA, numARows * numAColumns * sizeof(float)));
   CHECK_ERROR(cudaMalloc((void **)&deviceB, numBRows * numBColumns * sizeof(float)));
   CHECK_ERROR(cudaMalloc((void **)&deviceC, numCRows * numBColumns * sizeof(float)));

   // Copy from CPU memory to GPU
   CHECK_ERROR(cudaMemcpy(deviceA, A, numARows * numAColumns * sizeof(float), cudaMemcpyHostToDevice));
   CHECK_ERROR(cudaMemcpy(deviceB, B, numBRows * numBColumns * sizeof(float), cudaMemcpyHostToDevice));

   // Set C to 0 (not necessary)
   CHECK_ERROR(cudaMemset(deviceC, 0, numCRows * numCColumns * sizeof(float)));
   
   // Launch kernel
   dim3 blockDim(16, 16);
   dim3 gridDim(ceil(((float)numBColumns) / blockDim.x), ceil(((float)numARows) / blockDim.y));
   computeMatrixMulGPU<<<gridDim, blockDim>>>(deviceA, deviceB, deviceC, numARows, numAColumns, numBRows, numBColumns);

   // Copy from GPU memory to CPU
   CHECK_ERROR(cudaMemcpy(hostC, deviceC, numARows * numBColumns * sizeof(float), cudaMemcpyDeviceToHost));

   CHECK_ERROR(cudaFree(deviceA));
   CHECK_ERROR(cudaFree(deviceB));
   CHECK_ERROR(cudaFree(deviceC));
   // END CUDA PART
   
   cout << endl << ">> Start compare CPU vs GPU <<" << endl;
   if (compareMatrix(C, hostC, numCRows, numCColumns)) {
       cout << ">> Test succeded " << endl;
   }
   else
   {
       cout << ">> Test Failed" << endl;
   }

   free(A);
   free(B);
   free(C);

   return 0;
}



void initMatrix(float *m, int numRows, int numCols){
   for (int i=0; i<numRows; i++){
       for (int j=0; j<numCols; j++){
           m[i*numCols+j] = sin(i*numCols+j);
       }
   }
}

bool compareMatrix(float *A, float *B, int numRows, int numColumns)
{
   float sum = 0.0;
   float max = 0.0;
   float min = 10.0;
   bool result = true;
   for (int row = 0; row < numRows; row++)
   {
       for (int col = 0; col < numColumns; col++)
       {
           if (A[row*numColumns+col] !=  B[row*numColumns+col])
           {
               result = false;
           }
           float diff = A[row*numColumns+col] - B[row*numColumns+col];
           if (diff > max) max = diff;
           if (diff < min) min = diff;
           sum += diff;
       }
   }
   cout << "mean: " << sum / (numRows*numColumns) << " max: " << max << " min: " << min << endl;
   return result;
}

void testMatrixMulCPU(){
   int numARows = 2; // number of rows in the matrix A
   int numAColumns = 3; // number of columns in the matrix A
   int numBRows = 3; // number of rows in the matrix B
   int numBColumns = 4; // number of columns in the matrix B
   int numCRows = 2; // number of rows in the matrix C
   int numCColumns = 4; // number of columns in the matrix C 

   float A[numARows*numAColumns] = {1, 2, 3, 4, 5, 6};
   float B[numBRows*numBColumns] = {1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4};
   float *C = (float *)malloc(numCRows*numCColumns*sizeof(float));
   float C_res[numCRows*numCColumns] = {6, 12, 18, 24, 15, 30, 45, 60};

   computeMatrixMulCPU(A, B, C, numARows, numAColumns, numBRows, numBColumns);
   cout << ">> Start test CPU <<" << endl;
   if (compareMatrix(C, C_res, numCRows, numCColumns)) {
       cout << ">> Test CPU succeded " << endl << endl;
   }
   else
   {
       cout << ">> Test CPU Failed" << endl << endl;
       exit(-1);
   }

   free(C);

}

void testMatrixMulGPU(){
   int numARows = 2; // number of rows in the matrix A
   int numAColumns = 3; // number of columns in the matrix A
   int numBRows = 3; // number of rows in the matrix B
   int numBColumns = 4; // number of columns in the matrix B
   int numCRows = 2; // number of rows in the matrix C
   int numCColumns = 4; // number of columns in the matrix C 

   float A[numARows*numAColumns] = {1, 2, 3, 4, 5, 6};
   float B[numBRows*numBColumns] = {1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4};
   float *C = (float *)malloc(numCRows*numCColumns*sizeof(float));
   float C_res[numCRows*numCColumns] = {6, 12, 18, 24, 15, 30, 45, 60};

   float *deviceA;
   float *deviceB;
   float *deviceC;

   CHECK_ERROR(cudaMalloc((void **)&deviceA, numARows * numAColumns * sizeof(float)));
   CHECK_ERROR(cudaMalloc((void **)&deviceB, numBRows * numBColumns * sizeof(float)));
   CHECK_ERROR(cudaMalloc((void **)&deviceC, numCRows * numBColumns * sizeof(float)));
   CHECK_ERROR(cudaMemcpy(deviceA, A, numARows * numAColumns * sizeof(float), cudaMemcpyHostToDevice));
   CHECK_ERROR(cudaMemcpy(deviceB, B, numBRows * numBColumns * sizeof(float), cudaMemcpyHostToDevice));
   CHECK_ERROR(cudaMemset(deviceC, 0, numCRows * numCColumns * sizeof(float)));

   dim3 blockDim(16, 16);
   dim3 gridDim(ceil(((float)numBColumns) / blockDim.x), ceil(((float)numARows) / blockDim.y));
   computeMatrixMulGPU<<<gridDim, blockDim>>>(deviceA, deviceB, deviceC, numARows, numAColumns, numBRows, numBColumns);

   CHECK_ERROR(cudaMemcpy(C, deviceC, numARows * numBColumns * sizeof(float), cudaMemcpyDeviceToHost));

   cout << ">> Start test GPU  <<"  << endl;
   if (compareMatrix(C, C_res, numCRows, numCColumns)) {
       cout << ">> Test GPU succeded " << endl << endl;
   }
   else
   {
       cout << ">> Test GPU Failed" << endl << endl;
       exit(-1);
   }

   CHECK_ERROR(cudaFree(deviceA));
   CHECK_ERROR(cudaFree(deviceB));
   CHECK_ERROR(cudaFree(deviceC));
   free(C);
}
