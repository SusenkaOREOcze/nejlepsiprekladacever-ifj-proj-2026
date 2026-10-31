/**
   @file test_hashtable.c
   @author xbosakd00 (DasBean125)
   @data 2026-10-06

   @brief Tests the hash table data structure. Your life may depend on it if this succeedes.

   Is this a test? It has to be, otherwise I can't go on...
*/

#include "../include/hashtable.h"

#ifndef HTBL_TEST
#define HTBL_TEST
#endif
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
	htbl_init(valid_table, 12);
	assert_int_equal(htbl_init(valid_table, 0), HTBLSTAT_BAD_ARG);
	assert_int_equal(hashtable_init(NULL, 12), HTBLSTAT_BAD_ARG);

	print_message("  Testing returns of htbl_free and htbl_clear\n");
	assert_int_equal(htbl_free(null_table), HTBLSTAT_BAD_ARG);
	assert_int_equal(hashtable_free(NULL), HTBLSTAT_BAD_ARG);
	assert_int_equal(htbl_clear(null_table), HTBLSTAT_BAD_ARG);
	assert_int_equal(hashtable_clear(NULL), HTBLSTAT_BAD_ARG);

	print_message("  Testing returns of htbl_insert\n");
	unsigned long data = 21;
	assert_int_equal(htbl_insert(null_table, "Hi", &data), HTBLSTAT_BAD_ARG);
	assert_int_equal(hashtable_insert(&valid_table.table, "Hi", NULL, 21), HTBLSTAT_BAD_ARG);
	assert_int_equal(hashtable_insert(&valid_table.table, "Hi", &data, 0), HTBLSTAT_BAD_ARG);
	assert_int_equal(htbl_insert(valid_table, NULL, &data), HTBLSTAT_BAD_ARG);
	assert_int_equal(htbl_insert(valid_table, "\0", &data), HTBLSTAT_BAD_ARG);
	assert_uint_equal(valid_table.table->count, 0);

	print_message("  Testing returns of htbl_delete\n");
	assert_int_equal(htbl_delete(null_table, "Hi"), HTBLSTAT_BAD_ARG);
	assert_int_equal(htbl_delete(valid_table, "\0"), HTBLSTAT_BAD_ARG);
	assert_int_equal(htbl_delete(valid_table, "Hi"), HTBLSTAT_OK);

	print_message("  Testing returns of htbl_exists and htbl_retrieve\n");
	assert_false(htbl_exists(null_table, "Hi"));
	assert_false(htbl_exists(valid_table, "Hi"));
	assert_false(htbl_exists(valid_table, NULL));
	assert_false(htbl_exists(valid_table, "\0"));

	unsigned long retrieve = 69420;
	assert_false(htbl_retrieve(null_table, "Hi", retrieve));
	assert_false(htbl_retrieve(valid_table, "Nope", retrieve));
	assert_uint_equal(retrieve, 69420);

	assert_int_equal(htbl_free(null_table), HTBLSTAT_BAD_ARG);
	assert_int_equal(hashtable_free(NULL), HTBLSTAT_BAD_ARG);
	htbl_free(valid_table);
}

static void test_find_entry(void **state) {
	(void)state;

	htbl_ulong_t table;
	htbl_init(table, 32);

	constexpr size_t key_count = 16;
	static const char *keys[key_count] = {
	    "The",     "Fitness",  "Gram", "pacer", "test",          "is",   "a",    "multi-stage",
	    "aerobic", "capacity", "test", "that",  "progressively", "gets", "more", "difficult",
	};
	static size_t indeces[key_count] = {
	    0,
	};
	/* Inserting keys for testing */
	for (size_t i = 0; i < key_count; i++) {
		htbl_insert(table, keys[i], &i);
	}

	/* Find the index of every key _without_ using the _find_entry function */
	for (size_t key_idx = 0; key_idx < key_count; key_idx++) {
		htbl_key_t key = keys[key_idx];

		for (size_t table_idx = 0; table_idx < table.table->size; table_idx++) {
			htbl_entry_t *entry = table.table->array[table_idx];
			if (entry == NULL || strcmp(entry->key, key) != 0)
				continue;
			indeces[key_idx] = table_idx;
		}
	}

	print_message("  Testing hashtable_find_entry with HTBL_FIND_VALID, table has only valid entries\n");
	for (size_t i = 0; i < key_count; i++) {
		size_t find_func_index;
		size_t find_invalid_index;

		assert_non_null(hashtable_find_entry(table.table, keys[i], &find_func_index, HTBL_FIND_VALID));
		assert_uint_equal(find_func_index, indeces[i]);
		assert_non_null(hashtable_find_entry(table.table, keys[i], &find_invalid_index, HTBL_FIND_INVALID));
		assert_uint_equal(find_invalid_index, indeces[i]);
	}

	htbl_delete(table, keys[3]);
	htbl_delete(table, keys[14]);
	print_message("  Repeating test with HTBL_FIND_VALID with two invalid (tombstone) entries in table\n");
	for (size_t i = 0; i < key_count; i++) {
		size_t find_func_index;
		htbl_entry_t *entry = hashtable_find_entry(table.table, keys[i], &find_func_index, HTBL_FIND_VALID);

		/* If a valid entry was returned, indeces must still match */
		if (entry)
			assert_uint_equal(find_func_index, indeces[i]);
	}

	htbl_free(table);
}

static void test_init_free(void **state) {
	(void)state;

	htbl_ulong_t newTable;
	assert_int_equal(htbl_init(newTable, 0), HTBLSTAT_BAD_ARG);

	const size_t newSize = 19;
	assert_int_equal(htbl_init(newTable, newSize), HTBLSTAT_OK);
	assert_uint_equal(newTable.table->size, newSize);
	assert_uint_equal(newTable.table->count, 0);

	for (size_t i = 0; i < newTable.table->size; i++) {
		assert_null(newTable.table->array[i]);
	}

	assert_int_equal(htbl_free(newTable), HTBLSTAT_OK);
	assert_null(newTable.table);
}

static void test_insert_retrieve(void **state) {
	(void)state;

	htbl_ulong_t table;
	htbl_init(table, 16);

	unsigned long value = 42;
	assert_int_equal(htbl_insert(table, "Hello World", &value), HTBLSTAT_OK);
	assert_uint_equal(table.table->count, 1);

	unsigned long dest = 0;
	assert_true(htbl_retrieve(table, "Hello World", dest));
	assert_uint_equal(dest, value);
	assert_false(htbl_retrieve(table, "Nonexistent", dest));

	htbl_free(table);
}

static void test_clear(void **state) {
	(void)state;

	htbl_ulong_t table;
	const size_t newSize = 21;
	htbl_init(table, newSize);

	const char *keys[] = {"Electro", "Motive", "Division", "Super Duty", "70 Series"};
	const size_t keyCount = sizeof(keys) / sizeof(char *);
	for (size_t i = 0; i < keyCount; i++)
		assert_int_equal(htbl_insert(table, keys[i], &i), HTBLSTAT_OK);
	assert_uint_equal(table.table->count, keyCount);

	assert_int_equal(htbl_delete(table, "Motive"), HTBLSTAT_OK);
	assert_false(htbl_exists(table, "Motive"));

	assert_int_equal(htbl_clear(table), HTBLSTAT_OK);
	for (size_t i = 0; i < keyCount; i++)
		assert_false(htbl_exists(table, keys[i]));
	assert_uint_equal(table.table->count, 0);
	assert_uint_equal(table.table->size, newSize);

	htbl_free(table);
}

static void test_resize(void **state) {
	(void)state;

	htbl_ulong_t table;
	const size_t initSize = 6;
	htbl_init(table, initSize);

	const char *keys[] = {"Resistance", "Reactance", "Impedance", "Conductance", "Susceptance", "Admittance"};
	const size_t keyCount = sizeof(keys) / sizeof(char *);
	const size_t resizeCount = (const size_t)(0.64f * initSize) + 1; /* This factor is statically defined,
	                                                                  * so it must be manually copied
	                                                                  * here */
	for (size_t i = 0; i < keyCount; i++) {
		assert_int_equal(htbl_insert(table, keys[i], &i), HTBLSTAT_OK);

		if (i == (resizeCount - 1))
			assert_int_equal(table.table->size, initSize);
		if (i == resizeCount)
			assert_int_equal(table.table->size, initSize * 2);
	}

	for (size_t i = 0; i < keyCount; i++) {
		unsigned long data = 0;
		assert_true(htbl_retrieve(table, keys[i], data));
		assert_uint_equal(data, i);
	}

	htbl_free(table);
}

static void test_resize_optimization(void **state) {
	(void)state;

	htbl_ulong_t table;
	htbl_init(table, 4);

	unsigned long data = 69;
	htbl_insert(table, "0", &data);
	/* Create a tombstone without resizing */
	data = 125;
	htbl_insert(table, "1", &data);
	htbl_delete(table, "1");

	/* Add items until a resize is triggered but without overwriting the tombstone */
	data = 400;
	htbl_insert(table, "2", &data);
	htbl_insert(table, "3", &data); /* 3/4 > 0.64 -> resize operation on _next_ insert */
	htbl_insert(table, "4", &data);
	assert_uint_equal(table.table->size, 8);

	/* Make sure all the invalid entries are gone */
	for (size_t i = 0; i < table.table->size; i++) {
		htbl_entry_t *entry = table.table->array[i];
		if (entry != NULL)
			assert_false(entry->key[0] == '\0');
	}

	htbl_free(table);
}

static void test_insert_update_semantics(void **state) {
	(void)state;

	htbl_ulong_t table;
	htbl_init(table, 16);

	/* Add some data to the table */
	unsigned long data = 256;
	const unsigned long vicarious_data = data;
	htbl_insert(table, "Vicariously", &data);
	data += 80;
	htbl_insert(table, "I", &data);
	data -= 60;
	htbl_insert(table, "live while", &data);
	data *= 3;
	htbl_insert(table, "the whole world dies.", &data);

	/* Delete and add the same key back and make sure no duplicates are created */
	htbl_delete(table, "Vicariously");
	htbl_insert(table, "Vicariously", (unsigned long *)&vicarious_data);

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
	htbl_retrieve(table, "the whole world dies.", old_data);
	htbl_insert(table, "the whole world dies.", (unsigned long *)&vicarious_data);
	htbl_retrieve(table, "the whole world dies.", old_data);
	assert_uint_equal(old_data, vicarious_data);

	htbl_free(table);
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
