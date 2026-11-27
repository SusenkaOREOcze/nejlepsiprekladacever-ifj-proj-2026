/**
 * @file test_symtable_extra.c
 * @brief Additional scope and symbol identity tests.
 */

#include "../include/symtable.h"
#include "cmocka/cmocka.h"

/**
 * @brief Tests name shadowing across nested scopes.
 * @details Lookup should find the inner symbol first, then the outer symbol after leaving the inner scope;
 *          the two symbols are expected to have different IDs.
 */
static void test_shadowing_keeps_distinct_ids(void **state) {
	(void)state;
	symt_t *table = NULL;
	symt_symbol_t outer = {.data_type = SYMT_TYPE_INTEGER, .mutable = true};
	symt_symbol_t inner = {.data_type = SYMT_TYPE_STRING, .mutable = true};
	symt_symbol_t found = {0};

	assert_int_equal(symt_init(&table), SYMTSTAT_OK);
	assert_int_equal(symt_insert(table, "x", &outer), SYMTSTAT_OK);
	assert_int_equal(symt_inc_scope_depth(table), SYMTSTAT_OK);
	assert_int_equal(symt_insert(table, "x", &inner), SYMTSTAT_OK);
	assert_int_equal(symt_find_symbol(table, "x", &found), SYMTSTAT_OK);
	assert_int_equal(found.uid, inner.uid);
	assert_int_not_equal(inner.uid, outer.uid);
	assert_int_equal(symt_dec_scope_depth(table), SYMTSTAT_OK);
	assert_int_equal(symt_find_symbol(table, "x", &found), SYMTSTAT_OK);
	assert_int_equal(found.uid, outer.uid);
	assert_int_equal(symt_free(table), SYMTSTAT_OK);
}

/**
 * @brief Tests that leaving a scope removes its local symbols from lookup.
 * @details A symbol inserted only in the popped scope is expected to be absent afterward.
 */
static void test_popped_scope_drops_symbols(void **state) {
	(void)state;
	symt_t *table = NULL;
	symt_symbol_t symbol = {.data_type = SYMT_TYPE_BOOLEAN, .mutable = true};
	symt_symbol_t found = {0};

	assert_int_equal(symt_init(&table), SYMTSTAT_OK);
	assert_int_equal(symt_inc_scope_depth(table), SYMTSTAT_OK);
	assert_int_equal(symt_insert(table, "local", &symbol), SYMTSTAT_OK);
	assert_int_equal(symt_dec_scope_depth(table), SYMTSTAT_OK);
	assert_int_equal(symt_find_symbol(table, "local", &found), SYMTSTAT_NO_SYMBOL);
	assert_int_equal(symt_free(table), SYMTSTAT_OK);
}

/**
 * @brief Tests retaining a mutable symbol's ID during an update.
 * @details Reusing the identifier must preserve its declared type and the ID.
 */
static void test_mutable_update_preserves_id(void **state) {
	(void)state;
	symt_t *table = NULL;
	symt_symbol_t symbol = {.data_type = SYMT_TYPE_INTEGER, .mutable = true};
	symt_symbol_t found = {0};

	// AST references must keep pointing to the same symbol after an update.
	assert_int_equal(symt_init(&table), SYMTSTAT_OK);
	assert_int_equal(symt_insert(table, "x", &symbol), SYMTSTAT_OK);
	const unsigned long long original_uid = symbol.uid;
	assert_int_equal(symt_insert(table, "x", &symbol), SYMTSTAT_OK);
	assert_int_equal(symt_find_symbol(table, "x", &found), SYMTSTAT_OK);
	assert_int_equal(found.data_type, SYMT_TYPE_INTEGER);
	assert_int_equal(found.uid, original_uid);
	assert_int_equal(symt_free(table), SYMTSTAT_OK);
}
// Fail test changing the data_type, etc...

/**
 * @brief Tests that an immutable symbol cannot be inserted again to change its value.
 * @details The update is expected to fail and the original value and ID should remain intact.
 */
static void test_immutable_update_is_rejected(void **state) {
	(void)state;
	symt_t *table = NULL;
	symt_symbol_t symbol = {.data_type = SYMT_TYPE_INTEGER, .mutable = false};
	symt_symbol_t found = {0};

	// A second insertion must leave a val symbol unchanged.
	assert_int_equal(symt_init(&table), SYMTSTAT_OK);
	assert_int_equal(symt_insert(table, "x", &symbol), SYMTSTAT_OK);
	const unsigned long long original_uid = symbol.uid;
	symbol.data_type = SYMT_TYPE_DOUBLE;
	// this fails because symt_insert doesnt check for mutability.
	assert_int_not_equal(symt_insert(table, "x", &symbol), SYMTSTAT_OK);
	assert_int_equal(symt_find_symbol(table, "x", &found), SYMTSTAT_OK);
	assert_int_equal(found.data_type, SYMT_TYPE_INTEGER);
	assert_int_equal(found.uid, original_uid);
	assert_int_equal(symt_free(table), SYMTSTAT_OK);
}

/**
 * @brief Tests that a popped scope is not reused with its former symbols.
 * @details A symbol from the first nested scope is expected to be absent in a later sibling scope.
 */
static void test_sibling_scopes_do_not_share_symbols(void **state) {
	(void)state;
	symt_t *table = NULL;
	symt_symbol_t symbol = {.data_type = SYMT_TYPE_INTEGER, .mutable = true};
	symt_symbol_t found = {0};

	assert_int_equal(symt_init(&table), SYMTSTAT_OK);
	assert_int_equal(symt_inc_scope_depth(table), SYMTSTAT_OK);
	assert_int_equal(symt_insert(table, "first", &symbol), SYMTSTAT_OK);
	assert_int_equal(symt_dec_scope_depth(table), SYMTSTAT_OK);
	assert_int_equal(symt_inc_scope_depth(table), SYMTSTAT_OK);
	assert_int_equal(symt_find_symbol(table, "first", &found), SYMTSTAT_NO_SYMBOL);
	assert_int_equal(symt_dec_scope_depth(table), SYMTSTAT_OK);
	assert_int_equal(symt_free(table), SYMTSTAT_OK);
}

/**
 * @brief Tests lookup behavior for a missing symbol.
 * @details Lookup should return SYMTSTAT_NO_SYMBOL and leave the output structure unchanged.
 */
static void test_missing_symbol_preserves_output(void **state) {
	(void)state;
	symt_t *table = NULL;
	symt_symbol_t found = {.data_type = SYMT_TYPE_STRING, .uid = 123};

	assert_int_equal(symt_init(&table), SYMTSTAT_OK);
	assert_int_equal(symt_find_symbol(table, "missing", &found), SYMTSTAT_NO_SYMBOL);
	assert_int_equal(found.data_type, SYMT_TYPE_STRING);
	assert_int_equal(found.uid, 123);
	assert_int_equal(symt_free(table), SYMTSTAT_OK);
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_shadowing_keeps_distinct_ids),
	    cmocka_unit_test(test_popped_scope_drops_symbols),
	    cmocka_unit_test(test_mutable_update_preserves_id),
	    cmocka_unit_test(test_immutable_update_is_rejected),
	    cmocka_unit_test(test_sibling_scopes_do_not_share_symbols),
	    cmocka_unit_test(test_missing_symbol_preserves_output),
	};

	return cmocka_run_group_tests_name("Symtable extra", tests, NULL, NULL);
}
