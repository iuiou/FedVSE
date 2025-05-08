/*
 * Copyright (C) 2011-2021 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <stdarg.h>
#include <stdio.h>      /* vsnprintf */
#include <string.h>
#include <math.h>
#include <stdint.h>

#include "Enclave.h"
#include "Enclave_t.h"
#include "Enclave_t.c"
#include "mbedtls/aes.h"
#include "mbedtls/cipher.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#define FAIL_AES	0x2

// #define LOCAL_SGX_DEBUG

extern sgx_status_t SGX_CDECL ocall_printf(const char* msg);

/* 
 * local_printf: 
 *   Invokes OCALL to display the enclave buffer to the terminal.
 *   'local_printf' function is required for sgx protobuf logging module.
 */
 
static int local_printf(const char *fmt, ...)
{
    char buf[BUFSIZ] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print_string(buf);
    return 0;
}

static float UnsignedVectorToFloat(const unsigned char* arr, size_t offset) {
    float result;
    memcpy(&result, arr+offset, sizeof(float));
    return result;
}

static int32_t UnsignedVectorToInt32(const unsigned char* arr, size_t offset) {
    int32_t result;
    memcpy(&result, arr+offset, sizeof(int32_t));
    return result;
}

static unsigned char* Int32ToUnsignedChar(int ori) {
    unsigned char* result = (unsigned char*) malloc(16 * sizeof(unsigned char));
    memcpy(result, &ori, sizeof(ori));
    // local_printf("%02x\n", result[0]);
    for(int j = 4; j < 16; j++) {
        result[j] = '\0';
    }
    return result;
}

static unsigned char* FloatToUnsignedChar(float ori) {
    unsigned char* result = (unsigned char*) malloc(16 * sizeof(unsigned char));
    memcpy(result, &ori, sizeof(ori));
    for(int j = 4;j < 16; j++) {
        result[j] = '\0';
    }
    return result;
} 

static int mbedtls_decrypt_data(const uint8_t *aes_key, const uint8_t* aes_iv,
                                const unsigned char* encrypt_key, size_t encrypt_key_size, 
                                unsigned char*& decrypt_key) {
    const int key_size = 16;
    uint8_t key[key_size], iv[key_size];
    int ret_status = 0;

    memcpy(key, aes_key, sizeof(uint8_t)*key_size);
    memcpy(iv, aes_iv, sizeof(uint8_t)*key_size);

    #ifdef LOCAL_SGX_DEBUG
    local_printf("[FINISH] memcpy key and iv\n");
    local_printf("Key: ");
    for (int i = 0; i < key_size; ++i) {
        local_printf("%02x", key[i]);
    }
    local_printf("\n");
    local_printf(" Iv: ");
    for (int i = 0; i < key_size; ++i) {
        local_printf("%02x", iv[i]);
    }
    local_printf("\n");
    #endif

    mbedtls_aes_context aes_context;
    mbedtls_aes_init(&aes_context);

    #ifdef LOCAL_SGX_DEBUG
    local_printf("[FINISH] mbedtls_aes_init\n");
    #endif

    ret_status = mbedtls_aes_setkey_dec(&aes_context, key, 128);
    if(ret_status != 0){
        #ifdef LOCAL_SGX_DEBUG
        local_printf("failed to set decryption key\n");
        #endif
        mbedtls_aes_free(&aes_context);
        return -1;
    }

    decrypt_key = (unsigned char *)calloc(encrypt_key_size, sizeof(unsigned char));
    if (decrypt_key == NULL) {
        #ifdef LOCAL_SGX_DEBUG
        local_printf("failed to allocate memory for decrypt_key\n");
        #endif
        mbedtls_aes_free(&aes_context);
        return -1;
    }

    ret_status = mbedtls_aes_crypt_cbc(&aes_context, MBEDTLS_AES_DECRYPT, encrypt_key_size, iv, encrypt_key, decrypt_key);
    if (ret_status != 0){
        #ifdef LOCAL_SGX_DEBUG
        local_printf("failed to decrypt data\n");
        #endif
        free(decrypt_key);
        decrypt_key = NULL;
        mbedtls_aes_free(&aes_context);
        return -1;
    }

    mbedtls_aes_free(&aes_context);
    return 0;
}

static int mbedtls_encrypt_data(const uint8_t *aes_key, const uint8_t* aes_iv,
                                const unsigned char* plain_key, size_t plain_key_size, 
                                unsigned char*& encrypt_key) {
    const int key_size = 16;
    uint8_t key[key_size], iv[key_size];
    int ret_status = 0;

    // 复制密钥和IV到本地缓冲区
    memcpy(key, aes_key, key_size);
    memcpy(iv, aes_iv, key_size);

    #ifdef LOCAL_SGX_DEBUG
    local_printf("[FINISH] memcpy key and iv\n");
    local_printf("Key: ");
    for (int i = 0; i < key_size; ++i) {
        local_printf("%02x", key[i]);
    }
    local_printf("\nIv:  ");
    for (int i = 0; i < key_size; ++i) {
        local_printf("%02x", iv[i]);
    }
    local_printf("\n");
    #endif

    mbedtls_aes_context aes_context;
    mbedtls_aes_init(&aes_context);

    #ifdef LOCAL_SGX_DEBUG
    local_printf("[FINISH] mbedtls_aes_init\n");
    #endif

    // 设置加密密钥（注意此处改为setkey_enc）
    ret_status = mbedtls_aes_setkey_enc(&aes_context, key, 128);
    if(ret_status != 0) {
        #ifdef LOCAL_SGX_DEBUG
        local_printf("failed to set encryption key\n");
        #endif
        mbedtls_aes_free(&aes_context);
        return -1;
    }

    #ifdef LOCAL_SGX_DEBUG
    local_printf("[FINISH] set encryption key\n");
    #endif

    // 分配加密数据缓冲区
    encrypt_key = (unsigned char *)calloc(plain_key_size, sizeof(unsigned char));
    if (encrypt_key == NULL) {
        #ifdef LOCAL_SGX_DEBUG
        local_printf("failed to allocate memory for encrypt_key\n");
        #endif
        mbedtls_aes_free(&aes_context);
        return -1;
    }

    #ifdef LOCAL_SGX_DEBUG
    local_printf("[FINISH] allocate memory for encrypt_key\n");
    #endif

    // 执行CBC模式加密（注意改为ENCRYPT模式）
    ret_status = mbedtls_aes_crypt_cbc(&aes_context, MBEDTLS_AES_ENCRYPT,
                                      plain_key_size, iv, plain_key, encrypt_key);
    if (ret_status != 0) {
        #ifdef LOCAL_SGX_DEBUG
        local_printf("failed to encrypt data\n");
        #endif
        free(encrypt_key);
        encrypt_key = NULL;
        mbedtls_aes_free(&aes_context);
        return -1;
    }

    #ifdef LOCAL_SGX_DEBUG
    local_printf("[FINISH] encrypt data\n");
    #endif

    mbedtls_aes_free(&aes_context);
    return 0;
}

static int mbedtls_decode(const unsigned char* decrypt_key) {
    int value = 0;  
    for (size_t i = 0; i < sizeof(int); ++i) {  
        int tmp = decrypt_key[i];
        value |= (tmp << (i * 8));
    }  
    return value;
}

#define INF_PAIR_FIRST       1e20

struct QueueItem {
	float dist;
	int vid;
    int silo_id;
};

struct OblivQueue {
    int N;
    int ptr;
    struct QueueItem* a;
    struct QueueItem* b;
};

struct Array {
    int size;
    int ptr;
    int silo_id;
    struct QueueItem* a;
};

struct Interval {
    float l, r;
    size_t numL, numR;
    int op;
};

struct IntervalArray {
    int size;
    int ptr;
    int silo_id;
    struct Interval* a;
};

static struct OblivQueue* obliv_queue = NULL;

static struct Array* array[10] = {0};

static struct IntervalArray* intarray[10] = {0};

static void compare_and_swap(struct QueueItem* a, struct QueueItem* b) {
	float dista = a->dist;
	float distb = b->dist;

	if(dista < distb) {
	    int vida = a->vid, vidb = b->vid;
        int siloa = a->silo_id, silob = b->silo_id;

        a->dist = distb; a->vid = vidb; a->silo_id = silob;
        b->dist = dista; b->vid = vida; b->silo_id = siloa;
	}
}

static void obliv_bitonic(struct QueueItem *mem, int N) {
	int K = log2(N);
	int d = 1 << K;
	for (int n = 0; n < (d >> 1); n++) {
		compare_and_swap(&mem[n], &mem[d - n - 1]);
	}
	K--;
	if (K <= 0) {
		return;
	}
	for (int k = K; k > 0; k--) {
		d = 1 << k;
		for (int m = 0; m < N; m += d) {
			for (int n = 0; n < (d >> 1); n++) {
				compare_and_swap(&mem[m + n], &mem[m + (d >> 1) + n]);
			}
		}
	}
}

static void obliv_bitonic_sort(struct QueueItem *mem, int N) {
	struct QueueItem* map;
	map = (struct QueueItem *)malloc(N * sizeof(struct QueueItem));
	for (int i = 0; i < N; i++) {
		map[i].dist = mem[i].dist;
        map[i].vid = mem[i].vid;
        map[i].silo_id = mem[i].silo_id;
	}
	int K = log2(N);
	for (int k = 1; k <= K; k++) {
		int d = 1 << k;
		for (int n = 0; n < N; n += d) {
			struct QueueItem* map_ptr = &map[n];
			obliv_bitonic(map_ptr, d);
		}
	}
	for (int n = 0; n < N; n++) {
		mem[n].dist = map[n].dist;
        mem[n].vid = map[n].vid;
        mem[n].silo_id = map[n].silo_id;
	}
	free(map);
}

static void obliv_resort(struct OblivQueue *q) {
    obliv_bitonic_sort(q->a, q->N);
}

static void obliv_append(struct OblivQueue *q, float dist, int vid, int silo_id) {
    int pos = 0;
    for (int i=0; i<q->ptr; ++i) {
        if (q->a[i].dist > dist) {
            pos = i + 1;
        }
    }
    for (int i=0; i<q->ptr+1; ++i) {
        if (i < pos) {
            q->b[i].dist = q->a[i].dist;
            q->b[i].vid = q->a[i].vid;
            q->b[i].silo_id = q->a[i].silo_id;
        } else if (i == pos) {
            q->b[i].dist = dist;
            q->b[i].vid = vid;
            q->b[i].silo_id = silo_id;
        } else {
            q->b[i].dist = q->a[i-1].dist;
            q->b[i].vid = q->a[i-1].vid;
            q->b[i].silo_id = q->a[i-1].silo_id;
        }
    }
    q->ptr += 1;
    for (int i=0; i<q->ptr; ++i) {
        q->a[i].dist = q->b[i].dist;
        q->a[i].vid = q->b[i].vid;
        q->a[i].silo_id = q->b[i].silo_id;
    }
}

static void obliv_init_queue(struct OblivQueue* q, int queue_size) {
    if (q->N != queue_size) {
        if (q->a != NULL) free(q->a);
        q->a = (struct QueueItem*) malloc(queue_size * sizeof(struct QueueItem));
        if (q->b != NULL) free(q->b);
        q->b = (struct QueueItem*) malloc(queue_size * sizeof(struct QueueItem));      
    }
    q->N = queue_size;
    q->ptr = 0;
    for(int i=0; i<queue_size; i++) {
        q->a[i].dist = INF_PAIR_FIRST;
        q->a[i].vid = -1;
        q->a[i].silo_id = -1;
    }
}

static void OblivDequeue(struct OblivQueue *q) {
    q->ptr -= 1;
    q->a[q->ptr].dist = INF_PAIR_FIRST;
    q->a[q->ptr].vid = -1;
    q->a[q->ptr].silo_id = -1;
    for (int i=0; i<q->ptr; ++i) {
        q->a[i].dist = q->a[i].dist;
        q->a[i].vid = q->a[i].vid;
        q->a[i].silo_id = q->a[i].silo_id;
    }
}

static void OblivDequeue(struct OblivQueue *q, int num) {
    for (int i=0; i<num; ++i) {
        q->ptr -= 1;
        q->a[q->ptr].dist = INF_PAIR_FIRST;
        q->a[q->ptr].vid = -1;
        q->a[q->ptr].silo_id = -1;
    }
    for (int i=0; i<q->ptr; ++i) {
        q->a[i].dist = q->a[i].dist;
        q->a[i].vid = q->a[i].vid;
        q->a[i].silo_id = q->a[i].silo_id;
    }
}

static void OblivEnqueue(struct OblivQueue *q, float dist, int id, int silo_id) {
    if(q->ptr >= q->N) {
        if (dist >= q->a[q->ptr-1].dist) {
            return;
        } else {
            OblivDequeue(q);
        }
    }
    obliv_append(q, dist, id, silo_id);
    // q->a[q->ptr].dist = dist;
    // q->a[q->ptr].vid = id;
    // q->a[q->ptr].silo_id = silo_id;
    // q->ptr += 1;
    // obliv_resort(q);
}

static int GetDataIndex(struct OblivQueue *q, int id) {
    int ans = -1;
    for(int i=0; i<q->ptr; i++) {
        if (q->a[i].vid == id) {
            ans = i;
        }
    }
    return ans;
}

static int GetSiloIndex(struct OblivQueue *q, int silo_id) {
    int ans = -1;
    for(int i=0; i<q->ptr; i++) {
        if (q->a[i].silo_id == silo_id) {
            ans = i;
        }
    }
    return ans;
}

static void OblivFreeQueue(struct OblivQueue*& q) {
    if (q!=NULL && q->a!=NULL)
        free(q->a);
    q->a = NULL;
    if (q!=NULL && q->b!=NULL)
        free(q->b);
    q->b = NULL;
    if (q != NULL)
        free(q);
    q = NULL;
}

static void ClearArray(struct Array*& arr) {
    if(arr!=NULL && arr->a!=NULL) {
        free(arr->a);
    }
    arr->a = NULL;
    if(arr != NULL) free(arr);
    arr = NULL;
}

static void ClearIntervalArray(struct IntervalArray*& arr) {
    if(arr!=NULL && arr->a!=NULL) {
        free(arr->a);
    }
    arr->a = NULL;
    if(arr != NULL) free(arr);
    arr = NULL;
}

static void OblivCreateQueue(struct OblivQueue*& q, int queue_size) {
    q = (struct OblivQueue*) malloc (sizeof(struct OblivQueue));
    q->a = (struct QueueItem*) malloc(queue_size * sizeof(struct QueueItem));
    q->b = (struct QueueItem*) malloc(queue_size * sizeof(struct QueueItem));
    q->N = queue_size;
    obliv_init_queue(q, queue_size);
}

static void OblivInitQueue(struct OblivQueue* q, int queue_size) {
    obliv_init_queue(q, queue_size);
}

static void m_InitArray(struct Array* set, int array_size, int silo_id) {
    set->a = (struct QueueItem*) malloc(array_size * sizeof(struct QueueItem));
    set->ptr = 0;
    set->silo_id = silo_id;
    set->size = array_size;
}

static void m_InitIntervalArray(struct IntervalArray* set, int array_size, int silo_id) {
    set->a = (struct Interval*) malloc(array_size * sizeof(struct Interval));
    set->ptr = 0;
    set->silo_id = silo_id;
    set->size = array_size;
}

static void OblivQueueHead(struct OblivQueue *q, float* dist, int* vid, int* silo_id) {
    *dist = -1.0;
    *vid = -1;
    *silo_id = -1;
    for(int i=0; i<q->ptr; ++i) {
        *dist = q->a[i].dist;
        *vid = q->a[i].vid;
        *silo_id = q->a[i].silo_id;
    }
}

void ecall_OblivDequeue(void) {
    OblivDequeue(obliv_queue);
}

void ecall_OblivDequeueMore(int num) {
    OblivDequeue(obliv_queue, num);
}

void ecall_OblivCreateQueue(int queue_size) {
    OblivCreateQueue(obliv_queue, queue_size);
}

void ecall_OblivInitQueue(int queue_size) {
    OblivInitQueue(obliv_queue, queue_size);
}

int ecall_GetSiloIndex(int silo_id) {
    return GetSiloIndex(obliv_queue, silo_id);
}

int ecall_GetDataIndex(int data_id) {
    return GetDataIndex(obliv_queue, data_id);
}

void ecall_OblivFreeQueue(void) {
    OblivFreeQueue(obliv_queue);
    obliv_queue = NULL;
}

void ecall_OblivQueueHead(int* idx, int* silo_id) {
    float dist;
    OblivQueueHead(obliv_queue, &dist, idx, silo_id);
}

int ecall_Enqueue(int silo_id, int silo_beg_idx, int num,
                    const uint8_t *aes_key, const uint8_t* aes_iv,
                    const unsigned char* encrypt_data, size_t data_size) {

    int ret_status = 0;

    #ifdef LOCAL_SGX_DEBUG
    local_printf("[START] ecall_enqueue\n");
    local_printf("silo_id = %d, silo_beg_idx = %d, num = %d\n", silo_id, silo_beg_idx, num);
    #endif

    unsigned char* plain_data = NULL;
    ret_status = mbedtls_decrypt_data(aes_key, aes_iv, encrypt_data, data_size, plain_data);
    if (0 != ret_status) return ret_status;

    #ifdef LOCAL_SGX_DEBUG
    local_printf("[FINISH] AES decryption\n");
    {
        int batch_size = sizeof(int32_t) + sizeof(float);
        for (size_t j=0, offset=0; j<num; ++j, offset+=batch_size) {
            int vid = UnsignedVectorToInt32(plain_data, offset);
            float dis = UnsignedVectorToFloat(plain_data, offset+sizeof(int32_t));
            int idx = silo_beg_idx + j;
            local_printf("Decrypt data %d: silo_id = %d, dis = %.6f, vid = %d, idx = %d\n", j, silo_id, dis, vid, idx);
        }
    }
    #endif
    
    int batch_size = sizeof(int32_t) + sizeof(float);
    for (size_t j=0, offset=0; j<num; ++j, offset+=batch_size) {
        int vid = UnsignedVectorToInt32(plain_data, offset);
        float dis = UnsignedVectorToFloat(plain_data, offset+sizeof(int32_t));
        int idx = silo_beg_idx + j;
        OblivEnqueue(obliv_queue, dis, idx, silo_id);
    }

    #ifdef LOCAL_SGX_DEBUG
    local_printf("[FINISH] ecall_enqueue\n");
    #endif

    free(plain_data);

    return ret_status;
}

static void InitArray(int silo_id, int size) {
    array[silo_id] = (struct Array*) malloc (sizeof(struct Array));
    m_InitArray(array[silo_id], size, silo_id);    
}

static void InitIntervalArray(int silo_id, int size) {
    intarray[silo_id] = (struct IntervalArray*) malloc (sizeof(struct IntervalArray));
    m_InitIntervalArray(intarray[silo_id], size, silo_id);
}

void ecall_ClearInformation(size_t silo_id) {
    ClearArray(array[silo_id]);
}

static int ceilSqrtK(int k) {
    size_t i = 0;
    for(i = 0; i * i < k; i++) {
    }
    return i;
}

int ecall_ImportInformation(size_t silo_id, int importType, size_t data_size, size_t query_k,
                    const uint8_t *aes_key, const uint8_t* aes_iv,
                    const unsigned char* encrypt_data) {
    int ret_status = 0;
    
    unsigned char* plain_data = NULL; // decrypt submitted info
    ret_status = mbedtls_decrypt_data(aes_key, aes_iv, encrypt_data, data_size, plain_data);
    if (0 != ret_status) return ret_status;
    

    if(importType == 1) { // contribution pre-estimation
        InitArray(silo_id, 1);
        int batchSize = sizeof(float), offset = 0;
        array[silo_id]->silo_id = silo_id;
        array[silo_id]->size = 1;
        for(int i = 0;i < 1;i++) {
            float dis = UnsignedVectorToFloat(plain_data, offset + batchSize * i);
            array[silo_id]->a[i].dist = dis;
            array[silo_id]->a[i].vid = 0;
            array[silo_id]->a[i].silo_id = silo_id;
        }
    } else if(importType == 2) { // phase I: candidates refinement
        int batchSize = sizeof(float), offset = 0;
        int k = UnsignedVectorToInt32(plain_data, offset);
        int bucketSize = ceilSqrtK(query_k);
        int num = (k + bucketSize - 1) / bucketSize;

        #ifdef LOCAL_SGX_DEBUG
        local_printf("[IN SGX] silo_id : %d, bucketSize : %d, num : %d\n", silo_id, bucketSize, num);
        #endif

        InitArray(silo_id, num);
        offset += sizeof(int);
        array[silo_id]->silo_id = silo_id;
        array[silo_id]->size = num;
        array[silo_id]->ptr = 0;
        for(int i = 0;i < num; i++, offset += batchSize) {
            float dis = UnsignedVectorToFloat(plain_data, offset);
            array[silo_id]->a[i].dist = dis;
            array[silo_id]->a[i].vid = (i == num - 1 ? k - bucketSize * i : bucketSize);
            array[silo_id]->a[i].silo_id = silo_id;
        }
    } else if(importType == 3) { // phase II: secure top-k selection
        int batchSize = sizeof(float), offset = 0;
        int cnt = UnsignedVectorToInt32(plain_data, offset);
        InitArray(silo_id, cnt);
        offset += sizeof(int);
        array[silo_id]->silo_id = silo_id;
        array[silo_id]->size = cnt;
        array[silo_id]->ptr = 0;
        for(int i = 0;i < cnt; i++, offset += batchSize) {
            float dis = UnsignedVectorToFloat(plain_data, offset);
            array[silo_id]->a[i].dist = dis;
            array[silo_id]->a[i].vid = 0;
            array[silo_id]->a[i].silo_id = silo_id;
        }
    } else if(importType == 4) {
        ClearIntervalArray(intarray[silo_id]);
        int batchSize = sizeof(float) * 2, offset = 0;
        int k = UnsignedVectorToInt32(plain_data, offset);
        int bucketSize = ceilSqrtK(query_k);
        int num = (k + bucketSize - 1) / bucketSize;
        if(num == 0) {
            InitIntervalArray(silo_id, 0);
        } else {
            InitIntervalArray(silo_id, 2 * num + 1);
        }
        offset += 4;
        int ptr = 0;
        float inf = 1e12;
        for(int i = 0; i < num; i++, offset += batchSize) {
            float l = UnsignedVectorToFloat(plain_data, offset);
            float r = UnsignedVectorToFloat(plain_data, offset + 4);
            if(i == 0) {
                intarray[silo_id]->a[ptr].l = -inf;
                intarray[silo_id]->a[ptr].r = l;
                intarray[silo_id]->a[ptr].numL = 0;
                intarray[silo_id]->a[ptr].numR = 0;
                intarray[silo_id]->a[ptr].op = 2;
                ptr++;
            } 
            if (i == num - 1) {
                intarray[silo_id]->a[ptr].l = l;
                intarray[silo_id]->a[ptr].r = r;
                intarray[silo_id]->a[ptr].numL = i * bucketSize + 1;
                intarray[silo_id]->a[ptr].numR = k;
                intarray[silo_id]->a[ptr].op = 1;
                ptr++;

                intarray[silo_id]->a[ptr].l = r;
                intarray[silo_id]->a[ptr].r = inf;
                intarray[silo_id]->a[ptr].numL = k;
                intarray[silo_id]->a[ptr].numR = k;
                intarray[silo_id]->a[ptr].op = 2;
                ptr++;
            } else {
                intarray[silo_id]->a[ptr].l = l;
                intarray[silo_id]->a[ptr].r = r;
                intarray[silo_id]->a[ptr].numL = i * bucketSize + 1;
                intarray[silo_id]->a[ptr].numR = (i + 1) * bucketSize;
                intarray[silo_id]->a[ptr].op = 1;
                ptr++;

                float nextl = UnsignedVectorToFloat(plain_data, offset + 8);
                intarray[silo_id]->a[ptr].l = r;
                intarray[silo_id]->a[ptr].r = nextl;
                intarray[silo_id]->a[ptr].numL = (i + 1) * bucketSize;
                intarray[silo_id]->a[ptr].numR = (i + 1) * bucketSize;
                intarray[silo_id]->a[ptr].op = 2;
                ptr++;
            }
        }
    }
    free(plain_data);
    return ret_status;
}

static int pruned_k[10] = {0};

void ecall_JointEstimation(size_t silo_num, size_t k) {
    int ret_status = 0;
    float MinRadius = 1e12;
    for(int i = 0;i < silo_num; i++) {
        MinRadius = (MinRadius > array[i]->a[0].dist) ? array[i]->a[0].dist : MinRadius;
    }
    for(int i = 0;i < silo_num; i++) {
        pruned_k[i] = ceil(1.0 * k * (MinRadius / array[i]->a[0].dist));
        pruned_k[i] = (pruned_k[i] >= 1 ? pruned_k[i] : 1);

        #ifdef LOCAL_SGX_DEBUG
        local_printf("[Joint Estimation Results] siloid: %d, k: %d\n", i, pruned_k[i]);
        #endif
    }
}

int ecall_GetPrunedK(size_t silo_id, size_t max_output_size, 
                    const uint8_t* aes_key, const uint8_t* aes_iv,
                    unsigned char* encrypt_k) {
    int ret_status = 0;
    unsigned char* encrypt_key = NULL;
    unsigned char* plain_data = Int32ToUnsignedChar(pruned_k[silo_id]);
    ret_status = mbedtls_encrypt_data(aes_key, aes_iv, plain_data, max_output_size, encrypt_key);
    for(int i = 0;i < max_output_size;i++) {
        encrypt_k[i] = encrypt_key[i];
    }
    free(plain_data);
    free(encrypt_key);
    return ret_status;
}

static float thres[10] = {0.0};

void ecall_CandRefinement(size_t silo_num, size_t k) {
    OblivCreateQueue(obliv_queue, silo_num);
    for(int i = 0;i < silo_num;i++) {
        array[i]->ptr = 0;
        if (array[i]->size > 0) { 
            OblivEnqueue(obliv_queue, array[i]->a[0].dist, array[i]->a[0].vid, array[i]->a[0].silo_id);
            array[i]->ptr++;
        }
    }
    int cnt = 0;
    float radius = 0;
    while(cnt < k && obliv_queue->ptr > 0) {
        float dist;
        int count, silo_id;
        OblivQueueHead(obliv_queue, &dist, &count, &silo_id);
        OblivDequeue(obliv_queue);
        cnt += count;
        radius = (radius < dist) ? dist : radius;
        if(cnt >= k) break;
        if(array[silo_id]->ptr < array[silo_id]->size) {
            int pt = array[silo_id]->ptr;
            OblivEnqueue(obliv_queue, array[silo_id]->a[pt].dist, array[silo_id]->a[pt].vid, array[silo_id]->a[pt].silo_id);
            array[silo_id]->ptr++;
        }
    }
    for(int i = 0;i < silo_num; i++) {
        int pt = array[i]->ptr;
        thres[i] = array[i]->a[pt - 1].dist;
    }

    #ifdef LOCAL_SGX_DEBUG
    local_printf("[Candidate Refinement Results]: ");
    for(int i = 0;i < silo_num; i++) {
        local_printf("%f ", thres[i]);
    }
    local_printf("\n");
    #endif

    OblivFreeQueue(obliv_queue);
}

void ecall_CandRefinementBase(size_t silo_num, size_t k) {
    float l = 0, r = 0;
    for(size_t i = 0;i < silo_num;i++) {
        for(size_t j = 0;j < intarray[i]->size; j++) {
            r = intarray[i]->a[j].l > r ? intarray[i]->a[j].l : r;
        }
    }
    r += 1;
    const float eps = 1e-3;
    float radius = 0;
    local_printf("[Before Binary Search]\n");
    while(r - l > eps) {
        float mid = (r + l) / 2;
        size_t Rnum = 0;
        radius = 0;
        for(size_t i = 0;i < silo_num;i++) {
            if(intarray[i]->size == 0) continue;
            size_t L = 0, R = (size_t)intarray[i]->size - 1;
            int ansPos = -1;
            while(L <= R) {
                size_t Mid = (L + R) / 2;
                if((intarray[i]->a[Mid].op == 1 && (intarray[i]->a[Mid].l <= mid && mid <= intarray[i]->a[Mid].r)) ||
                (intarray[i]->a[Mid].op == 2 && (intarray[i]->a[Mid].l < mid && mid < intarray[i]->a[Mid].r))) {
                    ansPos = Mid;
                    break;
                } else if(intarray[i]->a[Mid].l > mid) {
                    R = Mid - 1;
                } else {
                    L = Mid + 1;
                }
            }
            if(ansPos != -1) {
                Rnum += intarray[i]->a[ansPos].numR;
                radius = radius < intarray[i]->a[ansPos].r ? intarray[i]->a[ansPos].r : radius;
            }
        }
        if(Rnum >= k) {
            r = mid;
        } else {
            l = mid;
        }
    }
    local_printf("[After Binary Search]\n");
    float inf = 1e12;
    if(radius == inf) {
        for(size_t i = 0;i < silo_num; i++) {
            thres[i] = radius;
        } 
    } else {
        for(size_t i = 0;i < silo_num;i++) {
            if(intarray[i]->size == 0) {
                thres[i] = 0;
                continue;
            }
            size_t L = 0, R = (size_t)intarray[i]->size - 1;
            int ansPos = -1;
            while(L <= R) {
                size_t Mid = (L + R) / 2;
                if((intarray[i]->a[Mid].op == 1 && (intarray[i]->a[Mid].l <= radius && radius <= intarray[i]->a[Mid].r)) ||
                (intarray[i]->a[Mid].op == 2 && (intarray[i]->a[Mid].l < radius && radius < intarray[i]->a[Mid].r))) {
                    ansPos = Mid;
                    break;
                } else if((intarray[i]->a[Mid].op == 1 && intarray[i]->a[Mid].l > radius) ||
                (intarray[i]->a[Mid].op == 2 && intarray[i]->a[Mid].l >= radius)) {
                    R = Mid - 1;
                } else {
                    L = Mid + 1;
                }
            }
            thres[i] = intarray[i]->a[ansPos].r;
        }
    }
}

int ecall_GetThres(size_t silo_id, size_t max_output_size,
                  const uint8_t* aes_key, const uint8_t* aes_iv,
                  unsigned char* encrypt_thres) {
    int ret_status = 0;
    unsigned char* encrypt_key = NULL;
    unsigned char* plain_data = FloatToUnsignedChar(thres[silo_id]);
    ret_status = mbedtls_encrypt_data(aes_key, aes_iv, plain_data, max_output_size, encrypt_key);   
    for(int i = 0;i < max_output_size;i++) {
        encrypt_thres[i] = encrypt_key[i];
    } 
    free(plain_data);
    free(encrypt_key);
    return ret_status;
}

static int finalK[10] = {0};

void ecall_TopkSelection(size_t silo_num, size_t k) {
    OblivCreateQueue(obliv_queue, silo_num);
    for(int i = 0;i < silo_num; i++) {
        array[i]->ptr = 0;
        if (array[i]->size > 0) { 
            OblivEnqueue(obliv_queue, array[i]->a[0].dist, array[i]->a[0].vid, array[i]->a[0].silo_id);
            array[i]->ptr++;
        }
    }
    int cnt = 0;
    while(cnt < k && obliv_queue->ptr > 0) {
        float dist;
        int count, silo_id;
        OblivQueueHead(obliv_queue, &dist, &count, &silo_id);
        OblivDequeue(obliv_queue);
        cnt += 1;
        finalK[silo_id]++;
        if(array[silo_id]->ptr < array[silo_id]->size) {
            int pt = array[silo_id]->ptr;
            OblivEnqueue(obliv_queue, array[silo_id]->a[pt].dist, array[silo_id]->a[pt].vid, array[silo_id]->a[pt].silo_id);
            array[silo_id]->ptr++;
        }
    }
    OblivFreeQueue(obliv_queue);

    #ifdef LOCAL_SGX_DEBUG 
    local_printf("[Top-k Selection Results]: ");
    for(int i = 0;i < silo_num;i++) {
        local_printf("%d ", finalK[i]);
    }
    local_printf("\n");
    #endif
}

int ecall_GetK(size_t silo_id) {
    int ans = finalK[silo_id];
    finalK[silo_id] = 0;
    return ans;
}