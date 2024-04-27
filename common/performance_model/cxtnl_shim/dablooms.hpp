/* Copyright @2012 by Justin Hines at Bitly under a very liberal license. See LICENSE in the source distribution. */
#pragma once

#include <sys/stat.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

#include "fixed_types.h"
#include "murmur.h"

#define DABLOOMS_VERSION "0.9.1"

namespace CxTnLMemShim {

class BloomFilter {

public:
    typedef struct {
        size_t bytes;
        char  *array;
    } bitmap_t;

    typedef struct {
        uint64_t id;
        uint32_t _pad;
    } counting_bloom_header_t;

    typedef struct {
        counting_bloom_header_t *header;
        unsigned int capacity;
        long offset;
        unsigned int counts_per_func;
        uint32_t *hashes;
        size_t nfuncs;
        size_t size;
        size_t num_bytes;
        double error_rate;
        bitmap_t *bitmap;
    } counting_bloom_t;

private:

    void free_bitmap(bitmap_t *bitmap)
    {
        free(bitmap->array);
        free(bitmap);
    }

    /* Create a new bitmap, not full featured, simple to give
    * us a means of interacting with the 4 bit counters */
    bitmap_t *new_bitmap(size_t bytes)
    {
        bitmap_t *bitmap;
        
        if ((bitmap = (bitmap_t *)malloc(sizeof(bitmap_t))) == NULL) {
            return NULL;
        }
        
        bitmap->bytes = bytes;
        bitmap->array = (char*)malloc(bytes);
        
        return bitmap;
    }

    int bitmap_increment(bitmap_t *bitmap, unsigned int index, long offset)
    {
        long access = index / 2 + offset;
        uint8_t temp;
        uint8_t n = bitmap->array[access];
        if (index % 2 != 0) {
            temp = (n & 0x0f);
            n = (n & 0xf0) + ((n & 0x0f) + 0x01);
        } else {
            temp = (n & 0xf0) >> 4;
            n = (n & 0x0f) + ((n & 0xf0) + 0x10);
        }
        
        if (temp == 0x0f) {
            // fprintf(stderr, "Error, 4 bit int Overflow\n");
            m_cnt_positive_overflow++;
            return -1;
        }
        
        bitmap->array[access] = n;
        return 0;
    }

    /* increments the four bit counter */
    int bitmap_decrement(bitmap_t *bitmap, unsigned int index, long offset)
    {
        long access = index / 2 + offset;
        uint8_t temp;
        uint8_t n = bitmap->array[access];
        
        if (index % 2 != 0) {
            temp = (n & 0x0f);
            n = (n & 0xf0) + ((n & 0x0f) - 0x01);
        } else {
            temp = (n & 0xf0) >> 4;
            n = (n & 0x0f) + ((n & 0xf0) - 0x10);
        }

        if (temp == 0x00) {
            m_cnt_negative_overflow++;
            // fprintf(stderr, "Error, Decrementing zero\n");
            return -1;
        }
        
        bitmap->array[access] = n;
        return 0;
    }

    int bitmap_check(bitmap_t *bitmap, unsigned int index, long offset)
    {
        long access = index / 2 + offset;
        if (index % 2 != 0 ) {
            return bitmap->array[access] & 0x0f;
        } else {
            return bitmap->array[access] & 0xf0;
        }
    }

    /*
    * Perform the actual hashing for `key`
    *
    * Only call the hash once to get a pair of initial values (h1 and
    * h2). Use these values to generate all hashes in a quick loop.
    *
    * See paper by Kirsch, Mitzenmacher [2006]
    * http://www.eecs.harvard.edu/~michaelm/postscripts/rsa2008.pdf
    */
    void hash_func(counting_bloom_t *bloom, const char *key, size_t key_len, uint32_t *hashes)
    {
        size_t i;
        uint32_t checksum[4];
        
        MurmurHash3_x64_128(key, key_len, SALT_CONSTANT, checksum);
        uint32_t h1 = checksum[0];
        uint32_t h2 = checksum[1];
        
        for (i = 0; i < bloom->nfuncs; i++) {
            hashes[i] = (h1 + i * h2) % bloom->counts_per_func;
        }
    }

    counting_bloom_t *counting_bloom_init_nfunc(unsigned int capacity, int nfunc, long offset)
    {
        counting_bloom_t *bloom;
        
        if ((bloom = (counting_bloom_t*) malloc(sizeof(counting_bloom_t))) == NULL) {
            fprintf(stderr, "Error, could not realloc a new bloom filter\n");
            return NULL;
        }
        bloom->bitmap = NULL;
        bloom->capacity = capacity;
        bloom->error_rate = 1 / pow(2, nfunc);
        bloom->offset = offset + sizeof(counting_bloom_header_t);
        bloom->nfuncs = nfunc;
        // bloom->counts_per_func = (int) ceil(capacity * fabs(log(bloom->error_rate)) / (bloom->nfuncs * pow(log(2), 2)));
        bloom->counts_per_func = (int) ceil(capacity / nfunc);
        bloom->size = bloom->nfuncs * bloom->counts_per_func;
        /* rounding-up integer divide by 2 of bloom->size */
        bloom->num_bytes = ((bloom->size + 1) / 2) + sizeof(counting_bloom_header_t);
        bloom->hashes = (uint32_t*) calloc(bloom->nfuncs, sizeof(uint32_t));
        
        return bloom;
    }


private:
    counting_bloom_t *bloom_;
    UInt32 m_cnt_negative_overflow, m_cnt_positive_overflow;

    UInt32 m_cnt_check, m_cnt_add, m_cnt_remove;
    UInt32 m_cnt_check_hit, m_cnt_check_miss;
    UInt32 m_remove_fail;

public:

    BloomFilter(unsigned int capacity, int nfunc):
        bloom_(NULL), m_cnt_negative_overflow(0), m_cnt_positive_overflow(0),
        m_cnt_check(0), m_cnt_add(0), m_cnt_remove(0),
        m_cnt_check_hit(0), m_cnt_check_miss(0), m_remove_fail(0)
    {
        bloom_ = counting_bloom_init_nfunc(capacity, nfunc, 0);
        bloom_->bitmap = new_bitmap(bloom_->num_bytes);
        bloom_->header = (counting_bloom_header_t *)(bloom_->bitmap->array);
    }

    ~BloomFilter() {
        if (bloom_ != NULL) {
            free(bloom_->hashes);
            bloom_->hashes = NULL;
            free(bloom_->bitmap);
            free(bloom_);
        }
    }

    void print_states() {
        counting_bloom_t* bloom = bloom_;
        int total = 0;
        for (size_t i = bloom->offset; i < bloom->bitmap->bytes; i++) {
            // Each slot takes four bits
            char byte = bloom->bitmap->array[i];
            total += ((byte & 0x0f) != 0);
            total += ((byte & 0xf0) != 0);
        }

        printf("Check Hit: %d, Check Miss: %d, Check: %d Add: %d, Remove: %d, Remove Fail: %d\n", m_cnt_check_hit, m_cnt_check_miss, \
            m_cnt_check, m_cnt_add, m_cnt_remove, m_remove_fail);
        printf("Positive Overflow: %d, Negative Overflow: %d\n", m_cnt_positive_overflow, m_cnt_negative_overflow);
        printf("Bloom Fill Rate: %d/%ld\n", total, bloom->size);
        printf("Bloom Logical Error Rate: %f\n", bloom->error_rate);
    }

    int counting_bloom_add(const char *s, size_t len)
    {
        m_cnt_add++;
        counting_bloom_t *bloom = bloom_;
        size_t index, i, offset;
        unsigned int *hashes = bloom->hashes;
        
        hash_func(bloom, s, len, hashes);
        
        for (i = 0; i < bloom->nfuncs; i++) {
            offset = i * bloom->counts_per_func;
            index = hashes[i] + offset;
            bitmap_increment(bloom->bitmap, index, bloom->offset);
        }
        
        return 0;
    }

    int counting_bloom_remove(const char *s, size_t len)
    {
        m_cnt_remove++;
        counting_bloom_t *bloom = bloom_;
        if (counting_bloom_check(s, len) == 0) {
            m_remove_fail++;
            return -1;
        }
        
        size_t index, i, offset;
        unsigned int *hashes = bloom->hashes;
        
        hash_func(bloom, s, len, hashes);
        
        for (i = 0; i < bloom->nfuncs; i++) {
            offset = i * bloom->counts_per_func;
            index = hashes[i] + offset;
            bitmap_decrement(bloom->bitmap, index, bloom->offset);
        }
        
        return 0;
    }

    int counting_bloom_check(const char *s, size_t len)
    {
        m_cnt_check++;
        counting_bloom_t *bloom = bloom_;
        unsigned int index, i, offset;
        unsigned int *hashes = bloom->hashes;
        
        hash_func(bloom, s, len, hashes);
        
        for (i = 0; i < bloom->nfuncs; i++) {
            offset = i * bloom->counts_per_func;
            index = hashes[i] + offset;
            if (!(bitmap_check(bloom->bitmap, index, bloom->offset))) {
                m_cnt_check_miss++;
                return 0;
            }
        }
        m_cnt_check_hit++;
        return 1;
    }

};
}



