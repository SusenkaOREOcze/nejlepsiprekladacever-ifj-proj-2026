/**
   @file hashtable_impl.h
   @author xbosakd00 (DasBean125)
   @date 2026-10-03

   @brief The private header file for the hashtable.c module.
*/

#ifndef HTBL_IMPL_H
#define HTBL_IMPL_H

#include "hashtable.h"

/*=====
  TYPES
  =====*/

/**
   A hashtable entry containing the key, its hash and the data stored at that particular key.
*/
typedef struct HashtableEntry {
	/**
	   The key of the given entry.  When the key has the value of an empty string "\0", the entry is
	   considered *Invalid*; as if it has been deleted (note that this doesn't need to be the case!).
	*/
	htbl_key_t key;

	/**
	   The hash of the key field. This is used for table resizing, so that the hash does not have to be
	   recalculated for every entry.
	*/
	size_t hash;

	/**
	   A byte array that stores the actual data at this particular key. Its very flexible.
	*/
	char data[];
} htbl_entry_t;

/**
   A hash table with implicit entry chaining.
*/
struct Hashtable {
	/**
	   The size (length) of the hashtable array.
	*/
	size_t size;

	/**
	   The number of _valid_ entries in the hash table. This means entries that don't have an empty key
	   (@see struct HashtableEntry).
	*/
	size_t count;

	/**
	   A array of pointers to entries stored in the hash table. This is a flexible array member.
	*/
	htbl_entry_t *array[];
};

/**
   A set of options used for adjusting the behavior of the htbl_find_entry() function.
*/
enum HashtableFindOptions {
	HTBL_FIND_VALID = 0,
	HTBL_FIND_INVALID,
};

/*=========
  FUNCTIONS
  =========*/

#ifndef UNIT_UNDER_TEST

/**
   Calculate a hash of the passed string for use by the hash table.

   This function has been taken from http://www.cse.yorku.ca/~oz/hash.html and has been slightly modified. It
   is the DJB2 hash function.

   @param str The string to calculate the hash of.
   @returns the hash of the passed string.
*/
inline size_t hashtable_hash_func(const char *str) {
	size_t hash = 5381;
	int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	return hash;
}

extern inline size_t hashtable_hash_func(const char *str);

#else /* UNIT_UNDER_TEST not defined */

htbl_entry_t *debug_htbl_find_entry(const htbl_t *table, htbl_key_t key, size_t *entry_index,
                                    const enum HashtableFindOptions opts);

size_t debug_calc_index_from_key(htbl_key_t key, const size_t size);

#endif /* UNIT_UNDER_TEST defined */

/**
   Calculate the index inside of the hash table array from the passed hash (or index).

   @param table The hash table to calculate the array index of.
   @param hash_or_index Typically a hash of the key of an entry, but may also be an index.
   @returns the equivalent array index of the table for the given hash.
*/
#define HASHTABLE_CALC_INDEX(table, hash_or_index) (hash_or_index) % ((table)->size)

/**
   Calculate the load factor of the given table.

   @param table The table to calculate the load factor of. See the note in @see htbl_calc_index().
   @returns A float in the range <0.f, 1.f) representing the current load factor.
*/
#define HASHTABLE_LOAD_FACTOR(table) (!(table)->size ? 0.0f : ((float)(table)->count) / (table)->size)

#endif /* hashtable_impl.h included */
