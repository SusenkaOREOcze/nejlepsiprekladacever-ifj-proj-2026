/**
   @file functable.c
   @author xbosakd00 (DasBean125)
   @date 2026-10-25

   @brief A module defining the function table data structure.

   For the "why's", see functable.h.
*/

#include "include/functable_impl.h"

/*=========
  Constants
  =========*/

constexpr size_t FUNCT_INIT_TYPE_ARRAY_SIZE = 1024;
constexpr size_t FUNCT_INIT_SCOPE_SIZE = 128;

/*=============
  Init and free
  =============*/

symt_stat_t funct_init(funct_t **const func_table) {
	if (func_table == NULL)
		return SYMTSTAT_BAD_ARG;

	/* Table allocation */
	funct_t *new_table = malloc(sizeof(*new_table));
	if (new_table == NULL)
		return SYMTSTAT_BAD_ALLOC;

	/* Type array allocations */
	new_table->func_call_type_array = malloc(FUNCT_INIT_TYPE_ARRAY_SIZE * sizeof(int8_t));
	if (new_table->func_call_type_array == NULL)
		goto FAIL_ALLOC_CALL;

	new_table->func_def_type_array = malloc(FUNCT_INIT_TYPE_ARRAY_SIZE * sizeof(int8_t));
	if (new_table->func_def_type_array == NULL)
		goto FAIL_ALLOC_DEF;

	/* Hashtable (scope, search table, whatever you fancy) init */
	if (htbl_init(new_table->scope, FUNCT_INIT_SCOPE_SIZE) != HTBLSTAT_OK)
		goto FAIL_ALLOC_HTBL;

	/* Remaining table init and assignment to parameter */
	new_table->call_array_count = 0;
	new_table->call_array_im_count = 0;
	new_table->call_array_size = FUNCT_INIT_TYPE_ARRAY_SIZE;

	new_table->def_array_count = 0;
	new_table->def_array_im_count = 0;
	new_table->def_array_size = FUNCT_INIT_TYPE_ARRAY_SIZE;

	new_table->state_insert = false;
	*func_table = new_table;

	return SYMTSTAT_OK;

	/* Sequential free for every successfully allocated piece of memory */
FAIL_ALLOC_HTBL:
	free(new_table->func_def_type_array);
FAIL_ALLOC_DEF:
	free(new_table->func_call_type_array);
FAIL_ALLOC_CALL:
	free(new_table);

	return SYMTSTAT_BAD_ALLOC;
}

symt_stat_t funct_free(funct_t *func_table) {
	if (func_table == NULL)
		return SYMTSTAT_BAD_ARG;

	/* Blow it all up */
	free(func_table->func_call_type_array);
	free(func_table->func_def_type_array);
	htbl_free(func_table->scope);
	free(func_table);

	return SYMTSTAT_OK;
}

symt_stat_t funct_find_symbol(const funct_t *table, const char *ident, funct_symbol_t *out_symbol) {
	if (table == NULL || ident == NULL || out_symbol == NULL)
		return SYMTSTAT_BAD_ARG;

	/* Starting to realize these are just glorified wrapper functions... */
	if (htbl_retrieve(table->scope, ident, *out_symbol) != HTBLSTAT_OK)
		return SYMTSTAT_NO_SYMBOL;

	return SYMTSTAT_OK;
}

symt_data_type_t funct_nth_param_type(const funct_symbol_t *func_symbol, const size_t param_number) {
	if (func_symbol == NULL || param_number >= func_symbol->param_count)
		return SYMT_TYPE_UNDEFINED;
	// TODO: figure this out
}

inline bool funct_pending_insert(const funct_t *table) { return table->state_insert; }

inline bool funct_exists(const funct_t *table, const char *ident) {
	if (table == NULL || ident == NULL)
		return false;

	return htbl_exists(table->scope, ident);
}


