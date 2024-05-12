/*
 * Copyright 2019 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * usage: ./gemm_test.exe PATH_TO_XCLBIN/gemx.xclbin PATH_TO_XCLBIN/config_info.dat
 *
 */

#include <iomanip>
#include <cmath>
#include "xf_blas.hpp"
#include <iostream>
#include <cstdint>
#include <cstring>

#define IDX2R(i, j, ld) (((i) * (ld)) + (j))
#define m 5 // a - mxk matrix
#define n 5 // b - kxn matrix
#define k 5 // c - mxn matrix

using namespace std;

BLAS_dataType* getGoldenMat(BLAS_dataType* a, BLAS_dataType* b, BLAS_dataType* c) {
    BLAS_dataType* goldenC;
    goldenC = (BLAS_dataType*)malloc(m * n * sizeof(BLAS_dataType));
    for (int row = 0; row < m; row++) {
        for (int col = 0; col < n; col++) {
            BLAS_dataType l_val = 0;
            for (int i = 0; i < k; i++) {
                l_val += a[IDX2R(row, i, k)] * b[IDX2R(i, col, n)];
            }
            goldenC[IDX2R(row, col, n)] = l_val + c[IDX2R(row, col, n)];
        }
    }
    return goldenC;
}

bool compareGemm(BLAS_dataType* c, BLAS_dataType* goldenC, float p_TolRel = 1e-3, float p_TolAbs = 1e-5) {
    bool l_check = true;
    for (int row = 0; row < m; row++) {
        for (int col = 0; col < n; col++) {
            BLAS_dataType l_ref = goldenC[IDX2R(row, col, n)];
            BLAS_dataType l_result = c[IDX2R(row, col, n)];
            float l_diffAbs = abs(l_ref - l_result);
            float l_diffRel = l_diffAbs;
            if (goldenC[IDX2R(row, col, n)] != 0) {
                l_diffRel /= abs(l_ref);
            }
            bool check = (l_diffRel <= p_TolRel) || (l_diffAbs <= p_TolAbs);
            if (!check) {
                cout << "golden result " << setprecision(10) << goldenC[IDX2R(row, col, n)]
                     << " is not equal to fpga result " << setprecision(10) << c[IDX2R(row, col, n)] << "\n";
                l_check = false;
            }
        }
    }
    return l_check;
}

int main(int argc, char **argv) {

    if (argc < 3){
        cerr << " usage: \n"
            << " gemm_pre_allocated_test.exe gemx.xclbin config_info.dat\n";
        return EXIT_FAILURE;
    }
    unsigned int l_argIdx = 1;
    string l_xclbinFile(argv[l_argIdx++]);
    string l_configFile(argv[l_argIdx++]);
    string l_logFile;

    ofstream logFile("xrt_report.txt");
    logFile.close();
    l_logFile = "xrt_report.txt";

    int i, j; // i-row index ,j- column index

    BLAS_dataType * a, * b, * c;

    int padded_lda, padded_ldb, padded_ldc;

    xfblasEngine_t engineName = XFBLAS_ENGINE_GEMM;
    xfblasStatus_t status = XFBLAS_STATUS_SUCCESS;

    status = xfblasCreate(l_xclbinFile.c_str(), l_configFile, l_logFile.c_str(), engineName);
    if (status != XFBLAS_STATUS_SUCCESS) {
        cout<<"Create Handle failed with error code: "<< status << "\n";
        return EXIT_FAILURE;
    }

    status = xfblasMallocManaged(&a, &padded_lda, m,k,sizeof(*a));

    if (status != XFBLAS_STATUS_SUCCESS) {
        cout<<"Malloc memory for matrix A failed with error code: "<< status << "\n";
        return EXIT_FAILURE;
    }
    status = xfblasMallocManaged(&b, &padded_ldb, k,n,sizeof(*b));

    if (status != XFBLAS_STATUS_SUCCESS) {
        cout<<"Malloc memory for matrix B failed with error code: "<< status << "\n";
        return EXIT_FAILURE;
    }

    status = xfblasMallocManaged(&c, &padded_ldc, m,n,sizeof(*c));

    if (status != XFBLAS_STATUS_SUCCESS) {
        cout<<"Malloc memory for matrix C failed with error code: "<< status << "\n";
        return EXIT_FAILURE;
    }

    int ind = 1;

    for( i = 0; i<  m; i ++){
        for( j = 0; j < k; j ++){
            a[ IDX2R (i,j,padded_lda)]=( BLAS_dataType ) ind++;
        }
    }

    for( i = 0; i<  k; i ++){
        for( j = 0; j < n; j ++){
            b[ IDX2R (i,j,padded_ldb )]=( BLAS_dataType ) ind++;
        }
    }

    for( i = 0; i<  m; i ++){
        for( j = 0; j < n; j ++){
            c[ IDX2R (i,j,padded_ldc )]= 1;
        }
    }

    cout<< "C before running GEMM\n";

    for ( i = 0; i < m; i ++){
            for ( j = 0; j < n; j ++){
                cout<< (c[ IDX2R (i,j,padded_ldc)])<<" ";
            }
            cout<<"\n";
    }

    status = xfblasGemm(XFBLAS_OP_N, XFBLAS_OP_N, m, n, k, 1, a, k, b, n, 1, c, n);

    status = xfblasDeviceSynchronize();

    if (status != XFBLAS_STATUS_SUCCESS) {
        cout<<"Matrix Multiplication failed with error code: "<< status << "\n";
        return EXIT_FAILURE;
    }

    cout<<"C after running GEMM\n";

    for ( i = 0; i < m; i ++){
            for ( j = 0; j < n; j ++){
                cout<< (c[ IDX2R (i,j, padded_ldc)])<<" ";
            }
            cout<<"\n";
    }

    //  591 606 621 636 651
    // 1491 1531 1571 1611 1651
    // 2391 2456 2521 2586 2651
    // 3291 3381 3471 3561 3651
    // 4191 4306 4421 4536 4651


    xfblasFree(a);
    xfblasFree(b);
    xfblasFree(c);
    xfblasDestroy();

    return EXIT_SUCCESS;

}
