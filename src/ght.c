/*
 * ght.c - Generic Hash Table implementation
 * 
 * Copyright (c) 2024 Laurent Mailloux-Bourassa
 * 
 * This file is part of the Generic Hash Table (GHT) library.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include "ght.h"

typedef struct ght_bucket ght_bucket_t;
typedef struct ght_bucket
{
    ght_key_t key;
    ght_hash_t hash;
    ght_data_t data;
    ght_bucket_t* next;
} ght_bucket_t;

typedef struct ght_table
{
    ght_bucket_t** buckets;
    ght_width_t width;
    ght_load_t load;
    ght_load_factor_t auto_resize;
    ght_digestor_t digestor;
} ght_table_t;

static ght_hash_t _ght_digestor_murmur3(ght_key_t key);
static GHT_FORCE_INLINE uint32_t _ght_murmur3_32(ght_key_t key, uint32_t seed);
static GHT_FORCE_INLINE uint64_t _ght_murmur3_64(ght_key_t key, uint64_t seed);
static void _ght_delete_recursive(ght_bucket_t* bucket, ght_load_t* load, ght_deallocator_t deallocator);
static void _ght_move_recursive(ght_bucket_t* bucket, ght_table_t* to_table);

#define GHT_DIGESTOR_MURMUR3(key, seed) _Generic((key), \
        uint64_t: _ght_murmur3_64,                      \
        default: _ght_murmur3_32                        \
        )(key, seed)

ght_table_t* ght_create(ght_width_t width, ght_digestor_t digestor, ght_load_factor_t auto_resize)
{
    if (!width) return NULL;
    
    ght_table_t* table = calloc(1, sizeof(ght_table_t));
    table->buckets = calloc(width, sizeof(ght_bucket_t*));
    table->width = width;
    table->auto_resize = auto_resize > 0.0 ? auto_resize : 0.0;
    
    if (!digestor)
    {
        digestor = _ght_digestor_murmur3;
    }
    
    table->digestor = digestor;
    
    return table;
}

ght_status_t ght_destroy(ght_table_t* table, ght_deallocator_t deallocator)
{
    if (!table) return -1;
    
    for (ght_load_t i = 0; table->load && (i < table->width); i++)
    {
        _ght_delete_recursive(table->buckets[i], &table->load, deallocator);
    }
    
    free(table->buckets);
    free(table);
    
    return 0;
}

ght_status_t ght_insert(ght_table_t* table, ght_key_t key, ght_data_t data)
{
    if (!table) return -1;
    
    if (table->auto_resize > 0.0 && (ght_load_factor_t) (table->load + 1)/(ght_load_factor_t) table->width > table->auto_resize)
    {
        ght_resize(table, table->width * 2);
    }
    
    ght_bucket_t* bucket = calloc(1, sizeof(ght_bucket_t));
    bucket->key = key;
    bucket->data = data;
    
    bucket->hash = table->digestor(key);
    ght_index_t index = bucket->hash % table->width;
    bucket->next = table->buckets[index];
    table->buckets[index] = bucket;
    table->load++; 
    
    return 0;
}

ght_data_t ght_search(ght_table_t* table, ght_key_t key)
{
    if (!table) return 0;
    
    ght_index_t index = table->digestor(key) % table->width;
    ght_bucket_t* bucket = table->buckets[index];
    ght_bucket_t* prev = NULL;
    
    while (bucket && (key != bucket->key))
    {
        prev = bucket;
        bucket = bucket->next;
    }
    
    if (!bucket)
    {
        return 0;
    }

    if (prev)
    {
        prev->next = bucket->next;
    }
    
    if (table->buckets[index] != bucket)
    {
        bucket->next = table->buckets[index];
        table->buckets[index] = bucket;
    }
    
    return bucket->data;
}

ght_status_t ght_delete(ght_table_t* table, ght_key_t key, ght_deallocator_t deallocator)
{
    if (!table) return -1;
    
    ght_index_t index = table->digestor(key) % table->width;
    ght_bucket_t* bucket = table->buckets[index];
    ght_bucket_t* prev = NULL;
    
    while (bucket && (key != bucket->key))
    {
        prev = bucket;
        bucket = bucket->next;
    }
    
    if (!bucket)
    {
        return -1;
    }

    if (prev)
    {
        prev->next = bucket->next;
    }
    
    if (table->buckets[index] == bucket)
    {
        table->buckets[index] = bucket->next;
    }
    
    if (deallocator)
    {
        deallocator(bucket->key, bucket->data);
    }
    
    free(bucket);
    table->load--;
    
    return 0;
}

ght_load_t ght_load(ght_table_t* table)
{
    if (!table) return 0;

    return table->load;
}

ght_width_t ght_width(ght_table_t* table)
{
    if (!table) return 0;

    return table->width;
}

ght_load_factor_t ght_load_factor(ght_table_t* table)
{
     if (!table || table->width == 0) return 0.0;
     
     return (ght_load_factor_t) table->load / (ght_load_factor_t) table->width;
}

ght_status_t ght_resize(ght_table_t* table, ght_width_t width)
{
    if (!table || !width) return -1;
    
    ght_table_t* new = ght_create(width, table->digestor, 0);
    
    for (ght_load_t i = 0; i < table->width; i++)
    {
        _ght_move_recursive(table->buckets[i], new);
    }
    
    free(table->buckets);
    table->buckets = new->buckets;
    table->width = new->width;
    free(new);
    
    return 0;
}

static ght_hash_t _ght_digestor_murmur3(ght_key_t key)
{
    uint32_t seed = 0x9747b28c;
    return GHT_DIGESTOR_MURMUR3(key, seed);
}

static GHT_FORCE_INLINE uint32_t _ght_murmur3_32(ght_key_t key, uint32_t seed)
{
    uint32_t hash = seed;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    
    uint32_t k1 = key;
    
    k1 *= c1;
    k1 = (k1 << 15) | (k1 >> (32 - 15));  // ROTL32(k1, 15)
    k1 *= c2;

    hash ^= k1;
    hash = (hash << 13) | (hash >> (32 - 13));  // ROTL32(hash, 13)
    hash = hash * 5 + 0xe6546b64;

    // Finalization
    hash ^= sizeof(uint32_t);
    hash ^= (hash >> 16);
    hash *= 0x85ebca6b;
    hash ^= (hash >> 13);
    hash *= 0xc2b2ae35;
    hash ^= (hash >> 16);
    
    return hash;
}

static GHT_FORCE_INLINE uint64_t _ght_murmur3_64(ght_key_t key, uint64_t seed)
{
    uint64_t hash = seed;
    const uint64_t c1 = 0x87c37b91114253d5ULL;
    const uint64_t c2 = 0x4cf5ad432745937fULL;
    
    uint64_t k1 = key;
    
    k1 *= c1;
    k1 = (k1 << 31) | (k1 >> (64 - 31));  // ROTL64(k1, 31)
    k1 *= c2;

    hash ^= k1;
    hash = (hash << 27) | (hash >> (64 - 27));  // ROTL64(hash, 27)
    hash = hash * 5 + 0x52dce729;

    // Finalization
    hash ^= sizeof(uint64_t);
    hash ^= (hash >> 33);
    hash *= 0xff51afd7ed558ccdULL;
    hash ^= (hash >> 33);
    hash *= 0xc4ceb9fe1a85ec53ULL;
    hash ^= (hash >> 33);

    return hash;
}

static void _ght_delete_recursive(ght_bucket_t* bucket, ght_load_t* load, ght_deallocator_t deallocator)
{
    if (!bucket) return;
    
    if (bucket->next)
    {
        _ght_delete_recursive(bucket->next, load, deallocator);
    }
    
    if (deallocator)
    {
        deallocator(bucket->key, bucket->data);
    }
    
    free(bucket);
    (*load)--;
}

static void _ght_move_recursive(ght_bucket_t* bucket, ght_table_t* to_table)
{
    if (!bucket) return;
    
    if (bucket->next)
    {
        _ght_move_recursive(bucket->next, to_table);
    }

    ght_index_t index = bucket->hash % to_table->width;
    bucket->next = to_table->buckets[index];
    to_table->buckets[index] = bucket;
}