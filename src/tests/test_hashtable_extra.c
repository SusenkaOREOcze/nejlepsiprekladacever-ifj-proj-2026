/**
 * @file test_hashtable_extra.c
 * @brief Additional public API and deleted-slot regression tests.
 */

#include "../include/hashtable.h"
#include "cmocka/cmocka.h"

#include <stdio.h>

DEFTYPE_HASHTABLE(int) int_table_t;

/**
 * @brief Tests inserting a value and retrieving it by its key.
 * @details The retrieved value is expected to equal the value that was inserted.
 */
static void test_insert_and_retrieve(void **state) {
	(void)state;
	int_table_t table = {0};
	int value = 42;
	int found = -1;

	assert_int_equal(HTBL_INIT(table, 4), HTBLSTAT_OK);
	assert_int_equal(HTBL_INSERT(table, "answer", &value), HTBLSTAT_OK);
	assert_true(HTBL_RETRIEVE(table, "answer", found));
	assert_int_equal(found, 42);
	assert_int_equal(HTBL_FREE(table), HTBLSTAT_OK);
}

/**
 * @brief Tests replacing the value associated with an existing key.
 * @details A lookup after the update is expected to return the replacement value.
 */
static void test_update_existing_key(void **state) {
	(void)state;
	int_table_t table = {0};
	int first = 1;
	int second = 2;
	int found = 0;

	assert_int_equal(HTBL_INIT(table, 4), HTBLSTAT_OK);
	assert_int_equal(HTBL_INSERT(table, "x", &first), HTBLSTAT_OK);
	assert_int_equal(HTBL_INSERT(table, "x", &second), HTBLSTAT_OK);
	assert_true(HTBL_RETRIEVE(table, "x", found));
	assert_int_equal(found, 2);
	assert_int_equal(HTBL_FREE(table), HTBLSTAT_OK);
}

/**
 * @brief Tests reusing a table slot after its entry is deleted.
 * @details Inserting a different key into the deleted slot is expected to succeed.
 */
static void test_delete_then_reuse_only_slot(void **state) {
	(void)state;
	int_table_t table = {0};
	int value = 7;

	// The deleted slot is the only physical slot in the table.
	assert_int_equal(HTBL_INIT(table, 1), HTBLSTAT_OK);
	assert_int_equal(HTBL_INSERT(table, "old", &value), HTBLSTAT_OK);
	assert_int_equal(HTBL_DELETE(table, "old"), HTBLSTAT_OK);
	assert_int_equal(HTBL_INSERT(table, "new", &value), HTBLSTAT_OK);
	assert_true(HTBL_EXISTS(table, "new"));
	assert_int_equal(HTBL_FREE(table), HTBLSTAT_OK);
}

/**
 * @brief Tests that clearing a table removes its entries while keeping it usable.
 * @details The old key is expected to be absent and a new entry should be insertable.
 */
static void test_clear_then_reuse_table(void **state) {
	(void)state;
	int_table_t table = {0};
	int value = 3;

	assert_int_equal(HTBL_INIT(table, 2), HTBLSTAT_OK);
	assert_int_equal(HTBL_INSERT(table, "old", &value), HTBLSTAT_OK);
	assert_int_equal(HTBL_CLEAR(table), HTBLSTAT_OK);
	assert_false(HTBL_EXISTS(table, "old"));
	assert_int_equal(HTBL_INSERT(table, "new", &value), HTBLSTAT_OK);
	assert_int_equal(HTBL_FREE(table), HTBLSTAT_OK);
}

/**
 * @brief Tests lookup behavior when the requested key is absent.
 * @details Lookup is expected to fail without changing the caller's output value.
 */
static void test_missing_key_preserves_output(void **state) {
	(void)state;
	int_table_t table = {0};
	int found = 99;

	assert_int_equal(HTBL_INIT(table, 4), HTBLSTAT_OK);
	assert_false(HTBL_RETRIEVE(table, "missing", found));
	assert_int_equal(found, 99);
	assert_int_equal(HTBL_FREE(table), HTBLSTAT_OK);
}

/**
 * @brief Tests that many entries remain retrievable as the table grows.
 * @details All 200 inserted keys are expected to return their corresponding values.
 */
static void test_many_insertions_survive_resize(void **state) {
	(void)state;
	int_table_t table = {0};
	char key[32];
	int found = -1;

	assert_int_equal(HTBL_INIT(table, 2), HTBLSTAT_OK);
	for (int i = 0; i < 200; i++) {
		snprintf(key, sizeof(key), "key%d", i);
		assert_int_equal(HTBL_INSERT(table, key, &i), HTBLSTAT_OK);
	}

	for (int i = 0; i < 200; i++) {
		snprintf(key, sizeof(key), "key%d", i);
		assert_true(HTBL_RETRIEVE(table, key, found));
		assert_int_equal(found, i);
	}
	assert_int_equal(HTBL_FREE(table), HTBLSTAT_OK);
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_insert_and_retrieve),
	    cmocka_unit_test(test_update_existing_key),
	    cmocka_unit_test(test_delete_then_reuse_only_slot),
	    cmocka_unit_test(test_clear_then_reuse_table),
	    cmocka_unit_test(test_missing_key_preserves_output),
	    cmocka_unit_test(test_many_insertions_survive_resize),
	};

	return cmocka_run_group_tests_name("Hashtable extra", tests, NULL, NULL);
}
