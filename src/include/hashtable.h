/**
   @file hashtable.h

   @author xbosakd00 (DasBean125)

   @date 2026-09-28

   @brief The public header file for the hashtable.c module, defining the hash table data type.

   @details This file contains declarations required for the usage of the hashtable.c module and the data
   structure it defines. It is an abstract data type; the implementation is deliberately hidden from the user
   and the full type definition is performed in the file hashtable_impl.h.
*/

#ifndef _HASHTABLE_H
#define _HASHTABLE_H

/*=====
  TYPES
  =====*/
/**
   Define a new hashtable which should hold data of type 'datatype'.

   Example usage for a hashtable that is to hold integers:
   @code
   DEFTYPE_HASHTABLE(int) my_int_hashtable_t;
   @endcode
 */
#define DEFTYPE_HASHTABLE(datatype)		\
    typedef _DEF_HASHTABLE(datatype)

/**
   A hashtable entry key.
 */
typedef const char* htbl_key_t;



/*=========
  FUNCTIONS
  =========*/
/**
   Initialize a hash table to some non-zero initial size.

   @param hash_table A variable of type defined by @see DEFTYPE_HASHTABLE().

   @param init_size An integer specifying the initial size of the table.

   @returns HTBLSTAT_OK on successful allocation, HTBLSTAT_BAD_ALLOC on bad allocation, or HTBLSTAT_BAD_ARG if
   one of the arguments doesn't make sense.
 */
#define htbl_init(hash_table, init_size) _htbl_init((hash_table), (init_size))

/**
   Free a hash table from memory.

   @param hash_table The hash table to free; the type must (again) be defined by @see DEFTYPE_HASHTABLE().

   @returns HTBLSTAT_OK on success or HTBLSTAT_BAD_ARG if the argument is invalid.
 */
#define htbl_free(hash_table) _htbl_free((hash_table))

/**
   Clears the contents of the hash table, keeping it still allocated for reuse.

   @param hash_table The table whose entries to clear. Type must be @see DEFTYPE_HASHTABLE().

   @returns HTBLSTAT_OK on success or HTBLSTAT_BAD_ARG if argument is invalid.
 */
#define htbl_clear(hash_table) _htbl_clear((hash_table))


/**
   Inserts an entry with a given key and data into the hash table.

   This function-like macro has _updating semantics_ - this means that if the given key already exists, its
   value is _overwritten_ with the new data. If you don't wish for this to happen, use @see htbl_exists().

   @param hash_table The hash table to insert an entry into. @see DEFTYPE_HASHTABLE().

   @param key A string representing the key of the entry. Must contain at least one character. The argument
   may be a string literal.

   @param data The data to add into the inserted entry. _Must be an l-value_ (an addressable argument) of the
   same type as the value type of the hash table (@see DEFTYPE_HASHTABLE()).

   @returns HTBLSTAT_OK on success, HTBLSTAT_BAD_ARG if one or more arguments make no sense, or HTBL_BAD_ALLOC
   if allocation fails.
 */
#define htbl_insert(hash_table, key, data) _htbl_insert((hash_table), (key), (data))

/**
   Deletes an entry with the given key from the hash table.

   @param hash_table The hash table to delete data from (@see DEFTYPE_HASHTABLE()).

   @param key The key of the entry to delete. Same restrictions as @see htbl_insert() apply.

   @returns HTBLSTAT_OK on success _or when such a key doesn't exist_, or HTBLSTAT_BAD_ARG if one or more
   arguments make no sense.

   @note The return code when no deletion takes place because of a missing key may change in the future.
 */
#define htbl_delete(hash_table, key) _htbl_delete((hash_table), (key))

/**
   Retrieves an item with a given key from the hash table and puts its data into a given variable.

   @param hash_table The hash table to retrieve data from (@see DEFTYPE_HASHTABLE()).

   @param key The key of the entry to retrieve data from. Same restrictions as @see htbl_insert() apply.

   @param var_name The name of the variable into which to insert data. _Must be a modifiable l-value_.

   @returns HTBLSTAT_OK if the key exists and has been retrieved, or HTBLSTAT_INVALID_KEY if the key wasn't
   found.
 */
#define htbl_retrieve(hash_table, key, var_name) _htbl_retrieve((hash_table), (key), (var_name))

/**
   A predicate that states if the given key exists in the hash table.

   @param hash_table The hash table to search through (@see DEFTYPE_HASHTABLE()).

   @param key The key of the entry to look for. Same restrictions as @see htbl_insert() apply.
 */
#define htbl_exists(hash_table, key) _htbl_exists((hash_table), (key))


#endif /* hashtable.h included */
