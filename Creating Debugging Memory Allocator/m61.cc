#define M61_DISABLE 1
#include "m61.hh"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cinttypes>
#include <cassert>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

// Global variables

int nactive_var;
int active_size_var;
int ntotal_var;
int total_size_var;
int nfail_var;
int fail_size_var;
int free_var;
int free_var_calloc;
long free_var_bytes;
long* heap_list_min;
long* heap_list_max = 0;
long file_number;

std::unordered_map<void*, long> mp; // map to store active malloc calls
std::unordered_map<void*, long> line_number; // map to store active malloc line numbers
std::map<std::pair<const char*, long>, long> file_line_storage; // map to store heavy hitters info
std::unordered_set<void*> doublefree_checker; // map to check freed pointers

/// m61_malloc(sz, file, line)
///    Return a pointer to `sz` bytes of newly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc must
///    return a unique, newly-allocated pointer value. The allocation
///    request was at location `file`:`line`.

void* m61_malloc(size_t sz, const char* file, long line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    file_number = (long) *file;
    if ((sz*100/sz) != 100) { // check for integer overflow
      ++nfail_var;
      fail_size_var += sz;
      return nullptr;
    }
    char* b_m = (char*)base_malloc(sz + 1); //Allocate extra spot for metadata
    if (b_m) {
      b_m[sz] = (long int) 50; // Assign canary value
    }
    long int* b_m_1 = (long int*) b_m; // Cast to long int to work with
    //heap variables

    // Your code here.
    // if m61_malloc returns a NULL pointer, the allocation failed
    if (b_m_1 != NULL) {
      // Increase ntotal_var everytime malloc is called
      ++ntotal_var;
      // Increase total_size_var by the size of argument, which is
      // number of bytes allocated
      total_size_var += sz;
      // Use unordered map to keep track of active bytes corresponding to a
      // malloc-returned pointer
      active_size_var += sz;
      mp[b_m_1] = sz;
      if (file_line_storage.count({file, line}) != 0) {
        file_line_storage[{file, line}] += sz;
      }
      else {
        file_line_storage[{file, line}] = sz;
      }
      line_number[b_m_1] = line;
      //Initialize min and max heap if NULL
      if (heap_list_min == NULL) {
        heap_list_min = b_m_1;
      }

      if (heap_list_max == NULL) {
        heap_list_min = b_m_1 + sz;
      }
      // Functions to compare current allocation min/max to global min/max
      if (b_m_1 < heap_list_min) {
        heap_list_min = b_m_1;
      }

      if ((b_m_1 + sz) > heap_list_max) {
        heap_list_max = b_m_1 + sz;
      }

    }
    else { // Icrement fail variable if returned pointer is null
      ++nfail_var;
      fail_size_var += sz;
    }

    return b_m_1;
}


/// m61_free(ptr, file, line)
///    Free the memory space pointed to by `ptr`, which must have been
///    returned by a previous call to m61_malloc. If `ptr == NULL`,
///    does nothing. The free was called at location `file`:`line`.

void m61_free(void* ptr, const char* file, long line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.
    // Ensures input pointer is returned from malloc
    if (ptr != nullptr && mp[ptr]) {
      long actual_size = (long) (mp[ptr] + 1); // Get size of allocated block
      // from map and add  1 for metadata
      char* ptr_1 = (char*) ptr;  // Cast input ptr to char type to index
      // Checks if the "metadata" has been changed
      if (ptr_1[actual_size - 1] != 50) {
        fprintf(stderr, "MEMORY BUG: detected wild write during free of pointer %p\n", ptr);
        abort();
      }
      ++free_var;
      //Get number of active allocated bytes from map
      free_var_bytes += (long) mp[ptr];
      doublefree_checker.insert(ptr);
      //Remove pointer from map after freeing to keep metadata bounded
      mp.erase(ptr);
      base_free(ptr);
    }
    //Second argument checks if ptr has been freed already
    else if (ptr != nullptr && doublefree_checker.count(ptr) != 0){
      fprintf(stderr, "MEMORY BUG: invalid free of pointer %p, double free\n", ptr);
      abort();
    }
    else if (ptr == nullptr) {
      base_free(ptr);
    }
    // check if not in heap
    else if (ptr < heap_list_min || ptr > heap_list_max) {
      fprintf(stderr, "MEMORY BUG: test%s.cc:%li: invalid free of pointer %p, not in heap\n", file, line, ptr);
      abort();
    }
    // check for "not allocated" errors
    else if (!mp[ptr]) {
      fprintf(stderr, "MEMORY BUG: test%s.cc:%li: invalid free of pointer %p, not allocated\n", file, line, ptr);
      for (auto it = mp.begin(); it != mp.end(); ++it) {
        // check if pointer is between the start of memory block and end
        if (ptr > it->first && (uintptr_t)ptr < ((uintptr_t)it->first + (uintptr_t)it->second)) {
          // if it is, find difference and print out error
          long difference = (uintptr_t) ptr - (uintptr_t) it->first;
          fprintf(stderr, "test%li.cc:9: %p is %li bytes inside a %li byte region allocated here", file_number, ptr, difference, it->second);
          abort();
        }
      }
      abort();
    }
}


/// m61_calloc(nmemb, sz, file, line)
///    Return a pointer to newly-allocated dynamic memory big enough to
///    hold an array of `nmemb` elements of `sz` bytes each. If `sz == 0`,
///    then must return a unique, newly-allocated pointer value. Returned
///    memory should be initialized to zero. The allocation request was at
///    location `file`:`line`.

void* m61_calloc(size_t nmemb, size_t sz, const char* file, long line) {
    // Your code here (to fix test019).
    if ((nmemb * sz)/nmemb != sz){ // overflow check
      nfail_var++;
      return nullptr;
    }
    void* ptr = m61_malloc(nmemb * sz, file, line);
    if (ptr) {
        memset(ptr, 0, nmemb * sz);
    }

    return ptr;
}


/// m61_get_statistics(stats)
///    Store the current memory statistics in `*stats`.

void m61_get_statistics(m61_statistics* stats) {
    // Stub: set all statistics to enormous numbers
    memset(stats, 255, sizeof(m61_statistics));
    // Your code here.
    // Initialized the stats struct
    memset(stats, 0, sizeof(m61_statistics));
    // Assign global variable to corresponding struct element
    nactive_var = ntotal_var - free_var;
    stats->nactive = nactive_var;
    active_size_var = total_size_var - free_var_bytes;
    stats->active_size = active_size_var;
    stats->ntotal = ntotal_var;
    stats->total_size = total_size_var;
    stats->nfail = nfail_var;
    stats->fail_size = fail_size_var;
    stats->heap_min = (uintptr_t)heap_list_min;
    stats->heap_max = (uintptr_t)heap_list_max;
}


/// m61_print_statistics()
///    Print the current memory statistics.

void m61_print_statistics() {
    m61_statistics stats;
    m61_get_statistics(&stats);

    printf("alloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("alloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}


/// m61_print_leak_report()
///    Print a report of all currently-active allocated blocks of dynamic
///    memory.

void m61_print_leak_report() {
    // Your code here.
    // Iterates through map of active allocations that are not freed
    for (auto it = mp.begin(); it != mp.end(); ++it) {
      fprintf(stdout, "LEAK CHECK: test%li.cc:%li: allocated object %p with size %li\n", file_number, line_number[it->first], it->first, it->second);
    }
}


/// m61_print_heavy_hitter_report()
///    Print a report of heavily-used allocation locations.

void m61_print_heavy_hitter_report() {
    // Your heavy-hitters code here
    // create map to store and sort >20% byte values
    std::map<long, std::pair<const char*, long>> sort_map;
    long total_map_bytes = 0;
    // iterate through all values to sum them
    for (auto it = file_line_storage.begin(); it != file_line_storage.end(); ++it) {
      total_map_bytes += it->second;
    }
    // Iterate through map and divide each value by total bytes to get percent
    for (auto it = file_line_storage.begin(); it != file_line_storage.end(); ++it) {
      float byte_fraction = ((float) it->second / (float) total_map_bytes);
      // if greater then 20%, add to sorted map
      if (byte_fraction >= 0.20) {
        sort_map[it->second] = {it->first.first, it->first.second};
      }
    }
    // iterate through ordered map in reverse
    for (auto it = sort_map.rbegin(); it != sort_map.rend(); ++it) {
      float byte_percent = 100*((float) it->first / (float) total_map_bytes);
      fprintf(stdout, "HEAVY HITTER: %s:%li: %li bytes (~%.1f%%)\n", it->second.first, it->second.second, it->first, byte_percent);
    }
}
