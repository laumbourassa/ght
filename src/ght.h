/*
 * ght.h - Generic Hash Table interface
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

#ifndef GHT_H
#define GHT_H

#include <stdint.h>

#define	GHT_FORCE_INLINE inline __attribute__((always_inline))

typedef struct ght_table ght_table_t;   // Opaque type representing the hash table.
typedef uintptr_t ght_key_t;            // Type representing a key used to access the corresponding data in the table.
typedef uintptr_t ght_data_t;           // Type representing the data stored in the table.
typedef int8_t ght_status_t;            // Type indicating if an error occured while executing a function.
typedef size_t ght_index_t;             // Type representing the index of an item in the table.
typedef size_t ght_load_t;              // Type representing the number of elements in the table.
typedef size_t ght_width_t;             // Type representing the number of buckets in the table.
typedef size_t ght_hash_t;              // Type representing the result of a hashing function (digestor).
typedef double ght_load_factor_t;       // Type representing the load of table divided by its width.

typedef ght_hash_t (*ght_digestor_t)(ght_key_t key);                // User-provided hashing function
typedef void (*ght_deallocator_t)(ght_key_t key, ght_data_t data);  // User-provided deallocator function for custom structures

// Conversion functions for various types to ght_data_t
static GHT_FORCE_INLINE ght_data_t _ght_int8_to_data(int8_t data) {return (ght_data_t) data;}
static GHT_FORCE_INLINE ght_data_t _ght_int16_to_data(int16_t data) {return (ght_data_t) data;}
static GHT_FORCE_INLINE ght_data_t _ght_int32_to_data(int32_t data) {return (ght_data_t) data;}
static GHT_FORCE_INLINE ght_data_t _ght_int64_to_data(int64_t data) {return (ght_data_t) data;}
static GHT_FORCE_INLINE ght_data_t _ght_uint8_to_data(uint8_t data) {return (ght_data_t) data;}
static GHT_FORCE_INLINE ght_data_t _ght_uint16_to_data(uint16_t data) {return (ght_data_t) data;}
static GHT_FORCE_INLINE ght_data_t _ght_uint32_to_data(uint32_t data) {return (ght_data_t) data;}
static GHT_FORCE_INLINE ght_data_t _ght_uint64_to_data(uint64_t data) {return (ght_data_t) data;}
static GHT_FORCE_INLINE ght_data_t _ght_float_to_data(float data) {return *(ght_data_t*) &data;}
static GHT_FORCE_INLINE ght_data_t _ght_double_to_data(double data) {return *(ght_data_t*) &data;}
//static GHT_FORCE_INLINE ght_data_t _ght_longdouble_to_data(long double data) {return *(ght_data_t*) &data;} // Long double can be larger than ght_data_t
static GHT_FORCE_INLINE ght_data_t _ght_voidptr_to_data(void* data) {return (ght_data_t) data;}

#define GHT_DATA(data)  _Generic((data),                \
        int8_t: _ght_int8_to_data,                      \
        int16_t: _ght_int16_to_data,                    \
        int32_t: _ght_int32_to_data,                    \
        int64_t: _ght_int64_to_data,                    \
        uint8_t: _ght_uint8_to_data,                      \
        uint16_t: _ght_uint16_to_data,                    \
        uint32_t: _ght_uint32_to_data,                    \
        uint64_t: _ght_uint64_to_data,                    \
        float: _ght_float_to_data,                      \
        double: _ght_double_to_data,                    \
        default: _ght_voidptr_to_data                   \
        )(data)

#define GHT_KEY(key) GHT_DATA(key)

/**
 * @brief Creates a new hash table.
 * 
 * @param width The width of the table.
 * @param digestor Function to hash keys. If NULL, Murmur3 will be used by default.
 * @param auto_resize A value between 0 and 1 indicating the load factor at which the table should automatically double in width. 0 to never resize.
 * @return Pointer to the created ght_table_t or NULL on failure.
 */
ght_table_t* ght_create(ght_width_t width, ght_digestor_t digestor, ght_load_factor_t auto_resize);

/**
 * @brief Destroys the entire table and frees all allocated memory.
 * 
 * @param table The table to destroy.
 * @param deallocator Function to deallocate any custom data, or NULL.
 * @return 0 on success, -1 on failure.
 */
ght_status_t ght_destroy(ght_table_t* table, ght_deallocator_t deallocator);

/**
 * @brief Inserts data in the table and associates it to a key.
 * 
 * @param table The table to insert the data into.
 * @param key The key to associate the data with.
 * @param data The data to insert.
 * @return 0 on success, -1 on failure.
 */
ght_status_t ght_insert(ght_table_t* table, ght_key_t key, ght_data_t data);

/**
 * @brief Searches and returns the data associated to a key.
 * 
 * @param table The table to search.
 * @param key The key associated to the data.
 * @return The data or 0 if table is empty or data isn't found.
 */
ght_data_t ght_search(ght_table_t* table, ght_key_t key);

/**
 * @brief Deletes the data associated to a key.
 * 
 * @param table The table to delete the data from.
 * @param key The key associated to the data.
 * @param deallocator Function to deallocate any custom data, or NULL.
 * @return 0 on success, -1 on failure.
 */
ght_status_t ght_delete(ght_table_t* table, ght_key_t key, ght_deallocator_t deallocator);

/**
 * @brief Returns the number of elements in the table.
 * 
 * @param table The table to get the load of.
 * @return The number of elements in the table, or 0 if the table is NULL.
 */
ght_load_t ght_load(ght_table_t* table);

/**
 * @brief Returns the width of the table.
 * 
 * @param table The table to get the width of.
 * @return The width of the table, or 0 if the table is NULL.
 */
ght_width_t ght_width(ght_table_t* table);

/**
 * @brief Returns the load factor of the table.
 * 
 * @param table The table to get the load factor from.
 * @return The load factor of the table.
 */
ght_load_factor_t ght_load_factor(ght_table_t* table);

/**
 * @brief Resizes the table.
 * 
 * @param table The table to resize.
 * @param width The new width of the table.
 * @return 0 on success, -1 on failure.
 */
ght_status_t ght_resize(ght_table_t* table, ght_width_t width);

#endif /* GHT_H */