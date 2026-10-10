/**
   @file hashtable_impl.h

   @author xbosakd00 (DasBean125)

   @date 2026-10-03

   @brief The private header file for the hashtable.c module.

   The definitions of all the important stuff that make this data type tick.
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

   Behaves like a normal hash table where every entry key is hashed, then a modulo of this number and the
   table size is taken, which computes the index of the given entry. When hash collisions occur (also called a
   "synonymous entry"), the next closest free (NULL) index in the array is used to store the synonym (this is
   the "implicit entry chaining" part).
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

/**
   Define a new hash table designated to store a given type.

   @param datatype An identifier of (possibly) any complete data type (int, char, struct ...).

   @note The hash table data type that the user works with is a union, which the user does not know. This is
   why every function that the user has access to is actually a function-like macro.

   Why is the union here in the first place? The payload member is a "dummy" member, in that it will never be
   used to access any data; it is merely there to store type information. This permits the use of the ternary
   operator to issue warnings when, for example, inserting the wrong type into a table (@see _htbl_insert()).

   This very cool trick is from the following website:
   https://danielchasehooper.com/posts/typechecked-generic-c-data-structures/#fnref:3. Give it a read!
*/
// TODO: delete #define _DEF_HASHTABLE(datatype), but move the comment

#ifndef HTBL_TEST

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

#endif /* HTBL_TEST not defined */

/**
   Calculate the index inside of the hash table array from the passed hash (or index).

   @note Any function-like macro that has a parameter "hash_table" requires that variable to be of a type
   defined by _DEF_HASHTABLE (i.e., a _union_). On the contrary, any function-like macro that has a parameter
   "table" requires that variable to be of type htbl_t!

   @param table The hash table to calculate the array index of.

   @param hash_or_index Typically a hash of the key of an entry, but may also be a number (because of modulo
   arithmetic) representing the index inside the table. This is useful for offsets within the array.

   @returns the equivalent array index of the table for the given hash.
*/
#define hashtable_calc_index(table, hash_or_index) (hash_or_index) % ((table)->size)

/**
   Calculate the load factor (the ratio between the number of entries and the table size) of the given table.

   @param table The table to calculate the load factor of. See the note in @see htbl_calc_index().

   @returns A _float_ in the range <0.f, 1.f) representing the current load factor.
*/
#define hashtable_load_factor(table) (!(table)->size ? 0.0f : ((float)(table)->count) / (table)->size)

#endif /* hashtable_impl.h included */
