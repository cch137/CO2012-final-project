#include "types.h"

// Initial size of the hash table
#define HT_INITIAL_SIZE 16
// Load factor threshold for expanding the hash table
#define HT_LOAD_FACTOR_EXPAND 0.7
// Load factor threshold for shrinking the hash table
#define HT_LOAD_FACTOR_SHRINK 0.1

// Seed for the hash function, affecting hash distribution
extern db_uint_t hash_seed;

// Creates a new hash table context
DBHash *ht_create_context();

// Frees the memory allocated for a hash table context
void ht_free(DBHash *ht);

void ht_reset(DBHash *ht);

// Creates a new entry with the specified key and type; assigns value directly
DBHashEntry *ht_create_entry(char *key, db_type_t type, void *value);

void ht_free_entry(DBHashEntry *entry);

void ht_set_entry_value(DBHashEntry *entry, db_type_t type, void *value);

// Retrieves an entry by key; returns NULL if not found
DBHashEntry *ht_get_entry(DBHash *ht, const char *key);

// Adds an entry to the hash table
DBHashEntry *ht_add_entry(DBHash *ht, DBHashEntry *entry);

// Removes an entry by key; returns NULL if not found
DBHashEntry *ht_remove_entry(DBHash *ht, const char *key);
