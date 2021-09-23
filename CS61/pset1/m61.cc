#define M61_DISABLE 1
#include "m61.hh"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cinttypes>
#include <cassert>
#include <map>
#include <unordered_set>


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
long file_line;

std::map<void*, long> mp;
std::unordered_set<void*> doublefree_checker;

/// m61_malloc(sz, file, line)
///    Return a pointer to `sz` bytes of newly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc must
///    return a unique, newly-allocated pointer value. The allocation
///    request was at location `file`:`line`.

void* m61_malloc(size_t sz, const char* file, long line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    file_number = (long) *file;
    file_line = line;
    if ((sz*100/sz) != 100) {
      ++nfail_var;
      fail_size_var += sz;
      return NULL;
    }
    char* b_m = (char*)base_malloc(sz); //Allocate extra spot for metadata
    if (b_m) {
      b_m[sz] = (long int) 50; //Assign canary value
    }
    long int* b_m_1 = (long int*) b_m; //Cast to long int to work with heap variables

    // Your code here.
    // if m61_malloc returns a NULL pointer, the allocation failed
    if (b_m_1 != NULL) {
      // Increase ntotal_var everytime malloc is called
      ++ntotal_var;
      // Increase total_size_var by the size of argument, which is number of bytes allocated
      total_size_var += sz;
      // Use unordered map to keep track of active bytes corresponding to a malloc-returned pointer
      active_size_var += sz;
      mp[b_m_1] = sz;
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
    else {
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
    if (ptr != nullptr && mp[ptr]) { //Second argument ensure input pointer is returned from malloc
      long actual_size = (long) (mp[ptr] + 1); // Get size of allocated block from map
      char* ptr_1 = (char*) ptr;  // Cast input ptr to char type
      if (ptr_1[actual_size - 1] != 50) { // Checks if the "metadata" has been changed
        fprintf(stderr, "MEMORY BUG: detected wild write during free of pointer %p\n", ptr);
        abort();
      }
      ++free_var;
      free_var_bytes += (long) mp[ptr]; //Get number of active allocated bytes from map
      doublefree_checker.insert(ptr);
      mp.erase(ptr); //Remove pointer from map after freeing to keep metadata bounded
      base_free(ptr);
    }
    else if (ptr != nullptr && doublefree_checker.count(ptr) != 0){//Second argument checks
      //if ptr has been freed already, output error
      fprintf(stderr, "MEMORY BUG: invalid free of pointer %p, double free\n", ptr);
      abort();
    }
    else if (ptr == nullptr) {
      base_free(ptr);
    }
    else if (ptr < heap_list_min || ptr > heap_list_max) {
      fprintf(stderr, "MEMORY BUG: test%s.cc:%li: invalid free of pointer %p, not in heap\n", file, line, ptr);
      abort();
    }
    else {
      fprintf(stderr, "MEMORY BUG: test%s.cc:%li: invalid free of pointer %p, not allocated\n", file, line, ptr);
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
    if ((nmemb * sz)/nmemb != sz){
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
    for (auto it = mp.begin(); it != mp.end(); ++it) {
      fprintf(stdout, "LEAK CHECK: test%li.cc:%li: allocated object %p with size %li\n", file_number, file_line, it->first, it->second);
    }
}


/// m61_print_heavy_hitter_report()
///    Print a report of heavily-used allocation locations.

void m61_print_heavy_hitter_report() {
    // Your heavy-hitters code here
}
