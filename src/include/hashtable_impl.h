/**
   @file hashtable_impl.h

   @author xbosakd00 (DasBean125)

   @date 2026-10-03

   @brief The private header file for the hashtable.c module.

   @details This file contains the internal definition of the hash table types as well as function
   declarations and other auxiliary constructs. It is to be included only within the hashtable.c module.
*/

#ifndef _HTBL_IMPL_H
#define _HTBL_IMPL_H

#include "hashtable.h"
#include <cstddef>
#include <stddef.h>


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
       A byte array that stores the actual data at this particular key. This is a flexible array member.
    */
    char data[];
} htbl_entry_t;

/**
   A hash table with implicit entry chaining.

   Functions like a normal hash table, where every entry key is hashed, then a modulo of this number and the
   table size is taken, which results in the index of the given entry. When hash collisions occur (also called
   a "synonymous entry"), the next closest free (NULL) index in the array is used to store the synonym (this
   is the "implicit entry chaining" part).
*/
typedef struct Hashtable {
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
} htbl_t;

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
#define _DEF_HASHTABLE(datatype)					\
    union {								\
        htbl_t *table;							\
        datatype *payload;						\
    }

/**
   Calculate a hash of the passed string for use by the hash table.

   This function has been taken from http://www.cse.yorku.ca/~oz/hash.html and has been slightly modified. It
   is the DJB2 hash function.

   @param str The string to calculate the hash of.

   @returns the hash of the passed string.
*/
static inline size_t hashtable_hash_func(const char *str) {
    size_t hash = 5381;
    int c;

    while ((c = *str++))
	hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

/**
   Calculate the index inside of the hash table array from the passed hash (or index).

   @note Any function-like macro that has a parameter "hash_table" requires that variable to be of a type
   defined by _DEF_HASHTABLE (i.e., a _union_). On the contrary, any function-like macro that has a parameter
   "table" requires that variable to be of type htbl_t!

   @param table The hash table to calculate the array index of.

   @param hash_or_index Typically a hash of the key of an entry, but may also be a number representing the index
   inside the table. This is useful for offsets within the array.

   @returns the equivalent array index of the table for the given hash.
*/
#define hashtable_calc_index(table, hash_or_index)	\
    (hashOrIndex) % ((table)->size)

/**
   Find an entry with the given key in the given hash table.

   @param table The table to search through.

   @param key The key to look for.

   @param entry_index A pointer to a variable in which to store the index of the found entry. May be NULL.

   @param opts Options that adjust which entries may be retrieved (@see struct HashtableFindOptions).

   @returns A pointer to the found entry, or NULL if the entry does not exist or if an error occurs.
*/
static htbl_entry_t *hashtable_find_entry(const htbl_t *table, htbl_key_t key, size_t *entry_index,
                                     const enum HashtableFindOptions opts);

/**
   Calculate the load factor (the ratio between the number of entries and the table size) of the given table.

   @param table The table to calculate the load factor of. See the note in @see htbl_calc_index().

   @returns A _float_ in the range <0.f, 1.f) representing the current load factor.
*/
#define hashtable_load_factor(table)					\
    (!(table)->size ? 0.0f : ((float)(table)->count) / (table)->size)

/**
   The load factor which will cause the next entry inserted to inflate the size of the table.
*/
static constexpr float HTBL_INFLATE_LOAD_FACTOR = 0.64f;

/**
   Create an empty hash table with a non-zero initial size.

   @param table A pointer to the _variable_ that holds the hash table (as it will be written indirectly).

   @param init_size The initial capacity of the hash table. Must be greater than 0.

   @returns @see htbl_stat_t.
*/
htbl_stat_t hashtable_init(htbl_t **table, const size_t init_size);

/**
   Wrapper macro to hide the implementation of the hashtable_init function. See @see htbl_init().
*/
#define _htbl_init(hash_table, init_size)	\
    hashtable_init(&(hashtbl).table, (size))

/**
   Free the given table from memory.

   @param table A pointer to the variable holding the table that will be freed.

   @return @see htbl_stat_t.
*/
htbl_stat_t hashtable_free(htbl_t **table);

/**
   Wrapper macro to hide the implementation of the hashtable_free function. See @see htbl_init().
*/
#define _htbl_free(hash_table)			\
    hashtable_free(&(hash_table).table)

/**
   Remove every entry from the table, leaving the table array still in memory.

   @param table The table to clear.

   @returns @see htbl_stat_t.
*/
htbl_stat_t hashtable_clear(htbl_t *table);

/**
   Wrapper macro to hide the implementation of the hashtable_clear function.

   I think you get it now.
*/
#define _htbl_clear(hash_table)			\
    hashtable_clear((hash_table).table)



/**
   Insert an entry into the table at the given key with the given data.

   This function allows the data argument to be of any type. Make sure you set the data_size properly (or
   don't; disrespect the operating system, see what happens).

   @param table The table to insert data into. This is a pointer to the variable that stores the table,
   because this function can _resize_ the table under certain conditions.

   @param key The key of the newly inserted entry.

   @param data A pointer to the data to be added to the entry.

   @param data_size How many bytes the data takes up in memory.

   @returns @see htbl_stat_t.
*/
htbl_stat_t hashtable_insert(htbl_t **table, htbl_key_t key, void *data, const size_t data_size);

/**
   A wrapper macro to hide the hashtable_insert function.
*/
#define _htbl_insert(hash_table, key, data)				\
    hashtable_insert(&(hash_table).table, (key),			\
		     (1 ? &(data) : (hash_table).payload), sizeof(*(hash_table).payload))

/**
   Delete an entry from the hash table.

   This function is special in that the entry is not actually deallocated from memory, only its key
   is. Afterwards the key has the empty string "\0" assigned to it. Any key in such form has the semantics of
   _being invalid_ (also known as being "plugged"), i.e. its data should not be accessed (note that the data
   is still very much there, as well as the hash).

   @param table The hash table to delete the entry from.

   @param key The key of the to-be-deleted entry.

   @returns @see htbl_stat_t.
*/
htbl_stat_t hashtable_delete(htbl_t *table, htbl_key_t key);

/**
   Wrapper macro to hide the hashtable_delete function.
*/
#define _htbl_delete(hash_table, key)		\
    hashtable_delete((hash_table).table, (key))

/**
   Retrieve an entry at a given key from the hash table.

   @param table The table to retrieve an entry from.

   @param key The key of the sought-after entry.

   @returns A pointer to the .data field of the entry struct (@see htbl_entry_t), or NULL if the entry is not
   found.
*/
char *hashtable_retrieve(htbl_t *table, htbl_key_t key);

/**
   An actual implementation of the htbl_retrieve macro.

   This thing is slightly scary, but I will attempt to explain it now, so prepare your brain cells:

   The parentheses use the comma operator to evaluate an expression (specifically the ternary operator,
   covered in the next paragraph), then throw its value away and evaluate a second expression, the
   htbl_exists(...) function, whose value is ultimately returned by the macro.

   The inner first expression, the ternary operator, is used such that its 'true' path is evaluated when the
   key is found; when this happens, the var_name identificator is written to. Otherwise the other path is
   evaluated, which simply returns 0 cast to the type of the payload (_this is why that union was used!_). The
   benefit of this? When var_name is a variable of a different type, the compiler will sound a warning of
   mismatched types inside the ternary operator.

   If you don't understand the semantics of the comma or ternary operator, look at cpp-reference or something.
*/
#define _htbl_retrieve(hash_table, key, var_name)			\
    ( hashtable_exists((hashtbl).table, (key)) ?			\
      (var) = *(__typeof__((hashtbl).payload)) hashtable_retrieve((hashtbl).table, (key)) : \
      (__typeof__(*(hashtbl).payload))0 ),				\
    hashtable_exists((hashtbl).table, (key)) )

/**
   A predicate stating whether an item with a given key exists inside the hash table.

   @param table A table to search through.

   @param key The key of the entry to find.

   @returns true if an entry with the key exists, or false if not.
*/
inline bool hashtable_exists(const htbl_t *table, htbl_key_t key);

/**
   A wrapper for the hashtable_exists function.
*/
#define _htbl_exists(hash_table, key)		\
    hashtable_exists((hash_table).table, (key))




/* 'extern' declarations of all inline functions. This is required in case the compiler decides to not inline
 * the aforementioned functions. */
extern size_t hashtable_hash_func(const char *str);
extern bool hashtable_exists(const htbl_t *table, htbl_key_t key);


#endif /* hashtable_impl.h included */
