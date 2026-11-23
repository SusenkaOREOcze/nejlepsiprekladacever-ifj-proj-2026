/**
   @file test_hashtable.c
   @author xbosakd00 (DasBean125)
   @data 2026-10-06

   @brief Tests the hash table data structure. Your life may depend on it if this succeedes.

   Is this a test? It has to be, otherwise I can't go on...
*/

#ifndef UNIT_UNDER_TEST
#define UNIT_UNDER_TEST
#endif
#include "../include/hashtable.h"
#include "../include/hashtable_impl.h"

#include "cmocka/cmocka.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXEC_TEST_CATEGORY(name, tests)                                                                      \
	do {                                                                                                     \
		printf("\n[    " name "    ]\n");                                                                    \
		test_results |= (cmocka_run_group_tests_name((name), (tests), NULL, NULL));                          \
	} while (0)

DEFTYPE_HASHTABLE(unsigned long) htbl_ulong_t;

/*===============
  HTBL_INIT tests
  ===============*/
static htbl_ulong_t null_table = {.table = NULL};
static void htbl_init_returns(void **state) {
	(void)state;
	htbl_ulong_t table;

	assert_int_equal(hashtable_init(NULL, 64), HTBLSTAT_BAD_ARG);
	assert_int_equal(HTBL_INIT(table, 0), HTBLSTAT_BAD_ARG);
}

static void htbl_init_single(void **state) {
	(void)state;

	htbl_ulong_t table;
	assert_int_equal(HTBL_INIT(table, 64), HTBLSTAT_OK);

	assert_uint_equal(table.table->size, 64);
	assert_uint_equal(table.table->count, 0);
	for (size_t i = 0; i < table.table->size; i++)
		assert_null(table.table->array[i]);

	free(table.table);
}

/*===============
  HTBL_FREE tests
  ===============*/
static void htbl_free_returns(void **state) {
	(void)state;

	assert_int_equal(HTBL_FREE(null_table), HTBLSTAT_BAD_ARG);
	assert_int_equal(hashtable_free(NULL), HTBLSTAT_BAD_ARG);
}

static void htbl_free_nullifies_arg(void **state) {
	(void)state;

	htbl_ulong_t table;
	HTBL_INIT(table, 64);

	assert_int_equal(HTBL_FREE(table), HTBLSTAT_OK);
	assert_null(table.table);
}

/*================
  HTBL_CLEAR tests
  ================*/
static void htbl_clear_returns(void **state) {
	(void)state;

	assert_int_equal(HTBL_CLEAR(null_table), HTBLSTAT_BAD_ARG);
	assert_int_equal(hashtable_clear(NULL), HTBLSTAT_BAD_ARG);
}

// TODO: add clear tests after insertion is tested

/*=================
  HTBL_INSERT tests
  =================*/
static void htbl_insert_returns(void **state) {
	(void)state;
	unsigned long nuhuh = 0;
	htbl_ulong_t table;

	assert_int_equal(HTBL_INSERT(null_table, "Glimpse of light", &nuhuh), HTBLSTAT_BAD_ARG);
	assert_int_equal(HTBL_INSERT(table, NULL, &nuhuh), HTBLSTAT_BAD_ARG);
	assert_int_equal(HTBL_INSERT(table, "", &nuhuh), HTBLSTAT_BAD_ARG);
	assert_int_equal(HTBL_INSERT(table, "comes shining through", NULL), HTBLSTAT_BAD_ARG);
}

static void htbl_insert_single(void **state) {
	(void)state;
	unsigned long data = 5;
	htbl_ulong_t table;

	assert_int_equal(HTBL_INIT(table, 64), HTBLSTAT_OK);

	assert_int_equal(HTBL_INSERT(table, "this waking dream", &data), HTBLSTAT_OK);
	assert_uint_equal(table.table->count, 1);
	assert_uint_equal(table.table->size, 64); /* better not be resizing right now */

	bool found = false;
	size_t index = 0; /* for cleanup */
	for (size_t i = 0; i < table.table->size && !found; i++) {
		if (table.table->array[i] != NULL) {
			found = true;
			index = i;
		}
	}
	assert_true(found);
	assert_uint_equal((unsigned long)(*table.table->array[index]->data), 5);
	assert_string_equal(table.table->array[index]->key, "this waking dream");

	free((char *)table.table->array[index]->key);
	free(table.table->array[index]);
	free(table.table);
}

static void htbl_insert_4_non_synonyms(void **state) {
	(void)state;
	htbl_ulong_t table;

	HTBL_INIT(table, 64);

	static char *keys[] = {"chaos is", "the grain", "on which", "those bastards feed"};
	static unsigned long data[] = {4, 6, 8, 10};
	const size_t key_count = sizeof(keys) / sizeof(*keys);

	for (size_t i = 0; i < key_count; i++) {
		assert_int_equal(HTBL_INSERT(table, keys[i], &data[i]), HTBLSTAT_OK);
	}
	assert_uint_equal(table.table->size, 64);
	assert_uint_equal(table.table->count, 4);

	int found_count = 0;
	for (size_t i = 0; i < table.table->size; i++) {
		htbl_entry_t *entry = table.table->array[i];
		if (entry == NULL)
			continue;

		for (size_t j = 0; j < key_count; j++) {
			if (strcmp(entry->key, keys[j]) == 0) {
				assert_uint_equal((unsigned long)(*entry->data), data[j]);
				found_count++;
			}
		}
	}
	assert_int_equal(found_count, 4);

	HTBL_FREE(table);
}

static void htbl_insert_4_synonyms_sequential(void **state) {
	(void)state;
	htbl_ulong_t table;
	HTBL_INIT(table, 64);

	/* this (probably) guarantees that the keys will immediately follow each other in the array */
	static char *keys[] = {"a", "b", "c", "d"};
	static unsigned long data[] = {1, 1, 2, 3};
	const size_t key_count = sizeof(keys) / sizeof(*keys);

	for (size_t i = 0; i < key_count; i++) {
		assert_int_equal(HTBL_INSERT(table, keys[i], &data[i]), HTBLSTAT_OK);
	}
	assert_uint_equal(table.table->size, 64);
	assert_uint_equal(table.table->count, 4);

	int found_count = 0;
	int last_index = -1;
	for (size_t i = 0; i < table.table->size; i++) {
		htbl_entry_t *entry = table.table->array[i];
		if (entry == NULL)
			continue;
		if (last_index == -1)
			last_index = i - 1;

		for (size_t j = 0; j < key_count; j++) {
			if (strcmp(entry->key, keys[j]) == 0) {
				assert_uint_equal((unsigned long)(*entry->data), data[j]);
				assert_int_equal(last_index + 1, (int)i); /* last + 1 must be current */
				found_count++;
			}
		}
		last_index = i;
	}
	assert_int_equal(found_count, 4);

	HTBL_FREE(table);
}

/*=================
  HTBL_DELETE tests
  =================*/
static void htbl_delete_returns(void **state) {
	(void)state;
	htbl_ulong_t table;
	HTBL_INIT(table, 74);

	assert_int_equal(HTBL_DELETE(null_table, "hello"), HTBLSTAT_BAD_ARG);
	assert_int_equal(hashtable_delete(NULL, "nope"), HTBLSTAT_BAD_ARG);
	assert_int_equal(HTBL_DELETE(table, ""), HTBLSTAT_BAD_ARG);

	HTBL_FREE(table);
}

static void htbl_delete_single(void **state) {
	(void)state;
	htbl_ulong_t table;
	HTBL_INIT(table, 69);

	unsigned long data = 4;
	HTBL_INSERT(table, "la bomba", &data);
	size_t index = 0;
	for (size_t i = 0; i < table.table->size; i++) {
		if (table.table->array[i] != NULL) {
			index = i;
			break;
		}
	}

	assert_int_equal(HTBL_DELETE(table, "la bomba"), HTBLSTAT_OK);
	htbl_entry_t *exploded_entry = table.table->array[index];
	assert_true(exploded_entry->key[0] == '\0');
	assert_uint_equal((unsigned long)(*exploded_entry->data), data); /* make sure entry still exists */

	assert_int_equal(HTBL_DELETE(table, "la bomba"), HTBLSTAT_OK); /* kinda cringe, idk */

	HTBL_FREE(table);
}

/*=================
  HTBL_EXISTS tests
  =================*/
static void htbl_exists_returns(void **state) {
	(void)state;
	htbl_ulong_t table;

	HTBL_INIT(table, 88);

	assert_false(HTBL_EXISTS(null_table, "nope"));
	assert_false(HTBL_EXISTS(table, ""));

	HTBL_FREE(table);
}

static void htbl_exists_single(void **state) {
	(void)state;
	htbl_ulong_t table;

	HTBL_INIT(table, 62);
	unsigned long data = 62;
	HTBL_INSERT(table, "I do, in fact, exist", &data);

	assert_true(HTBL_EXISTS(table, "I do, in fact, exist"));

	HTBL_FREE(table);
}

/*===================
  HTBL_RETRIEVE tests
  ===================*/
static void htbl_retrieve_returns(void **state) {
	(void)state;
	unsigned long nuhuh = 0;
	htbl_ulong_t table;
	HTBL_INIT(table, 35);

	assert_false(HTBL_RETRIEVE(null_table, "hi", nuhuh));
	assert_false(HTBL_RETRIEVE(table, "", nuhuh));

	HTBL_FREE(table);
}

static void hashtable_find_entry_returns(void **state) {
	(void)state;
	size_t entry_index;

	assert_null(hashtable_find_entry(NULL, "nope", NULL, HTBL_FIND_VALID));
	assert_null(hashtable_find_entry(NULL, "nope", NULL, HTBL_FIND_INVALID));
	assert_null(hashtable_find_entry(NULL, "nope", &entry_index, HTBL_FIND_VALID));
	assert_uint_equal(entry_index, ULONG_MAX);
	entry_index = 0;
	assert_null(hashtable_find_entry(NULL, "nope", &entry_index, HTBL_FIND_INVALID));
	assert_uint_equal(entry_index, ULONG_MAX);

	htbl_ulong_t table;
	HTBL_INIT(table, 33);

	assert_null(hashtable_find_entry(table.table, NULL, NULL, HTBL_FIND_INVALID));
	assert_null(hashtable_find_entry(table.table, NULL, NULL, HTBL_FIND_VALID));

	HTBL_FREE(table);
}

static constexpr size_t KEY_COUNT = 16;
static const char *test_keys[KEY_COUNT] = {
    "The",     "Fitness",  "Gram", "pacer", "test",          "is",   "a",    "multi-stage",
    "aerobic", "capacity", "test", "that",  "progressively", "gets", "more", "difficult",
};
static size_t test_indeces[KEY_COUNT] = {
    0,
};
static htbl_ulong_t test_table;

static void hashtable_find_entry_several_valid(void **state) {
	(void)state;
	HTBL_INIT(test_table, 32); /* beware: this gets freed in hashtable_find_entry_several_invalid() */

	for (size_t i = 0; i < KEY_COUNT; i++) {
		HTBL_INSERT(test_table, test_keys[i], &i);
	}

	/* finding indeces of all the keys the hard way */
	for (size_t key_idx = 0; key_idx < KEY_COUNT; key_idx++) {
		htbl_key_t key = test_keys[key_idx];

		for (size_t table_idx = 0; table_idx < test_table.table->size; table_idx++) {
			htbl_entry_t *entry = test_table.table->array[table_idx];
			if (entry == NULL || strcmp(entry->key, key) != 0)
				continue;
			test_indeces[key_idx] = table_idx;
		}
	}

	/* making sure hashtable_find_entry returns the same thing that we found earlier */
	for (size_t i = 0; i < KEY_COUNT; i++) {
		size_t find_func_index;
		size_t find_invalid_index;

		assert_non_null(
		    hashtable_find_entry(test_table.table, test_keys[i], &find_func_index, HTBL_FIND_VALID));
		assert_uint_equal(find_func_index, test_indeces[i]);
		assert_non_null(
		    hashtable_find_entry(test_table.table, test_keys[i], &find_invalid_index, HTBL_FIND_INVALID));
		assert_uint_equal(find_invalid_index, test_indeces[i]);
	}
}

static void hashtable_find_entry_several_invalid(void **state) {
	(void)state;
	HTBL_DELETE(test_table, test_keys[3]);
	HTBL_DELETE(test_table, test_keys[14]);

	for (size_t i = 0; i < KEY_COUNT; i++) {
		size_t find_func_index;
		htbl_entry_t *entry =
		    hashtable_find_entry(test_table.table, test_keys[i], &find_func_index, HTBL_FIND_VALID);

		/* If a valid entry was returned, indeces must still match */
		if (entry)
			assert_uint_equal(find_func_index, test_indeces[i]);
	}

	HTBL_FREE(test_table);
}

/*========================
  Advanced hashtable tests
  ========================*/
static void htbl_adv_auto_resize(void **state) {
	(void)state;

	htbl_ulong_t table;
	const size_t init_size = 6;
	assert_int_equal(HTBL_INIT(table, init_size), HTBLSTAT_OK);

	const char *keys[] = {"Resistance", "Reactance", "Impedance", "Conductance", "Susceptance", "Admittance"};
	const size_t key_count = sizeof(keys) / sizeof(char *);
	const size_t resize_count = (const size_t)(0.64f * init_size) + 1; /* This factor is statically defined,
	                                                                    * so it must be manually copied
	                                                                    * here */
	for (size_t i = 0; i < key_count; i++) {
		assert_int_equal(HTBL_INSERT(table, keys[i], &i), HTBLSTAT_OK);

		if (i == (resize_count - 1))
			assert_int_equal(table.table->size, init_size);
		if (i == resize_count)
			assert_int_equal(table.table->size, init_size * 2);
	}

	for (size_t i = 0; i < key_count; i++) {
		unsigned long data = 0;
		assert_true(HTBL_RETRIEVE(table, keys[i], data));
		assert_uint_equal(data, i);
	}

	HTBL_FREE(table);
}

static void htbl_adv_resize_tombstone_optimization(void **state) {
	(void)state;

	htbl_ulong_t table;
	HTBL_INIT(table, 4);

	unsigned long data = 69;
	HTBL_INSERT(table, "0", &data);
	/* Create a tombstone without resizing */
	data = 125;
	HTBL_INSERT(table, "1", &data);
	HTBL_DELETE(table, "1");

	/* Add items until a resize is triggered but without overwriting the tombstone */
	data = 400;
	HTBL_INSERT(table, "2", &data);
	HTBL_INSERT(table, "3", &data); /* 3/4 > 0.64 -> resize operation on _next_ insert */
	HTBL_INSERT(table, "4", &data);
	assert_uint_equal(table.table->size, 8);

	/* Make sure all the invalid entries are gone */
	for (size_t i = 0; i < table.table->size; i++) {
		htbl_entry_t *entry = table.table->array[i];
		if (entry != NULL)
			assert_false(entry->key[0] == '\0');
	}

	HTBL_FREE(table);
}

static void htbl_adv_insert_update_semantics(void **state) {
	(void)state;
	htbl_ulong_t table;
	HTBL_INIT(table, 42);

	unsigned long data = 256;
	const unsigned long vicarious_data = data;
	HTBL_INSERT(table, "live while the", &data);
	data += 80;
	HTBL_INSERT(table, "whole world dies", &data);

	/* Updating existing item 'whole world dies' */
	unsigned long ret_data = 0;
	assert_true(HTBL_RETRIEVE(table, "whole world dies", ret_data));
	assert_int_equal(HTBL_INSERT(table, "whole world dies", (unsigned long *)&vicarious_data), HTBLSTAT_OK);
	assert_true(HTBL_RETRIEVE(table, "whole world dies", ret_data));
	assert_uint_equal(ret_data, vicarious_data);

	HTBL_FREE(table);
}

static void htbl_adv_insert_creates_no_duplicates(void **state) {
	(void)state;
	htbl_ulong_t table;
	HTBL_INIT(table, 42);

	unsigned long data = 256;
	const unsigned long vicarious_data = data + 42;
	HTBL_INSERT(table, "Vicariously", &data);
	data += 80;
	HTBL_INSERT(table, "I", &data);

	/* Remove and add back the same key - this should not create a new entry */
	HTBL_DELETE(table, "Vicariously");
	HTBL_INSERT(table, "Vicariously", (unsigned long *)&vicarious_data); /* update existing */

	/* Now search for 'Vicariously'; if found, it must be the only one */
	bool found = false;
	for (size_t i = 0; i < table.table->size; i++) {
		if (table.table->array[i] == NULL)
			continue;
		if (!found) {
			if ((unsigned long)table.table->array[i]->data == vicarious_data)
				found = true;
		} else
			assert_uint_not_equal((unsigned long)table.table->array[i]->data, vicarious_data);
	}

	HTBL_FREE(table);
}

int main(void) {
	int test_results = 0;

	struct CMUnitTest htbl_init_tests[] = {
	    cmocka_unit_test(htbl_init_returns),
	    cmocka_unit_test(htbl_init_single),
	    cmocka_unit_test(htbl_free_returns),
	    cmocka_unit_test(htbl_free_nullifies_arg),
	};

	struct CMUnitTest htbl_insert_tests[] = {
	    cmocka_unit_test(htbl_insert_returns),
	    cmocka_unit_test(htbl_insert_single),
	    cmocka_unit_test(htbl_insert_4_non_synonyms),
	    cmocka_unit_test(htbl_insert_4_synonyms_sequential),
	};
	struct CMUnitTest htbl_delete_tests[] = {
	    cmocka_unit_test(htbl_delete_returns),
	    cmocka_unit_test(htbl_delete_single),
	    cmocka_unit_test(htbl_clear_returns),
	};

	struct CMUnitTest htbl_exists_tests[] = {
	    cmocka_unit_test(htbl_exists_returns),
	    cmocka_unit_test(htbl_exists_single),
	    cmocka_unit_test(htbl_retrieve_returns),
	    cmocka_unit_test(hashtable_find_entry_returns),
	    cmocka_unit_test(hashtable_find_entry_several_valid),
	    cmocka_unit_test(hashtable_find_entry_several_invalid),
	};

	struct CMUnitTest htbl_adv_tests[] = {
	    cmocka_unit_test(htbl_adv_auto_resize),
	    cmocka_unit_test(htbl_adv_resize_tombstone_optimization),
	    cmocka_unit_test(htbl_adv_insert_update_semantics),
	    cmocka_unit_test(htbl_adv_insert_creates_no_duplicates),
	};

	EXEC_TEST_CATEGORY("Hashtable alloc/dealloc", htbl_init_tests);
	EXEC_TEST_CATEGORY("Hashtable basic entry insertion", htbl_insert_tests);
	EXEC_TEST_CATEGORY("Hashtable cleanup", htbl_delete_tests);
	EXEC_TEST_CATEGORY("Hashtable basic entry retrieval", htbl_exists_tests); /* Includes HTBL_EXISTS tests */
	EXEC_TEST_CATEGORY("Hashtable advanced", htbl_adv_tests);                 /* a bunch of random sh*t */

	return test_results;
}
