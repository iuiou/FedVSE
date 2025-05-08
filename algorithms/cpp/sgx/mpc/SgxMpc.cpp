#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <vector>

# include <unistd.h>
# include <pwd.h>
# define MAX_PATH FILENAME_MAX

#define FAIL_AES	0x2

#include <sgx_urts.h>
#include "SgxMpc.h"
#include "Enclave_u.h"
#include "Enclave_u.c"

/* Global EID shared by multiple threads */
sgx_enclave_id_t global_eid = 0;

typedef struct _sgx_errlist_t {
    sgx_status_t err;
    const char *msg;
    const char *sug; /* Suggestion */
} sgx_errlist_t;

/* Error code returned by sgx_create_enclave */
static sgx_errlist_t sgx_errlist[] = {
    {
        SGX_ERROR_UNEXPECTED,
        "Unexpected error occurred.",
        NULL
    },
    {
        SGX_ERROR_INVALID_PARAMETER,
        "Invalid parameter.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_MEMORY,
        "Out of memory.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_LOST,
        "Power transition occurred.",
        "Please refer to the sample \"PowerTransition\" for details."
    },
    {
        SGX_ERROR_INVALID_ENCLAVE,
        "Invalid enclave image.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ENCLAVE_ID,
        "Invalid enclave identification.",
        NULL
    },
    {
        SGX_ERROR_INVALID_SIGNATURE,
        "Invalid enclave signature.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_EPC,
        "Out of EPC memory.",
        NULL
    },
    {
        SGX_ERROR_NO_DEVICE,
        "Invalid SGX device.",
        "Please make sure SGX module is enabled in the BIOS, and install SGX driver afterwards."
    },
    {
        SGX_ERROR_MEMORY_MAP_CONFLICT,
        "Memory map conflicted.",
        NULL
    },
    {
        SGX_ERROR_INVALID_METADATA,
        "Invalid enclave metadata.",
        NULL
    },
    {
        SGX_ERROR_DEVICE_BUSY,
        "SGX device was busy.",
        NULL
    },
    {
        SGX_ERROR_INVALID_VERSION,
        "Enclave version was invalid.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ATTRIBUTE,
        "Enclave was not authorized.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_FILE_ACCESS,
        "Can't open enclave file.",
        NULL
    },
    {
        SGX_ERROR_NDEBUG_ENCLAVE,
        "The enclave is signed as product enclave, and can not be created as debuggable enclave.",
        NULL
    },
    {
        SGX_ERROR_MEMORY_MAP_FAILURE,
        "Failed to reserve memory for the enclave.",
        NULL
    },
};

/* Check error conditions for loading enclave */
void print_error_message(sgx_status_t ret)
{
    size_t idx = 0;
    size_t ttl = sizeof sgx_errlist/sizeof sgx_errlist[0];

    for (idx = 0; idx < ttl; idx++) {
        if(ret == sgx_errlist[idx].err) {
            if(NULL != sgx_errlist[idx].sug)
                printf("Info: %s\n", sgx_errlist[idx].sug);
            printf("Error: %s\n", sgx_errlist[idx].msg);
            break;
        }
    }
    
    if (idx == ttl)
        printf("Error: Unexpected error occurred.\n");
}

/* Initialize the enclave:
 *   Call sgx_create_enclave to initialize an enclave instance
 */
int initialize_enclave(void)
{
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    
    /* Call sgx_create_enclave to initialize an enclave instance */
    /* Debug Support: set 2nd parameter to 1 */
    puts(ENCLAVE_FILENAME);
    fflush(stdout);
    ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, NULL, NULL, &global_eid, NULL);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        return -1;
    }

    return 0;
}

/* OCall functions */
void ocall_print_string(const char *str)
{
    /* Proxy/Bridge will check the length and null-terminate 
     * the input string to prevent buffer overflow. 
     */
    printf("%s", str);
    fflush(stdout);
}


/* Application entry */
// std::vector<int>
// SgxPSI(const uint8_t *aes_key, const uint8_t* aes_iv,
//             const unsigned char* a_encrypt_key, size_t a_size, size_t a_num, 
//             const unsigned char* b_encrypt_key, size_t b_size, size_t b_num)
// {
//     int result_status = 0xff;

//     /* Initialize the enclave */
//     if(initialize_enclave() < 0){
//         printf("ERROR: Initialize enclave FAIL\n");
//         exit(-1);
//     }

//     std::vector<int> psi_set(std::min(a_num, b_num), 0);
//     size_t psi_num = 0;
 
//     sgx_status_t status = ecall_PSI(global_eid, &result_status, aes_key, aes_iv,
//                                     a_encrypt_key, a_size, a_num,
//                                     b_encrypt_key, b_size, b_num,
//                                     psi_set.data(), psi_set.size(), &psi_num);
//     if (status != SGX_SUCCESS) {
// 	    printf("ERROR: SgxPSI FAIL\n");
// 	    print_error_message(status);
// 	    exit(-1);
//     }

//     /* Destroy the enclave */
//     sgx_destroy_enclave(global_eid);

//     if (0 == result_status) {
// 	    printf("Info: SgxPSI SUCCESS.\n");
//         printf("psi_num = %zu, psi_set:\n", psi_num);
//         for (size_t i=0; i<psi_num; ++i)
//             printf("\t#(%zu): %d\n", i+1, psi_set[i]);
//     } else {
//         if (result_status & FAIL_AES) 
//             printf("ERROR: SgxPSI FAIL.\n");
//     }

//     assert(psi_num <= psi_set.size());
//     if (psi_num < psi_set.size())
//         psi_set.resize(psi_num);

//     return psi_set;
// }

void SgxInitEnclave() {
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    
    /* Call sgx_create_enclave to initialize an enclave instance */
    /* Debug Support: set 2nd parameter to 1 */
    puts(ENCLAVE_FILENAME);
    fflush(stdout);
    ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, NULL, NULL, &global_eid, NULL);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        printf("ERROR: Initialize enclave FAIL\n");
        exit(-1);
    }
}

void SgxFreeEnclave() {
    sgx_destroy_enclave(global_eid);
}

void SgxOblivCreateQueue(int queue_size) {
    ecall_OblivCreateQueue(global_eid, queue_size);
}

void SgxOblivInitQueue(int queue_size) {
    ecall_OblivInitQueue(global_eid, queue_size);
}

void SgxOblivFreeQueue() {
    ecall_OblivFreeQueue(global_eid);
}

void SgxEnqueue(const int silo_id, const int silo_beg_idx, const int num,
                const uint8_t *aes_key, const uint8_t* aes_iv,
                const unsigned char* encrypt_data, size_t data_size) {
    
    int result_status = 0xff;

    sgx_status_t status = ecall_Enqueue(global_eid, &result_status, silo_id, silo_beg_idx, num, aes_key, aes_iv, encrypt_data, data_size);

    if (status != SGX_SUCCESS) {
	    printf("ERROR: SGX ENQUEUE FAIL\n");
	    print_error_message(status);
	    exit(-1);
    }
}

int SgxGetSiloIndex(int silo_id) {
    int ret = -100;
    ecall_GetSiloIndex(global_eid, &ret, silo_id);
    return ret;
}

int SgxGetDataIndex(int data_id) {
    int ret = -100;
    ecall_GetDataIndex(global_eid, &ret, data_id);
    return ret;
}

void SgxOblivQueueHead(int* idx, int* silo_id) {
    ecall_OblivQueueHead(global_eid, idx, silo_id);
}

void SgxOblivDequeue(void) {
    ecall_OblivDequeue(global_eid);
}

void SgxOblivDequeue(int num) {
    ecall_OblivDequeueMore(global_eid, num);
}

void SgxImportInfo(size_t silo_id, int importType, size_t data_size, size_t query_k,
                const uint8_t *aes_key, const uint8_t* aes_iv,
                const unsigned char* encrypt_data) {
    int result_status = 0xff;

    sgx_status_t status = ecall_ImportInformation(global_eid, &result_status, silo_id, importType, data_size, query_k, aes_key, aes_iv, encrypt_data);
    if (status != SGX_SUCCESS) {
	    printf("ERROR: SGX IMPORT DATA FAIL, IMPORTTYPE = %d\n", importType);
	    print_error_message(status);
	    exit(-1);
    }
}

void SgxClearInfo(size_t silo_id) {
    ecall_ClearInformation(global_eid, silo_id);
}

void SgxJointEstimation(size_t silo_num, size_t k) {
   ecall_JointEstimation(global_eid, silo_num, k);
}

std::vector<unsigned char> SgxGetPrunedK(size_t silo_id, size_t max_output_size, 
                   const uint8_t* aes_key, const uint8_t* aes_iv) {
    int result_status = 0xff;
    std::vector<unsigned char> result(max_output_size);

    sgx_status_t status = ecall_GetPrunedK(global_eid, &result_status, silo_id, result.size(), aes_key, aes_iv, result.data());
    
    // for(int i = 0;i < max_output_size;i++) {
    //     printf("%02x", result[i]);
    // }
    // printf("\n");
    if (status != SGX_SUCCESS) {
	    printf("ERROR: SGX GET PRUNED K FROM %d FAIL", (int)silo_id);
	    print_error_message(status);
	    exit(-1);
    }
    return result;
}

void SgxCandRefinement(size_t silo_num, size_t k) {
    ecall_CandRefinement(global_eid, silo_num, k);
}

void SgxCandRefinementBase(size_t silo_num, size_t k) {
    ecall_CandRefinementBase(global_eid, silo_num, k);
}

std::vector<unsigned char> SgxGetThres(size_t silo_id, size_t max_output_size,
                    const uint8_t* aes_key, const uint8_t* aes_iv) {
    int result_status = 0xff;
    std::vector<unsigned char> result(max_output_size);

    sgx_status_t status = ecall_GetThres(global_eid, &result_status, silo_id, result.size(), aes_key, aes_iv, result.data());
    if (status != SGX_SUCCESS) {
	    printf("ERROR: SGX GET THRESHOLD FROM %d FAIL", (int)silo_id);
	    print_error_message(status);
	    exit(-1);
    }
    return result;
}

void SgxTopkSelection(size_t silo_num, size_t k) {
    ecall_TopkSelection(global_eid, silo_num, k);
}

int SgxGetK(size_t silo_id) {
    int ret = -100;
    ecall_GetK(global_eid, &ret, silo_id);
    return ret;
}