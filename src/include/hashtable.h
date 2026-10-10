/**
   @file hashtable.h

   @author xbosakd00 (DasBean125)

   @date 2026-09-28

   @brief The public header file for the hashtable.c module; the file a user of this structure must include.

   Ever seen a hash table? It's a data structure that uses hashes to calculate an index of its element which
   is identified by a key... why'd you leave the keys upon the table? Here you go, create another fable
   (you wanted to)-
*/

#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stddef.h>

/*=====
  TYPES
  =====*/

/**
   A hashtable entry key. No, it won't unlock any locks.
 */
typedef const char *htbl_key_t;

/**
   A generic hash table type. You won't need to use this anywhere, but it's nice to know about.
*/
typedef struct Hashtable htbl_t;

/**
   A set of codes that are returned by most of the hash table operations (see their respective returns).
*/
typedef enum HashtableStatusCode {
	HTBLSTAT_OK = 0,
	HTBLSTAT_BAD_ARG,
	HTBLSTAT_BAD_ALLOC,
	HTBLSTAT_INTERNAL_FAIL
} htbl_stat_t;

/*====================
  DATA TYPE OPERATIONS
  ====================*/

/**
   Define a new hashtable which should hold data of type 'datatype'.

   Example usage for a hashtable that is to hold integers:
   @code
   DEFTYPE_HASHTABLE(int) my_int_hashtable_t;
   // ...
   my_int_hashtable_t my_table; // a beautiful table that will store bountiful data of your choosing
                                // (in this case, integers, but it could be automobiles)
   @endcode

   Pay no mind to the union; you are obligated to do this because by using this header file, you have
   implicitly signed a non-disclosement agreement which legally forbids you from explaining the internal
   structure to yourself.
 */

#define DEFTYPE_HASHTABLE(datatype)                                                                          \
	typedef union {                                                                                          \
		htbl_t *table;                                                                                       \
		datatype *payload;                                                                                   \
	}

/**
   Initialize a hash table to some non-zero initial size.

   @param hash_table A variable of type defined by @see DEFTYPE_HASHTABLE().

   @param init_size An integer specifying the initial size of the table.

   @returns HTBLSTAT_OK on successful allocation, HTBLSTAT_BAD_ALLOC on bad allocation, or HTBLSTAT_BAD_ARG if
   one of the arguments doesn't make sense.
 */
#define htbl_init(hash_table, init_size) hashtable_init(&(hash_table).table, (init_size))

/**
   Free a hash table from memory (and from life as well).

   @param hash_table The hash table to free; the type must (again) be defined by @see DEFTYPE_HASHTABLE().

   @returns HTBLSTAT_OK on success or HTBLSTAT_BAD_ARG if the argument is invalid.

   @note The parameter is set to NULL if this function succeeds.
 */
#define htbl_free(hash_table) hashtable_free(&(hash_table).table)

/**
   Clear the contents of the hash table, keeping it still allocated for reuse.

   This time the table survives this horrifying event; the same could not be said for any of the
   elements. Their electrons will be forever remembered by the memory frame(s) that stored them.

   @param hash_table The table whose entries to clear. Type must be @see DEFTYPE_HASHTABLE().

   @returns HTBLSTAT_OK on success or HTBLSTAT_BAD_ARG if argument is invalid.
 */
#define htbl_clear(hash_table) hashtable_clear((hash_table).table)

/**
   Insert an entry with a given key and data into the hash table.

   This function-like macro has _updating semantics_ - this means that if the given key already exists, its
   value is _overwritten_ with the new data. If you don't wish for this to happen, use your third eye and
   ginormous brain to imagine every possible scenario in which the given key already exists... or just use
   @see htbl_exists().

   @param hash_table The hash table to insert an entry into. @see DEFTYPE_HASHTABLE().

   @param key A string representing the key of the entry. Must contain at least one character. The argument
   may be a string literal.

   @param data The data to add into the inserted entry. _Must be an l-value_ (an addressable argument) of the
   same type as the value type of the hash table (@see DEFTYPE_HASHTABLE()).

   @returns HTBLSTAT_OK on success, HTBLSTAT_BAD_ARG if one or more arguments make no sense, or HTBL_BAD_ALLOC
   if allocation fails.
 */
#define htbl_insert(hash_table, key, data)                                                                   \
	hashtable_insert(&(hash_table).table, (key), (1 ? &(data) : (hash_table).payload),                       \
	                 sizeof(*(hash_table).payload))

/**
   Delete an entry with the given key from the hash table.

   This function will make you live a lie, but you're certainly already used to that, so why bother digging?
   What's that? You're _curious_? Such preposterous thoughts in today's day and age!

   @param hash_table The hash table to delete data from (@see DEFTYPE_HASHTABLE()).

   @param key The key of the entry to delete. Same restrictions as @see htbl_insert() apply.

   @returns HTBLSTAT_OK on success _or when such a key doesn't exist_, or HTBLSTAT_BAD_ARG if one or more
   arguments make no sense.

   @note The return code when no deletion takes place because of a missing key may change in the future.
 */
#define htbl_delete(hash_table, key) hashtable_delete((hash_table).table, (key))

/**
   Retrieve an item with a given key from the hash table and puts its data into a given variable.

   @param hash_table The hash table to retrieve data from (@see DEFTYPE_HASHTABLE()).

   @param key The key of the entry to retrieve data from. Same restrictions as @see htbl_insert() apply.

   @param var_name The name of the variable into which to insert data. _Must be a modifiable l-value_.

   @returns Non-zero if the key was found (and its value was retrieved), or zero if the key doesn't exist.

   @note This thing is slightly scary, but I will attempt to explain it now, so prepare your brain cells:
   + The parentheses use the comma operator to evaluate an expression (specifically the ternary operator,
   covered in the next paragraph), then throw its value away and evaluate a second expression, the
   htbl_exists(...) function, whose value is ultimately returned by the macro.
   + The inner first expression, the ternary operator, is used such that its 'true' path is evaluated when the
   key is found; when this happens, the var_name identificator is written to. Otherwise the other path is
   evaluated, which simply returns 0 cast to the type of the payload (_this is why that union was used!_). The
   benefit of this? When var_name is a variable of a different type, the compiler will sound a warning of
   mismatched types inside the ternary operator.
   If you don't understand the semantics of the comma or ternary operator, look at cpp-reference or something.
 */
#define htbl_retrieve(hash_table, key, var_name)                                                             \
	({                                                                                                       \
		const bool htbl_retrieve_key_exists = hashtable_exists((hash_table).table, (key));                   \
		((htbl_retrieve_key_exists ? (var_name) = *(__typeof__((hash_table).payload))hashtable_retrieve(     \
		                                 (hash_table).table, (key))                                          \
		                           : (__typeof__(*(hash_table).payload))0),                                  \
		 htbl_retrieve_key_exists);                                                                          \
	})

/**
   A predicate that states if the given key exists in the hash table.

   @param hash_table The hash table to search through (@see DEFTYPE_HASHTABLE()).

   @param key The key of the entry to look for. Same restrictions as @see htbl_insert() apply.

   @returns Non-zero if the given key exists, or zero if the key was not found.
 */
#define htbl_exists(hash_table, key) hashtable_exists((hash_table).table, (key))

/*=========
  FUNCTIONS (don't use these to interact with the data type)
  =========*/

htbl_stat_t hashtable_init(htbl_t **table, const size_t init_size);

htbl_stat_t hashtable_free(htbl_t **table);

htbl_stat_t hashtable_clear(htbl_t *table);

htbl_stat_t hashtable_insert(htbl_t **table, htbl_key_t key, void *data, const size_t data_size);

htbl_stat_t hashtable_delete(htbl_t *table, htbl_key_t key);

char *hashtable_retrieve(htbl_t *table, htbl_key_t key);

bool hashtable_exists(const htbl_t *table, htbl_key_t key);

#endif /* hashtable.h included */
