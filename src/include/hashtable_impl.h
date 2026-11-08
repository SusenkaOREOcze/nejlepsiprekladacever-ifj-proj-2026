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
	HTBL_FIND_VALID = 0, //!< Find only entries with matching keys.
	HTBL_FIND_INVALID,   //!< Find matching keys OR an invalid entry (whichever comes first).
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

#endif /* UNIT_UNDER_TEST defined */

/**
   Find an entry with the given key in the given hash table.

   @param table The table to search through.
   @param key The key to look for.
   @param entry_index A pointer to a variable in which to store the index of the found entry. May be NULL.

   @note If the given table is full and has no free (NULL) slot for a new entry, this parameter _will be
   set to the size of the table_ (i.e., an invalid index). If the table is NULL, this value is set to
   ULONG_MAX.

   @param opts Options that adjust which entries may be retrieved (@see struct HashtableFindOptions).
   @returns A pointer to the found entry, or NULL if the entry does not exist or if an error occurs.
*/
htbl_entry_t *hashtable_find_entry(const htbl_t *table, htbl_key_t key, size_t *entry_index,
                                   const enum HashtableFindOptions opts);

/**
   Resize the table to a bigger or smaller size.

   @param table A pointer to the table to change the size of.
   @param new_size The new size of the table, in number of rows.
   @returns HTBLSTAT_OK if the resize was successfull, HTBLSTAT_BAD_ARG with wrong arguments or
   HTBLSTAT_BAD_ALLOC if allocation of the table failed.
*/
htbl_stat_t hashtable_resize(htbl_t **table, const size_t new_size);

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
