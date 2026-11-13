/**
   @file hashtable.c

   @author xbosakd00 (DasBean125)

   @date 2026-09-28

   @brief The implementation of the hash table data structure.
*/

#ifdef UNIT_UNDER_TEST

#undef UNIT_UNDER_TEST
#include "include/hashtable_impl.h"
#define UNIT_UNDER_TEST

#else /* UNIT_UNDER_TEST defined */

#include "include/hashtable_impl.h"

#endif /* UNIT_UNDER_TEST not defined */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*=========================
  INTERNAL FUNCTIONS & DATA
  =========================*/

/**
   The load factor which will cause the next entry inserted to inflate the size of the table.
*/
static constexpr float HTBL_INFLATE_LOAD_FACTOR = 0.64f;

htbl_entry_t *hashtable_find_entry(const htbl_t *table, htbl_key_t key, size_t *entry_index,
                                   const enum HashtableFindOptions opts) {
	if (table == NULL || key == NULL) {
		fprintf(stderr, "hashtable_find_entry: provided table doesn't exist!\n");
		if (entry_index != NULL)
			*entry_index = ULONG_MAX;
		return NULL;
	}
	/* Search begins at the would-be index of the key */
	size_t key_hash = hashtable_hash_func(key);
	size_t base_index = HASHTABLE_CALC_INDEX(table, key_hash);
	int offset = -1;
	htbl_entry_t *cur_entry = NULL;
	htbl_entry_t *invalid_entry = NULL;

	/* Find a free slot */
	do { /* Note that 'offset' is guaranteed to be >=0 because of the do...while loop */
		offset++;
		cur_entry = table->array[HASHTABLE_CALC_INDEX(table, base_index + offset)];

		/* If we want invalid entries too, consider them as NULL (and therefor free to use) */
		if (opts == HTBL_FIND_INVALID && invalid_entry == NULL && cur_entry != NULL &&
		    cur_entry->key[0] == '\0') {
			invalid_entry = cur_entry;
			break;
		}
	} while (cur_entry != NULL && strcmp(cur_entry->key, key) != 0 && offset < (int)table->size);

	/* Sound an error if the loop concluded as a result of endless iteration */
	if (invalid_entry == NULL && offset >= (int)table->size) {
	    fprintf(stderr, "hashtable_find_entry: table has no free space left\n");
	    if (entry_index != NULL)
		*entry_index = table->size;
	    return NULL;
	}

	/* Give the caller the index when required */
	if (entry_index != NULL)
		*entry_index = (base_index + offset) % table->size;

	/* invalid_entry guaranteed to be NULL if opts != HTBL_FIND_INVALID */
	return cur_entry ?: invalid_entry;
}

htbl_stat_t hashtable_resize(htbl_t **table, const size_t new_size) {
	if (table == NULL || *table == NULL || new_size == 0)
		return HTBLSTAT_BAD_ARG;
	if (new_size == (*table)->size || new_size < ((*table)->count + 1))
		return HTBLSTAT_BAD_ARG;

	/* Cannot use realloc, because the entries must have their index recalculated */
	htbl_t *new_table = malloc(sizeof(*new_table) + new_size * sizeof(htbl_entry_t *));
	if (new_table == NULL)
		return HTBLSTAT_BAD_ALLOC;
	for (size_t idx = 0; idx < new_size; idx++) /* Random garbage in memory not allowed */
		new_table->array[idx] = NULL;

	/* Copy data from the old table over to the new one */
	new_table->size = new_size;
	new_table->count = (*table)->count;
	for (size_t idx = 0; idx < (*table)->size; idx++) {
		htbl_entry_t *old_entry = (*table)->array[idx];
		if (old_entry == NULL)
			continue;

		/* Invalid entries are removed from the table (increases efficiency) */
		if (old_entry->key[0] == '\0') {
			free(old_entry);
			continue;
		}

		/* Valid entries are inserted the same way as the function hashtable_insert() */
		size_t new_index = HASHTABLE_CALC_INDEX(new_table, old_entry->hash);
		while (new_table->array[HASHTABLE_CALC_INDEX(new_table, new_index)] != NULL)
			new_index++;
		new_table->array[HASHTABLE_CALC_INDEX(new_table, new_index)] = old_entry;
	}

	/* Free _just the old table itself_ */
	free(*table);
	*table = new_table;
	return HTBLSTAT_OK;
}

/*==============
  ADT OPERATIONS
  ==============*/

/**
   Create an empty hash table with a non-zero initial size.

   @param table A pointer to the _variable_ that holds the hash table (as it will be written indirectly).
   @param init_size The initial capacity of the hash table. Must be greater than 0.
   @returns @see htbl_stat_t.
*/
htbl_stat_t hashtable_init(htbl_t **table, const size_t init_size) {
	if (!init_size || table == NULL)
		return HTBLSTAT_BAD_ARG;

	/* Allocate the table */
	htbl_t *new_table = malloc(sizeof(*new_table) + init_size * sizeof(htbl_entry_t *));
	if (new_table == NULL)
		return HTBLSTAT_BAD_ALLOC;

	new_table->size = init_size;
	new_table->count = 0;
	for (size_t i = 0; i < new_table->size; i++)
		new_table->array[i] = NULL; /* Initialize the memory space */

	/* Write the table into the provided variable */
	*table = new_table;
	return HTBLSTAT_OK;
}

/**
   Free the given table from memory.

   @param table A pointer to the variable holding the table that will be freed.
   @return @see htbl_stat_t.
*/
htbl_stat_t hashtable_free(htbl_t **table) {
	if (table == NULL || *table == NULL)
		return HTBLSTAT_BAD_ARG;

	hashtable_clear(*table);

	free(*table);
	*table = NULL;
	return HTBLSTAT_OK;
}

/**
   Remove every entry from the table, leaving the table array still in memory.

   @param table The table to clear.
   @returns @see htbl_stat_t.
*/
htbl_stat_t hashtable_clear(htbl_t *table) {
	if (table == NULL)
		return HTBLSTAT_BAD_ARG;

	for (size_t i = 0; i < table->size; i++) {
		htbl_entry_t *entry = table->array[i];
		if (entry == NULL)
			continue;

		/* hashtable_delete sets keys to a static string -> make sure this hasn't happened
		   I thank our benefactors (Lukáš) for finding this bug */
		if (entry->key[0] != '\0')
			free((char *)entry->key);
		free(entry);
		table->array[i] = NULL;
	}

	table->count = 0;
	return HTBLSTAT_OK;
}

/**
   Insert an entry into the table at the given key with the given data.

   @param table A pointer to the table to insert data into.
   @param key The key of the newly inserted entry.
   @param data A pointer to the data to be added to the entry.
   @param data_size How many bytes the data takes up in memory.
   @returns @see htbl_stat_t.
*/
htbl_stat_t hashtable_insert(htbl_t **table, htbl_key_t key, void *data, const size_t data_size) {
	if (table == NULL || *table == NULL || data == NULL || !data_size || key == NULL || key[0] == '\0')
		return HTBLSTAT_BAD_ARG;

	/* Resize the table if needed */
	if (HASHTABLE_LOAD_FACTOR(*table) >= HTBL_INFLATE_LOAD_FACTOR) {
		htbl_stat_t resize_stat = hashtable_resize(table, 2 * (*table)->size);
		if (resize_stat != HTBLSTAT_OK)
			return HTBLSTAT_BAD_ALLOC;
	}

	/* See if an entry exists already */
	size_t key_hash = hashtable_hash_func(key), entry_index;
	htbl_entry_t *entry = hashtable_find_entry(*table, key, &entry_index, HTBL_FIND_INVALID);
	if (entry == NULL && entry_index == (*table)->size)
		return HTBLSTAT_INTERNAL_FAIL;

	/* Now either entry == NULL (is free) or it exists, in which case it _may_ be invalid */
	if (entry == NULL) { /* Allocate a new entry and add data to it */
		htbl_entry_t *new_entry = malloc(sizeof(htbl_entry_t) + data_size);
		if (new_entry == NULL)
			return HTBLSTAT_BAD_ALLOC;

		memcpy(new_entry->data, data, data_size);

		/* Populate the new entry with data */
		new_entry->hash = key_hash;
		const size_t key_length = strlen(key);

		htbl_key_t new_key = calloc(key_length + 1, sizeof(char));
		if (new_key == NULL) {
			free(new_entry);
			return HTBLSTAT_BAD_ALLOC;
		}
		strncpy((char *)new_key, key, key_length);

		new_entry->key = new_key;
		(*table)->array[entry_index] = new_entry;
		(*table)->count++;

	} else { /* The entry exists - update its data and potentially its key (if it is invalid) */
		memcpy(entry->data, data, data_size);

		if (entry->key[0] == '\0') {
			const size_t key_length = strlen(key);

			htbl_key_t new_key = calloc(key_length + 1, sizeof(char));
			if (new_key == NULL)
				return HTBLSTAT_BAD_ALLOC;
			strncpy((char *)new_key, key, key_length);

			entry->key = new_key; /* Note: entry->key is freed by hashtable_delete() */
			entry->hash = key_hash;
			(*table)->count++;
		}
	}

	return HTBLSTAT_OK;
}

/**
   Delete an entry from the hash table.

   @param table The hash table to delete the entry from.
   @param key The key of the to-be-deleted entry.
   @returns @see htbl_stat_t.
*/
htbl_stat_t hashtable_delete(htbl_t *table, htbl_key_t key) {
	if (table == NULL || key == NULL || key[0] == '\0')
		return HTBLSTAT_BAD_ARG;
	if (table->count == 0)
		return HTBLSTAT_OK;

	htbl_entry_t *entry = hashtable_find_entry(table, key, NULL, HTBL_FIND_VALID);
	/* Empty entry - nothing to delete */
	if (entry == NULL)
		return HTBLSTAT_OK;

	/* As mentioned in hashtable_impl.h, the entry is _not deallocated_, only its key is */
	free((char *)entry->key);
	entry->key = "\0";
	table->count--;

	return HTBLSTAT_OK;
}

/**
   Retrieve an entry at a given key from the hash table.

   @param table The table to retrieve an entry from.
   @param key The key of the sought-after entry.
   @returns A pointer to the .data field of the entry struct (@see htbl_entry_t), or NULL if the entry is not
   found or the table doesn't exist.
*/
char *hashtable_retrieve(htbl_t *table, htbl_key_t key) {
	if (table == NULL)
		return NULL;

	htbl_entry_t *entry = hashtable_find_entry(table, key, NULL, HTBL_FIND_VALID);
	return (entry != NULL ? entry->data : NULL);
}

/**
   A predicate stating whether an item with a given key exists inside the hash table.

   @param table A table to search through.
   @param key The key of the entry to find.
   @returns true if an entry with the key exists, or false if not.
*/
bool hashtable_exists(const htbl_t *table, htbl_key_t key) {
	if (table == NULL)
		return NULL;

	htbl_entry_t *entry = hashtable_find_entry(table, key, NULL, HTBL_FIND_VALID);
	return (entry != NULL);
}
