#include "iaddrs.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// #include "SFMT-src-1.4.1/SFMT.h"

#ifndef DEBUG_RESIZE
#define DEBUG_RESIZE 0
#endif

#ifndef IADDR_CACHE
#define IADDR_CACHE 0
#endif

/* static int min(int a, int b) { */
/*   return a < b ? a : b; */
/* } */

/**
 * Method implementations
 */
// Starting capacity of the hash table is 2^4 entries.
static const int START_LG_CAPACITY = 4;

// Torlate entries being displaced by a constant amount
/* static const size_t MAX_DISPLACEMENT = 64 / sizeof(iaddr_record_t); */
/* static const size_t MAX_DISPLACEMENT = 16; */
#if IADDR_CACHE
static const int IADDR_CACHE_SIZE = 8;
#endif

// Use a standardish trick from hardware caching to hash rip.
static const uint64_t TAG_OFFSET = 2;
/* static const uint64_t PRIME = (uint32_t)(-5); */
/* static const uint64_t ASEED = 0x8c678e6b; */
/* static const uint64_t BSEED = 0x9c16f733; */
/* static const int MIX_ROUNDS = 4; */

static size_t hash(uintptr_t iaddr, int lg_capacity) {
  uint64_t mask = (1 << lg_capacity) - 1;
  uint64_t key = (uint64_t)(iaddr >> TAG_OFFSET);
  /* uint64_t h = (uint64_t)((ASEED * key + BSEED) % PRIME); */
  /* for (int i = 0; i < MIX_ROUNDS; ++i) { */
  /*   h = h * (2 * h + 1); */
  /*   h = (h << (sizeof(h) / 2)) | (h >> (sizeof(h) / 2)); */
  /* } */
  uint64_t h = key;
  return (size_t)(h & mask);
}


// Return true if this entry is empty, false otherwise.
static inline bool empty_record_p(const iaddr_record_t *entry) {
  return (0 == entry->iaddr);
}


// Create an empty hashtable entry
static void make_empty_iaddr_record(iaddr_record_t *entry) {
  entry->iaddr = (uintptr_t)NULL;
}


// Allocate an empty hash table with 2^lg_capacity entries
static iaddr_table_t* iaddr_table_alloc(int lg_capacity) {
  assert(lg_capacity >= START_LG_CAPACITY);
  size_t capacity = 1 << lg_capacity;
  iaddr_table_t *table =
    (iaddr_table_t*)malloc(sizeof(iaddr_table_t)
                               + (capacity * sizeof(iaddr_record_t*)));

  table->lg_capacity = lg_capacity;
  table->table_size = 0;
#if IADDR_CACHE
  table->iaddr_cache = NULL;
#endif
  for (size_t i = 0; i < capacity; ++i) {
    /* make_empty_iaddr_record(&(table->records[i])); */
    table->records[i] = NULL;
  }

  return table;
}


// Create a new, empty hashtable.  Returns a pointer to the hashtable
// created.
iaddr_table_t* iaddr_table_create(void) {
  iaddr_table_t *tab = iaddr_table_alloc(START_LG_CAPACITY);
#if IADDR_CACHE
  tab->iaddr_cache
      = (iaddr_cache_el_t*)malloc(sizeof(iaddr_cache_el_t)
                                      * IADDR_CACHE_SIZE);
  // Initialize cache
  iaddr_cache_el_t *cache_el = tab->iaddr_cache;
  for (int i = 0; i < IADDR_CACHE_SIZE - 1; ++i) {
    cache_el->next = cache_el + 1;
    cache_el->record = NULL;
    cache_el = cache_el->next;
  }
  cache_el->next = NULL;
  cache_el->record = NULL;
#endif
  return tab;
}

// Helper function to get the entry in tab corresponding to iaddr.
// Returns a pointer to the entry if it can find a place to store it,
// NULL otherwise.
iaddr_record_t*
get_iaddr_record_const(uintptr_t iaddr, iaddr_table_t *tab) {

  assert((uintptr_t)NULL != iaddr);

#if IADDR_CACHE
  // Search for call site in cache
  iaddr_cache_el_t *cache_el = tab->iaddr_cache;
  iaddr_cache_el_t *last_cache_el = NULL;
  while (NULL != cache_el->record) {
    if (iaddr == cache_el->record->iaddr) {
      // Move record to front of cache
      if (NULL != last_cache_el) {
        last_cache_el->next = cache_el->next;
        cache_el->next = tab->iaddr_cache;
        tab->iaddr_cache = cache_el;
      }
      return cache_el->record;
    }
    if (NULL == cache_el->next) {
      // No more elements in cache
      break;
    }
    last_cache_el = cache_el;
    cache_el = cache_el->next;
  }
#endif

  // Call site not found in cache.  Hash the call site and search the
  // table.
  iaddr_record_t **first_record = &(tab->records[hash(iaddr, tab->lg_capacity)]);

  // Scan linked list
  iaddr_record_t *record = *first_record;
  iaddr_record_t *last_record = NULL;
  while (NULL != record && iaddr != record->iaddr) {
    last_record = record;
    record = record->next;
  }

  if (NULL == record) {
    // Allocate a new record
    record = (iaddr_record_t*)malloc(sizeof(iaddr_record_t));
    make_empty_iaddr_record(record);
    record->next = *first_record;
    *first_record = record;    
  } else if (NULL != last_record) {
    // Move record to front
    last_record->next = record->next;
    record->next = *first_record;
    *first_record = record;
  }

#if IADDR_CACHE
  // Place record at front of cache
  cache_el->record = record;
  // Move record to front of cache
  if (NULL != last_cache_el) {
    last_cache_el->next = cache_el->next;
    cache_el->next = tab->iaddr_cache;
    tab->iaddr_cache = cache_el;
  }
#endif

  /* int disp; */
  /* for (disp = 0; disp < min(MAX_DISPLACEMENT, (1 << tab->lg_capacity)); ++disp) { */
  /*   if (empty_record_p(record) || iaddr == record->iaddr) { */
  /*     break; */
  /*   } */
  /*   ++record; */
  /*   // Wrap around to the beginning */
  /*   if (&(tab->records[1 << tab->lg_capacity]) == record) { */
  /*     record = &(tab->records[0]); */
  /*   } */
  /* } */

  /* // Return false if the record was not found in the target area. */
  /* if (min(MAX_DISPLACEMENT, (1 << tab->lg_capacity))  <= disp) { */
  /*   return NULL; */
  /* } */

  return record;
}


// Return a hashtable with the contents of tab and more capacity.
static iaddr_table_t* increase_table_capacity(const iaddr_table_t *tab) {

  int new_lg_capacity = tab->lg_capacity + 1;
  iaddr_table_t *new_tab;

  new_tab = iaddr_table_alloc(new_lg_capacity);

  for (size_t i = 0; i < (1 << tab->lg_capacity); ++i) {
    iaddr_record_t *old = tab->records[i];
    iaddr_record_t *next_old;
    while (NULL != old) {
      next_old = old->next;
      iaddr_record_t **new = 
          &(new_tab->records[hash(old->iaddr, new_tab->lg_capacity)]);
      old->next = *new;
      *new = old;
      old = next_old;
    }
  }

  new_tab->table_size = tab->table_size;
#if IADDR_CACHE
  new_tab->iaddr_cache = tab->iaddr_cache;
#endif
  return new_tab;
}


// Add entry to tab, resizing tab if necessary.  Returns a pointer to
// the entry if it can find a place to store it, NULL otherwise.
static iaddr_record_t*
get_iaddr_record(uintptr_t iaddr, iaddr_table_t **tab) {
  return get_iaddr_record_const(iaddr, *tab);
/* #if DEBUG_RESIZE */
/*   int old_table_cap = 1 << (*tab)->lg_capacity; */
/* #endif */

/*   iaddr_record_t *entry; */
/*   while (NULL == (entry = get_iaddr_record_const(iaddr, *tab))) { */

/*     iaddr_table_t *new_tab = increase_table_capacity(*tab); */

/*     assert(new_tab); */

/*     free(*tab); */
/*     *tab = new_tab; */
/*   } */
/* #if DEBUG_RESIZE */
/*   if (1 << (*tab)->lg_capacity > 2 * old_table_cap) { */
/*     fprintf(stderr, "get_iaddr_record: new table capacity %d\n", */
/*     	    1 << (*tab)->lg_capacity); */
/*   } */
/* #endif */
/*   return entry; */
}

// Add the given iaddr_record_t data to **tab.  Returns 1 if iaddr was
// not previously on the stack, 0 if iaddr is already in the stack, -1
// on error.
int32_t add_to_iaddr_table(iaddr_table_t **tab, uintptr_t iaddr) {

  iaddr_record_t *record = get_iaddr_record(iaddr, tab);
  assert(NULL != record);

  if (NULL == record) {
    return -1;
  }
  
  if (empty_record_p(record)) {
    // Grow table if capacity exceeds 50%
    if (((*tab)->table_size + 1) > (1 << ((*tab)->lg_capacity - 1))) {
      iaddr_table_t *new_tab = increase_table_capacity(*tab);
      assert(new_tab);
      free(*tab);
      *tab = new_tab;
      record = get_iaddr_record(iaddr, tab);
      assert(empty_record_p(record));
    }
    record->iaddr = iaddr;
    record->on_stack = true;
    record->recursive = false;
    record->index = (*tab)->table_size++;
    return 1;
  }
  assert(record->iaddr == iaddr);
  if (record->on_stack) {
    record->recursive = true;
    return 0;
  }
  record->on_stack = true;
  return 1;
}

void iaddr_table_free(iaddr_table_t *tab) {
  for (int i = 0; i < (1 << tab->lg_capacity); ++i) {
    iaddr_record_t *record = tab->records[i];
    iaddr_record_t *next_record;
    while (NULL != record) {
      next_record = record->next;
      free(record);
      record = next_record;
    }
  }

#if IADDR_CACHE
  iaddr_cache_el_t *cache_el = tab->iaddr_cache;
  iaddr_cache_el_t *cache_start = cache_el;
  while (NULL != cache_el) {
    if ((uintptr_t)cache_el < (uintptr_t)cache_start) {
      cache_start = cache_el;
    }
    cache_el = cache_el->next;
  }
  free(cache_start);
#endif

  free(tab);
}
