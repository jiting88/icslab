/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int i,j,k,t=0;
    int tmp0,tmp1,tmp2,tmp3,tmp4,tmp5,tmp6,tmp7;
    if(M==32 && N==32){
        for(i=0;i<4;i++){
            for(j=0;j<4;j++){
                for(k=0;k<8;k++){
                    tmp0=A[8*i+k][8*j+0];
                    tmp1=A[8*i+k][8*j+1];
                    tmp2=A[8*i+k][8*j+2];
                    tmp3=A[8*i+k][8*j+3];
                    tmp4=A[8*i+k][8*j+4];
                    tmp5=A[8*i+k][8*j+5];
                    tmp6=A[8*i+k][8*j+6];
                    tmp7=A[8*i+k][8*j+7];
                    B[8*j+0][8*i+k]=tmp0;
                    B[8*j+1][8*i+k]=tmp1;
                    B[8*j+2][8*i+k]=tmp2;
                    B[8*j+3][8*i+k]=tmp3;
                    B[8*j+4][8*i+k]=tmp4;
                    B[8*j+5][8*i+k]=tmp5;
                    B[8*j+6][8*i+k]=tmp6;
                    B[8*j+7][8*i+k]=tmp7;
                }
            }
        }
    }
    if(M==64 && N==64){
        for(i=0;i<8;i++){
            for(j=0;j<8;j++){
                for(k=0;k<4;k++){
                    tmp0=A[8*i+k][8*j+0];
                    tmp1=A[8*i+k][8*j+1];
                    tmp2=A[8*i+k][8*j+2];
                    tmp3=A[8*i+k][8*j+3];
                    tmp4=A[8*i+k][8*j+4];
                    tmp5=A[8*i+k][8*j+5];
                    tmp6=A[8*i+k][8*j+6];
                    tmp7=A[8*i+k][8*j+7];
                    B[8*j+0][8*i+k]=tmp0;
                    B[8*j+1][8*i+k]=tmp1;
                    B[8*j+2][8*i+k]=tmp2;
                    B[8*j+3][8*i+k]=tmp3;
                    B[8*j+0][8*i+k+4]=tmp4;
                    B[8*j+1][8*i+k+4]=tmp5;
                    B[8*j+2][8*i+k+4]=tmp6;
                    B[8*j+3][8*i+k+4]=tmp7;
                    test=B[4][0];
                    
                }
                for(k=0;k<4;k++){
                    tmp0=B[8*j+k][8*i+4];
                    tmp1=B[8*j+k][8*i+5];
                    tmp2=B[8*j+k][8*i+6];
                    tmp3=B[8*j+k][8*i+7];
                    test=B[4][0];
                    B[8*j+k][8*i+4]=A[8*i+4][8*j+k];
                    B[8*j+k][8*i+5]=A[8*i+5][8*j+k];
                    B[8*j+k][8*i+6]=A[8*i+6][8*j+k];
                    B[8*j+k][8*i+7]=A[8*i+7][8*j+k];
                    test=B[4][0];
                    B[8*j+k+4][8*i+0]=tmp0;
                    B[8*j+k+4][8*i+1]=tmp1;
                    B[8*j+k+4][8*i+2]=tmp2;
                    B[8*j+k+4][8*i+3]=tmp3;
                    
                }
                for(k=4;k<8;k++){
                    tmp0=A[8*i+4][8*j+k];
                    tmp1=A[8*i+5][8*j+k];
                    tmp2=A[8*i+6][8*j+k];
                    tmp3=A[8*i+7][8*j+k];
                    B[8*j+k][8*i+4]=tmp0;
                    B[8*j+k][8*i+5]=tmp1;
                    B[8*j+k][8*i+6]=tmp2;
                    B[8*j+k][8*i+7]=tmp3;
                    test=B[4][0];
                }
            }
        }
    }
    if(M==61 && N==67){
        for(i=0;i<5;i++){
            for(j=0;j<5;j++){
                for(k=0;k<16;k++){
                    for(t=0;t<16;t++){
                        if(16*i+k<67 && 16*j+t<61){
                            tmp0=A[16*i+k][16*j+t];
                            B[16*j+t][16*i+k]=tmp0;
                        }
                    }
                }
            }
        }
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

