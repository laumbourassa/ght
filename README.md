# Generic Hash Table (GHT) Library

## Overview

The **Generic Hash Table (GHT)** library is a C library that provides a flexible and efficient hash table implementation with support for generic keys and values. It offers a customizable interface for hashing functions, memory management, and resizing, making it suitable for a wide range of applications.

## Features
- **Generic Key-Value Storage:** Supports different data types (integers, floats, and pointers) for both keys and values using a unified `ght_key_t` and `ght_data_t` type.
- **Customizable Hashing:** Allows users to define their own hash functions or use the built-in Murmur3 hash.
- **Automatic Resizing:** Supports automatic resizing based on load factor, optimizing memory usage and lookup efficiency.
- **Memory Management:** Offers user-defined deallocation functions for managing custom data structures.

## Getting Started

### Prerequisites
To use the GHT library, ensure you have a C compiler installed and the necessary tools to compile and link C programs.

### Installation

1. Clone the GHT library source files into your project directory.
2. Include the **ght.h** header file in your project:
```c
#include "ght.h"
```

### Compilation
To compile your program with the GHT library, ensure you link both the **ght.c** and **ght.h** files with your program:

```bash
gcc -o your_program your_program.c ght.c
```

### Basic Usage Example

```c
#include <stdio.h>
#include "ght.h"

int main()
{
  // Create a hash table
  ght_table_t* table = ght_create(NULL);

  // Insert key-value pairs
  ght_insert(table, GHT_KEY(1), GHT_DATA(100));
  ght_insert(table, GHT_KEY(2), GHT_DATA(200));

  // Search for a value by key
  printf("Value for key 1: %ld\n", (long) ght_search(table, GHT_KEY(1)));

  // Get table load and width
  printf("Table load: %zu\n", ght_load(table));
  printf("Table width: %zu\n", ght_width(table));

  // Delete a key-value pair
  ght_delete(table, GHT_KEY(1));

  // Destroy the table when done
  ght_destroy(table);

  return 0;
}
```

### API Documentation

#### Table Management
- `ght_table_t* ght_create(ght_cfg_t* cfg);`  
  Creates and returns a new hash table.

- `ght_status_t ght_destroy(ght_table_t* table);`  
  Destroys the table and frees all allocated memory using a custom deallocator if provided.

- `ght_load_t ght_load(ght_table_t* table);`  
  Returns the number of elements in the table.

- `ght_width_t ght_width(ght_table_t* table);`  
  Returns the width (number of buckets) of the table.

- `ght_load_factor_t ght_load_factor(ght_table_t* table);`  
  Returns the load factor of the table.

- `ght_status_t ght_resize(ght_table_t* table, ght_width_t width);`  
  Resizes the table to the specified width.

#### Data Operations
- `ght_status_t ght_insert(ght_table_t* table, ght_key_t key, ght_data_t data);`  
  Inserts a key-value pair into the table.

- `ght_data_t ght_search(ght_table_t* table, ght_key_t key);`  
  Searches and returns the value associated with the given key, or 0 if not found.

- `ght_status_t ght_delete(ght_table_t* table, ght_key_t key);`  
  Deletes the key-value pair from the table.

#### Conversion Macros
- `GHT_DATA(data)`  
  Converts various data types (integers, floats, pointers) to `ght_data_t`, which is used in the hash table.

- `GHT_KEY(key)`  
  Converts various data types to `ght_key_t`, which is used as a key in the hash table.

## License

The GHT library is released under the **MIT License**. You are free to use, modify, and distribute it under the terms of the license. See the [MIT License](https://opensource.org/licenses/MIT) for more details.

## Author

This library was developed by **Laurent Mailloux-Bourassa**.