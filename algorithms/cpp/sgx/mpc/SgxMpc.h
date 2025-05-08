#ifndef _SGX_MPC_H_
#define _SGX_MPC_H_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <vector>

#include "sgx_error.h"       /* sgx_status_t */
#include "sgx_eid.h"     /* sgx_enclave_id_t */

#ifndef TRUE
# define TRUE 1
#endif

#ifndef FALSE
# define FALSE 0
#endif

#if   defined(__GNUC__)
# define ENCLAVE_FILENAME "enclave.signed.so"
#endif

extern sgx_enclave_id_t global_eid;    /* global enclave id */

//sgx_status_t SGX_CDECL ocall_print_string(const char* str);

void SgxInitEnclave();

void SgxFreeEnclave();

void SgxOblivCreateQueue(int queue_size);

void SgxOblivFreeQueue(void);

void SgxOblivInitQueue(int queue_size);

void SgxEnqueue(const int silo_id, const int silo_beg_idx, const int num,
                const uint8_t *aes_key, const uint8_t* aes_iv,
                const unsigned char* encrypt_data, size_t data_size);

int SgxGetSiloIndex(int silo_id);

int SgxGetDataIndex(int data_id);

void SgxOblivQueueHead(int* idx, int* silo_id);

void SgxOblivDequeue(void);

void SgxOblivDequeue(int num);

void SgxImportInfo(size_t silo_id, int importType, size_t data_size, size_t query_k,
                const uint8_t *aes_key, const uint8_t* aes_iv,
                const unsigned char* encrypt_data);

void SgxClearInfo(size_t silo_id);

void SgxJointEstimation(size_t silo_num, size_t k);

std::vector<unsigned char> SgxGetPrunedK(size_t silo_id, size_t max_output_size, 
                   const uint8_t* aes_key, const uint8_t* aes_iv);

void SgxCandRefinement(size_t silo_num, size_t k);

void SgxCandRefinementBase(size_t silo_num, size_t k);

std::vector<unsigned char> SgxGetThres(size_t silo_id, size_t max_output_size,
                    const uint8_t* aes_key, const uint8_t* aes_iv);

void SgxTopkSelection(size_t silo_num, size_t k);

int SgxGetK(size_t silo_id);
             
#endif // _SGX_MPC_H_
