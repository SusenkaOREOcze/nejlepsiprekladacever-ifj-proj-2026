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
#include <stdio.h>
#include <string.h>

DEFTYPE_HASHTABLE(unsigned long) htbl_ulong_t;

static void test_return_codes(void **state) {
	(void)state;

	print_message("  Testing return codes of htbl_init\n");
	htbl_ulong_t null_table = {.table = NULL};
	htbl_ulong_t valid_table;
	HTBL_INIT(valid_table, 12);
	assert_int_equal(HTBL_INIT(valid_table, 0), HTBLSTAT_BAD_ARG);
	assert_int_equal(hashtable_init(NULL, 12), HTBLSTAT_BAD_ARG);

	print_message("  Testing returns of htbl_free and htbl_clear\n");
	assert_int_equal(HTBL_FREE(null_table), HTBLSTAT_BAD_ARG);
	assert_int_equal(hashtable_free(NULL), HTBLSTAT_BAD_ARG);
	assert_int_equal(HTBL_CLEAR(null_table), HTBLSTAT_BAD_ARG);
	assert_int_equal(hashtable_clear(NULL), HTBLSTAT_BAD_ARG);

	print_message("  Testing returns of htbl_insert\n");
	unsigned long data = 21;
	assert_int_equal(HTBL_INSERT(null_table, "Hi", &data), HTBLSTAT_BAD_ARG);
	assert_int_equal(hashtable_insert(&valid_table.table, "Hi", NULL, 21), HTBLSTAT_BAD_ARG);
	assert_int_equal(hashtable_insert(&valid_table.table, "Hi", &data, 0), HTBLSTAT_BAD_ARG);
	assert_int_equal(HTBL_INSERT(valid_table, NULL, &data), HTBLSTAT_BAD_ARG);
	assert_int_equal(HTBL_INSERT(valid_table, "\0", &data), HTBLSTAT_BAD_ARG);
	assert_uint_equal(valid_table.table->count, 0);

	print_message("  Testing returns of htbl_delete\n");
	assert_int_equal(HTBL_DELETE(null_table, "Hi"), HTBLSTAT_BAD_ARG);
	assert_int_equal(HTBL_DELETE(valid_table, "\0"), HTBLSTAT_BAD_ARG);
	assert_int_equal(HTBL_DELETE(valid_table, "Hi"), HTBLSTAT_OK);

	print_message("  Testing returns of htbl_exists and htbl_retrieve\n");
	assert_false(HTBL_EXISTS(null_table, "Hi"));
	assert_false(HTBL_EXISTS(valid_table, "Hi"));
	assert_false(HTBL_EXISTS(valid_table, NULL));
	assert_false(HTBL_EXISTS(valid_table, "\0"));

	unsigned long retrieve = 69420;
	assert_false(HTBL_RETRIEVE(null_table, "Hi", retrieve));
	assert_false(HTBL_RETRIEVE(valid_table, "Nope", retrieve));
	assert_uint_equal(retrieve, 69420);

	assert_int_equal(HTBL_FREE(null_table), HTBLSTAT_BAD_ARG);
	assert_int_equal(hashtable_free(NULL), HTBLSTAT_BAD_ARG);
	HTBL_FREE(valid_table);
}

static void test_find_entry(void **state) {
	(void)state;

	htbl_ulong_t table;
	HTBL_INIT(table, 32);

	constexpr size_t KEY_COUNT = 16;
	static const char *keys[KEY_COUNT] = {
	    "The",     "Fitness",  "Gram", "pacer", "test",          "is",   "a",    "multi-stage",
	    "aerobic", "capacity", "test", "that",  "progressively", "gets", "more", "difficult",
	};
	static size_t indeces[KEY_COUNT] = {
	    0,
	};
	/* Inserting keys for testing */
	for (size_t i = 0; i < KEY_COUNT; i++) {
		HTBL_INSERT(table, keys[i], &i);
	}

	/* Find the index of every key _without_ using the _find_entry function */
	for (size_t key_idx = 0; key_idx < KEY_COUNT; key_idx++) {
		htbl_key_t key = keys[key_idx];

		for (size_t table_idx = 0; table_idx < table.table->size; table_idx++) {
			htbl_entry_t *entry = table.table->array[table_idx];
			if (entry == NULL || strcmp(entry->key, key) != 0)
				continue;
			indeces[key_idx] = table_idx;
		}
	}

	print_message("  Testing hashtable_find_entry with HTBL_FIND_VALID, table has only valid entries\n");
	for (size_t i = 0; i < KEY_COUNT; i++) {
		size_t find_func_index;
		size_t find_invalid_index;

		assert_non_null(hashtable_find_entry(table.table, keys[i], &find_func_index, HTBL_FIND_VALID));
		assert_uint_equal(find_func_index, indeces[i]);
		assert_non_null(hashtable_find_entry(table.table, keys[i], &find_invalid_index, HTBL_FIND_INVALID));
		assert_uint_equal(find_invalid_index, indeces[i]);
	}

	HTBL_DELETE(table, keys[3]);
	HTBL_DELETE(table, keys[14]);
	print_message("  Repeating test with HTBL_FIND_VALID with two invalid (tombstone) entries in table\n");
	for (size_t i = 0; i < KEY_COUNT; i++) {
		size_t find_func_index;
		htbl_entry_t *entry = hashtable_find_entry(table.table, keys[i], &find_func_index, HTBL_FIND_VALID);

		/* If a valid entry was returned, indeces must still match */
		if (entry)
			assert_uint_equal(find_func_index, indeces[i]);
	}

	HTBL_FREE(table);
}

static void test_init_free(void **state) {
	(void)state;

	htbl_ulong_t new_table;
	assert_int_equal(HTBL_INIT(new_table, 0), HTBLSTAT_BAD_ARG);

	const size_t new_size = 19;
	assert_int_equal(HTBL_INIT(new_table, new_size), HTBLSTAT_OK);
	assert_uint_equal(new_table.table->size, new_size);
	assert_uint_equal(new_table.table->count, 0);

	for (size_t i = 0; i < new_table.table->size; i++) {
		assert_null(new_table.table->array[i]);
	}

	assert_int_equal(HTBL_FREE(new_table), HTBLSTAT_OK);
	assert_null(new_table.table);
}

static void test_insert_retrieve(void **state) {
	(void)state;

	htbl_ulong_t table;
	HTBL_INIT(table, 16);

	unsigned long value = 42;
	assert_int_equal(HTBL_INSERT(table, "Hello World", &value), HTBLSTAT_OK);
	assert_uint_equal(table.table->count, 1);

	unsigned long dest = 0;
	assert_true(HTBL_RETRIEVE(table, "Hello World", dest));
	assert_uint_equal(dest, value);
	assert_false(HTBL_RETRIEVE(table, "Nonexistent", dest));

	HTBL_FREE(table);
}

static void test_clear(void **state) {
	(void)state;

	htbl_ulong_t table;
	const size_t new_size = 21;
	HTBL_INIT(table, new_size);

	const char *keys[] = {"Electro", "Motive", "Division", "Super Duty", "70 Series"};
	const size_t key_count = sizeof(keys) / sizeof(char *);
	for (size_t i = 0; i < key_count; i++)
		assert_int_equal(HTBL_INSERT(table, keys[i], &i), HTBLSTAT_OK);
	assert_uint_equal(table.table->count, key_count);

	assert_int_equal(HTBL_DELETE(table, "Motive"), HTBLSTAT_OK);
	assert_false(HTBL_EXISTS(table, "Motive"));

	assert_int_equal(HTBL_CLEAR(table), HTBLSTAT_OK);
	for (size_t i = 0; i < key_count; i++)
		assert_false(HTBL_EXISTS(table, keys[i]));
	assert_uint_equal(table.table->count, 0);
	assert_uint_equal(table.table->size, new_size);

	HTBL_FREE(table);
}

static void test_resize(void **state) {
	(void)state;

	htbl_ulong_t table;
	const size_t init_size = 6;
	HTBL_INIT(table, init_size);

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

static void test_resize_optimization(void **state) {
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

static void test_insert_update_semantics(void **state) {
	(void)state;

	htbl_ulong_t table;
	HTBL_INIT(table, 16);

	/* Add some data to the table */
	unsigned long data = 256;
	const unsigned long vicarious_data = data;
	HTBL_INSERT(table, "Vicariously", &data);
	data += 80;
	HTBL_INSERT(table, "I", &data);
	data -= 60;
	HTBL_INSERT(table, "live while", &data);
	data *= 3;
	HTBL_INSERT(table, "the whole world dies.", &data);

	/* Delete and add the same key back and make sure no duplicates are created */
	HTBL_DELETE(table, "Vicariously");
	HTBL_INSERT(table, "Vicariously", (unsigned long *)&vicarious_data);

	bool found = false;
	for (size_t i = 0; i < table.table->size; i++) {
		if (table.table->array[i] == NULL)
			continue;
		/* Make sure no duplicates (including invalid entries) exist in the table */
		if (!found) {
			if ((unsigned long)table.table->array[i]->data == vicarious_data)
				found = true;
		} else
			assert_uint_not_equal((unsigned long)table.table->array[i]->data, vicarious_data);
	}

	/* Checking if updating data works */
	unsigned long old_data = 0;
	HTBL_RETRIEVE(table, "the whole world dies.", old_data);
	HTBL_INSERT(table, "the whole world dies.", (unsigned long *)&vicarious_data);
	HTBL_RETRIEVE(table, "the whole world dies.", old_data);
	assert_uint_equal(old_data, vicarious_data);

	HTBL_FREE(table);
}

int main(void) {
	const struct CMUnitTest tests[] = {cmocka_unit_test(test_return_codes),
	                                   cmocka_unit_test(test_init_free),
	                                   cmocka_unit_test(test_insert_retrieve),
	                                   cmocka_unit_test(test_clear),
	                                   cmocka_unit_test(test_resize),
	                                   cmocka_unit_test(test_find_entry),
	                                   cmocka_unit_test(test_resize_optimization),
	                                   cmocka_unit_test(test_insert_update_semantics)};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
