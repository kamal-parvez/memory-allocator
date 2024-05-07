#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>  // For memcpy
#include <stdint.h>  // For SIZE_MAX
#include "block_meta.h"


// typedef struct block_meta {
//   size_t size;
//   struct block_meta *next;
//   int free;
// } block_meta;


// Global base for the free list
block_meta *global_base = NULL;  // Head of the free list
block_meta *last_alloc = NULL;   // Pointer to the last allocated block



size_t calculate_usable_memory() {
    size_t total_free_memory = 0;
    block_meta* current = global_base;

    while (current != NULL) {
        if (current->free) {
            total_free_memory += current->size;
        }
        current = current->next;
    }

    return total_free_memory;
}

void reset_memory_tracking() {
    global_base = NULL;
    last_alloc = NULL;
    // Add any additional cleanup needed for other global tracking variables
}




// First Fit Algorithm

block_meta* find_first_fit(size_t size) {
    block_meta* current = global_base;
    while (current != NULL) {
        if (current->free && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    return NULL;  // No suitable block found
}

block_meta* request_space_first_fit(block_meta* last, size_t size) {
    block_meta* block = sbrk(size + sizeof(block_meta));
    if (block == (void*) -1) {
        return NULL; // sbrk failed, no memory allocated
    }

    block->size = size;
    block->free = 0;
    block->next = NULL;

    if (last) {  // NULL on the first call to malloc
        last->next = block;
    }

    return block;
}

void* first_fit_alloc(size_t size) {
    if (size <= 0) {
        return NULL;
    }

    block_meta *block = find_first_fit(size);
    if (!block) {  // No fitting block found, need to request more space
        block = request_space_first_fit(NULL, size);
        if (!block) {
            return NULL;  // Failed to request space
        }
    } else {
        block->free = 0;  // Mark block as not free
    }

    return (block + 1);  // Return a pointer to the usable memory area, skipping the block meta
}

void first_fit_free(void* ptr) {
    if (!ptr) {
        return;
    }

    block_meta* block_ptr = (block_meta*)ptr - 1;
    block_ptr->free = 1;

    // Optional: Coalesce free blocks here
}






// Worst Fit Algorithm
block_meta* find_worst_fit(size_t size) {
    block_meta *current = global_base;
    block_meta *worst_fit = NULL;

    while (current != NULL) {
        if (current->free && current->size >= size) {
            if (worst_fit == NULL || current->size > worst_fit->size) {
                worst_fit = current;
            }
        }
        current = current->next;
    }
    return worst_fit;
}

block_meta* request_space_worst_fit(block_meta* last, size_t size) {
    block_meta* block = sbrk(size + sizeof(block_meta));
    if (block == (void*) -1) {
        return NULL; // sbrk failed, no memory allocated
    }

    block->size = size;
    block->free = 0;
    block->next = NULL;

    if (last) {  // Link the new block to the last block in the list
        last->next = block;
    }

    return block;
}


void* worst_fit_alloc(size_t size) {
    if (size <= 0) {
        return NULL;
    }

    block_meta *block = find_worst_fit(size);
    if (!block) {  // No suitable block found, need to request more space
        block = request_space_worst_fit(NULL, size);
        if (!block) {
            return NULL;  // Failed to request space
        }
    } else {
        block->free = 0;  // Mark block as allocated
    }

    return (block + 1);  // Return a pointer to the usable memory area, skipping the block metadata
}

void worst_fit_free(void* ptr) {
    if (!ptr) {
        return;
    }

    block_meta* block_ptr = (block_meta*)ptr - 1;
    block_ptr->free = 1;

    // Optional: Coalesce free blocks here
}





// Next Fit Algorithm
block_meta* find_next_fit(size_t size) {
    block_meta *start = last_alloc ? last_alloc->next : global_base;

    // First, try to find a block starting from last_alloc
    block_meta *current = start;
    do {
        if (!current) {  // If reaching end of list, wrap around to the start
            current = global_base;
        }
        if (current->free && current->size >= size) {
            last_alloc = current;  // Update last_alloc to the current block
            return current;
        }
        current = current->next;
    } while (current != start);  // Continue until we've wrapped around to the start

    return NULL;  // No suitable block found
}

block_meta* request_space_next_fit(block_meta* last, size_t size) {
    block_meta* block = sbrk(size + sizeof(block_meta));
    if (block == (void*) -1) {
        return NULL; // sbrk failed, no memory allocated
    }

    block->size = size;
    block->free = 0;
    block->next = NULL;

    if (last) {  // Link the new block to the last block in the list
        last->next = block;
    } else {  // No blocks have been allocated yet
        global_base = block;
    }

    last_alloc = block;  // Update last_alloc to the newly created block
    return block;
}


void* next_fit_alloc(size_t size) {
    if (size <= 0) {
        return NULL;
    }

    block_meta *block = find_next_fit(size);
    if (!block) {  // No suitable block found, need to request more space
        block = request_space_next_fit(last_alloc, size);
        if (!block) {
            return NULL;  // Failed to request space
        }
    } else {
        block->free = 0;  // Mark block as allocated
    }

    return (block + 1);  // Return a pointer to the usable memory area, skipping the block metadata
}

void next_fit_free(void* ptr) {
    if (!ptr) {
        return;
    }

    block_meta* block_ptr = (block_meta*)ptr - 1;
    block_ptr->free = 1;

    // Optional: Coalesce free blocks here
}




// Best Fit Algorithm

block_meta* find_best_fit(block_meta **last, size_t size) {
    block_meta *best_fit = NULL;
    block_meta *current = global_base;
    while (current) {
        if (current->free && current->size >= size) {
            if (!best_fit || current->size < best_fit->size) {
                best_fit = current;
            }
        }
        *last = current;
        current = current->next;
    }

    return best_fit;
}

void split_block(block_meta* block, size_t size) {
    block_meta* new_block = (block_meta*)((char*)block + size + sizeof(block_meta));
    new_block->size = block->size - size - sizeof(block_meta);
    new_block->free = 1;
    new_block->next = block->next;
    block->size = size;
    block->next = new_block;
}

block_meta* find_free_block(block_meta **last, size_t size) {
  block_meta *current = global_base;
  while (current && !(current->free && current->size >= size)) {
    *last = current;
    current = current->next;
  }
  return current;
}

block_meta* request_space_best_fit(block_meta* last, size_t size) {
    // Adjust size to include the metadata structure
    size_t total_size = size + sizeof(block_meta);
    // Use sbrk to request memory: move the program break
    block_meta* block = sbrk(total_size);
    if (block == (void*) -1) {
        return NULL; // sbrk failed, no memory allocated
    }

    // If there's a last block, update its 'next' pointer
    if (last) {
        last->next = block;
    }

    // Initialize the new block's metadata
    block->size = size;
    block->next = NULL;
    block->free = 0;  // Mark as allocated

    return block;
}


void* best_fit_alloc(size_t size) {
    block_meta* block;
    if (size <= 0) {
        return NULL;
    }

    if (!global_base) { // First call
        block = request_space_best_fit(NULL, size);
        if (!block) {
            return NULL;
        }
        global_base = block;
    } else {
        block_meta *last = global_base;
        block = find_best_fit(&last, size);
        if (!block) { // Failed to find fit
            block = request_space_best_fit(last, size);
            if (!block) {
                return NULL;
            }
        } else {
            // Optional: split block if difference is significant
            if (block->size > size + sizeof(block_meta) + 4) { // Threshold of 4 bytes
                split_block(block, size);
            }
            block->free = 0;
        }
    }
    return (block+1);
}


void best_fit_free(void* ptr) {
    if (!ptr) {
        return;
    }

    block_meta* block_ptr = (block_meta*)ptr - 1;
    block_ptr->free = 1;

    // Optional: Coalesce free blocks here
}



// void* my_realloc(void* ptr, size_t size) {
//     if (size == 0) {
//         my_free(ptr);
//         return NULL;
//     }

//     if (!ptr) {
//         return my_malloc(size);
//     }

//     block_meta* block = (block_meta*)ptr - 1;  // Get the block metadata
//     if (block->size >= size) {
//         // If the block is already big enough, return the same pointer
//         return ptr;
//     }

//     // Allocate a new block
//     void* new_ptr = my_malloc(size);
//     if (!new_ptr) {
//         return NULL; // Allocation failed
//     }

//     // Copy the old data to the new block
//     memcpy(new_ptr, ptr, block->size);  // Only copy the old block's data
//     my_free(ptr);  // Free the old block

//     return new_ptr;
// }


// void* my_calloc(size_t num, size_t size) {
//     // Check for overflow in size calculation
//     if (size != 0 && num > SIZE_MAX / size) {
//         // Size calculation would overflow
//         return NULL;
//     }
    
//     size_t total_size = num * size;
//     void* ptr = my_malloc(total_size);
//     if (ptr) {
//         memset(ptr, 0, total_size);  // Set allocated memory to zero
//     }
//     return ptr;
// }
