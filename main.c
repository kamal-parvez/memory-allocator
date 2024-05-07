#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>
#include "block_meta.h"


typedef void* (*alloc_func)(size_t size);
typedef void (*free_func)(void* ptr);

typedef struct Allocator {
    alloc_func alloc;
    free_func free;
    char* name;
} Allocator;



// C Library Malloc and Free
// void* libc_malloc(size_t size) {
//     return malloc(size);
// }

// void libc_free(void* ptr) {
//     free(ptr);
// }


// Allocator prototypes
void* best_fit_alloc(size_t size);
void best_fit_free(void* ptr);
void* first_fit_alloc(size_t size);
void first_fit_free(void* ptr);
void* worst_fit_alloc(size_t size);
void worst_fit_free(void* ptr);
void* next_fit_alloc(size_t size);
void next_fit_free(void* ptr);
size_t calculate_usable_memory();
void reset_memory_tracking();


Allocator allocators[] = {
    {best_fit_alloc, best_fit_free, "Best Fit"},
    {first_fit_alloc, first_fit_free, "First Fit"},
    {worst_fit_alloc, worst_fit_free, "Worst Fit"},
    {next_fit_alloc, next_fit_free, "Next Fit"}
    // {libc_malloc, libc_free, "C Library"}
};




// Latency Measure
double measure_latency(alloc_func alloc, free_func free, size_t size, int iterations) {
    struct timespec start, end;
    double total_time = 0;
    void* ptr;

    for (int i = 0; i < iterations; i++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        ptr = alloc(size);
        clock_gettime(CLOCK_MONOTONIC, &end);
        total_time += (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
        free(ptr);
    }

    return total_time / iterations;  // Average time per operation in nanoseconds
}

void run_latency_tests(size_t sizes[], int num_sizes, int iterations, Allocator allocators[], int num_allocators) {
    FILE* file = fopen("latency_results.txt", "w");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    // Print header
    fprintf(file, "Size");
    for (int i = 0; i < num_allocators; i++) {
        fprintf(file, ",%s", allocators[i].name);
    }
    fprintf(file, "\n");

    for (int j = 0; j < num_sizes; j++) {
        fprintf(file, "%zu", sizes[j]); // Print the size
        for (int i = 0; i < num_allocators; i++) {
            double latency = measure_latency(allocators[i].alloc, allocators[i].free, sizes[j], iterations);
            fprintf(file, ",%f", latency); // Print the latency after the size, separated by commas
        }
        fprintf(file, "\n");
    }

    fclose(file);
}



// Throughput Measure
long measure_throughput(alloc_func alloc, free_func free, size_t size, int test_duration) {
    long operations = 0;
    time_t start_time = time(NULL);
    time_t current_time;

    do {
        void* ptr = alloc(size);
        free(ptr);
        operations++;  // Increment operation count for each alloc/free pair
        current_time = time(NULL);
    } while (current_time - start_time < test_duration);

    return operations / test_duration;  // Return operations per second
}


void run_throughput_tests(size_t sizes[], int num_sizes, int test_duration, Allocator allocators[], int num_allocators) {
    FILE* file = fopen("throughput_results.txt", "w");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    fprintf(file, "Size");
    for (int i = 0; i < num_allocators; i++) {
        fprintf(file, ",%s", allocators[i].name);
    }
    fprintf(file, "\n");

    for (int j = 0; j < num_sizes; j++) {
        fprintf(file, "%zu", sizes[j]);
        for (int i = 0; i < num_allocators; i++) {
            long throughput = measure_throughput(allocators[i].alloc, allocators[i].free, sizes[j], test_duration);
            fprintf(file, ",%ld", throughput);
        }
        fprintf(file, "\n");
    }

    fclose(file);
}



// Memory Utilization Efficiency
double simulate_allocations(Allocator allocator, size_t iterations, size_t max_size) {
    srand((unsigned)time(NULL));
    void** ptrs = malloc(sizeof(void*) * iterations);
    size_t total_requested_memory = 0;
    size_t total_successful_allocated = 0; // Track successful allocations

    // Allocate memory
    for (size_t i = 0; i < iterations; i++) {
        size_t size = rand() % max_size + 1;
        ptrs[i] = allocator.alloc(size);
        if (ptrs[i]) {
            total_successful_allocated += size; // Only add successful allocations
        }
    }

    // Deallocate half of them randomly
    for (size_t i = 0; i < iterations / 2; i++) {
        size_t index = rand() % iterations;
        if (ptrs[index]) {
            allocator.free(ptrs[index]);
            ptrs[index] = NULL;
        }
    }

    size_t usable_memory = calculate_usable_memory();
    printf("usable_memory : %ld    total : %ld\n", usable_memory, total_successful_allocated);
    double utilization_percentage = (double)usable_memory / total_successful_allocated * 100;

    // Free remaining allocations
    for (size_t i = 0; i < iterations; i++) {
        if (ptrs[i]) {
            allocator.free(ptrs[i]);
        }
    }

    free(ptrs);
    return utilization_percentage;
}


// Stress Testing
void stress_test(Allocator allocator, int operations, FILE* resultFile) {
    srand(time(NULL));  // Seed for randomness
    void** pointers = malloc(operations * sizeof(void*));
    size_t* sizes = malloc(operations * sizeof(size_t));
    int alloc_count = 0;

    for (int i = 0; i < operations; i++) {
        size_t size = (rand() % 1024) + 16;  // Allocate between 16 and 1040 bytes
        pointers[i] = allocator.alloc(size);
        sizes[i] = size;
        if (pointers[i] != NULL) alloc_count++;
        if (i % 2 == 0 && i > 0) {  // Free every other allocation to increase fragmentation
            allocator.free(pointers[i - 1]);
            pointers[i - 1] = NULL;
        }
    }

    // Free any remaining allocations
    for (int i = 0; i < operations; i++) {
        if (pointers[i] != NULL) {
            allocator.free(pointers[i]);
        }
    }

    free(pointers);
    free(sizes);

    fprintf(resultFile, "%s,%d\n", allocator.name, alloc_count);
}



// Internal Fragmentation
size_t calculate_internal_fragmentation(Allocator alloc, size_t size) {
    void* ptr = alloc.alloc(size);
    if (!ptr) return 0;

    // Assuming block_meta is right before the allocated memory
    block_meta* block = (block_meta*)((char*)ptr - sizeof(block_meta));
    size_t internal_frag = block->size - size;
    alloc.free(ptr);
    return internal_frag;
}

size_t attempt_large_allocation(Allocator alloc, size_t size) {
    void* ptr = alloc.alloc(size);
    if (ptr) {
        alloc.free(ptr);
        return 1; // Success
    }
    return 0; // Failure, indicating external fragmentation
}


// Scalability Testing
// Function to perform allocation and deallocation
double perform_allocations(Allocator alloc, int operations) {
    clock_t start = clock();
    for (int i = 0; i < operations; i++) {
        void* ptr = alloc.alloc(1024);  // Example size
        alloc.free(ptr);
    }
    clock_t end = clock();
    double time_spent = ((double)(end - start)) / CLOCKS_PER_SEC;
    return time_spent;
}


// Thread function
void* thread_func(void* arg) {
    Allocator* allocator = (Allocator*)arg;
    perform_allocations(*allocator, 1000);
    return NULL;
}

// Test multi-threaded scalability
double test_multi_threaded(Allocator alloc, int num_threads) {
    pthread_t threads[num_threads];
    clock_t start = clock();

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, thread_func, &alloc);
    }
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_t end = clock();
    double time_spent = ((double)(end - start)) / CLOCKS_PER_SEC;
    return time_spent;
}




int main() {

    // Measure Latency
    size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};  // Example sizes in bytes
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    int iterations = 1000;  // Number of allocations/deallocations to average
    int num_allocators = sizeof(allocators) / sizeof(allocators[0]);

    run_latency_tests(sizes, num_sizes, iterations, allocators, num_allocators);

    printf("Tests completed. Results are saved to 'latency_results.txt'.\n");


    // Measure Throughput
    // Define sizes to test
    // size_t sizes[] = {16, 64, 256, 1024, 4096};
    // int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    // int num_allocators = sizeof(allocators) / sizeof(allocators[0]);

    // Test duration in seconds
    int test_duration = 10;  // Change this to increase or decrease the test duration

    // Run the tests
    run_throughput_tests(sizes, num_sizes, test_duration, allocators, num_allocators);

    printf("Throughput tests completed. Results are saved to 'throughput_results.txt'.\n");


    // Memory Utilization Efficiency Test
    // int num_allocators = sizeof(allocators) / sizeof(Allocator);
    // size_t sizes[] = {1000, 2000, 5000, 10000, 20000, 50000};  // Different sizes to test
    // int num_sizes = sizeof(sizes) / sizeof(size_t);

    FILE* resultFile = fopen("memory_utilization_results.csv", "w");
    if (resultFile == NULL) {
        perror("Failed to open results file");
        return 1;
    }
    fprintf(resultFile, "Allocator,Average Utilization Percentage\n");

    for (int i = 0; i < num_allocators; i++) {
        double total_percentage = 0;
        // reset_memory_tracking();
        for (int j = 0; j < num_sizes; j++) {
            double percentage = simulate_allocations(allocators[i], sizes[j], sizes[j]);
            total_percentage += percentage;
            printf("%s   %.2f\n", allocators[i].name, percentage);
        }
        double average_percentage = total_percentage / num_sizes;
        fprintf(resultFile, "%s,%.2f\n", allocators[i].name, average_percentage);
    }

    fclose(resultFile);
    printf("Tests completed. Results are saved to 'memory_utilization_results.csv'.\n");

    
    // Stress Testing
    int num_allocators = sizeof(allocators) / sizeof(Allocator);

    FILE* resultFile = fopen("stress_test_results.csv", "w");
    fprintf(resultFile, "Allocator,Successful Allocations\n");

    for (int i = 0; i < num_allocators; i++) {
        printf("Running stress test for %s...\n", allocators[i].name);
        stress_test(allocators[i], 100000, resultFile);  // Example: 100,000 operations
    }

    fclose(resultFile);
    printf("Stress tests completed. Results are saved to 'stress_test_results.csv'.\n");



    // Internal Fragmentation test
    size_t sizes[] = {16, 64, 256, 1024, 4096, 10000, 20000, 14000, 34665, 356, 500, 3359, 4543, 55683};
    size_t large_size = 100000; // For testing external fragmentation
    int num_sizes = sizeof(sizes) / sizeof(size_t);
    FILE* file = fopen("fragmentation_results.csv", "w");

    if (file == NULL) {
        perror("Failed to open file");
        return EXIT_FAILURE;
    }

    fprintf(file, "Allocator,Total Internal Fragmentation,Success Large Allocation\n");

    for (int i = 0; i < sizeof(allocators) / sizeof(Allocator); i++) {
        size_t total_internal_frag = 0;
        size_t successes = 0;

        for (int j = 0; j < num_sizes; j++) {
            total_internal_frag += calculate_internal_fragmentation(allocators[i], sizes[j]);
        }

        successes = attempt_large_allocation(allocators[i], large_size);
        fprintf(file, "%s,%zu,%zu\n", allocators[i].name, total_internal_frag, successes);
    }

    fclose(file);




    // Scalability Test
    int operations[] = {1000, 2000, 3000, 4000, 5000, 6000, 7000, 10000};
    int num_operations = sizeof(operations) / sizeof(int);

    printf("Type,Allocator,Operations,Time\n");
    for (int i = 0; i < sizeof(allocators) / sizeof(Allocator); i++) {
        for (int j = 0; j < num_operations; j++) {
            double time = perform_allocations(allocators[i], operations[j]);
            printf("SingleThreaded,%s,%d,%f\n", allocators[i].name, operations[j], time);
        }
    }

    printf("Type,Allocator,Threads,Time\n");
    for (int i = 0; i < sizeof(allocators) / sizeof(Allocator); i++) {
        double time = test_multi_threaded(allocators[i], 3);  // Testing with 5 threads
        printf("MultiThreaded,%s,%d,%f\n", allocators[i].name, 3, time);
    }

    for (int i = 0; i < sizeof(allocators) / sizeof(Allocator); i++) {
        double time = test_multi_threaded(allocators[i], 5);  // Testing with 5 threads
        printf("MultiThreaded,%s,%d,%f\n", allocators[i].name, 5, time);
    }

    for (int i = 0; i < sizeof(allocators) / sizeof(Allocator); i++) {
        double time = test_multi_threaded(allocators[i], 8);  // Testing with 5 threads
        printf("MultiThreaded,%s,%d,%f\n", allocators[i].name, 8, time);
    }

    for (int i = 0; i < sizeof(allocators) / sizeof(Allocator); i++) {
        double time = test_multi_threaded(allocators[i], 10);  // Testing with 5 threads
        printf("MultiThreaded,%s,%d,%f\n", allocators[i].name, 10, time);
    }

    for (int i = 0; i < sizeof(allocators) / sizeof(Allocator); i++) {
        double time = test_multi_threaded(allocators[i], 12);  // Testing with 5 threads
        printf("MultiThreaded,%s,%d,%f\n", allocators[i].name, 12, time);
    }


    return 0;
}

