/**
   @file symtable.c
   @author xbosakd00 (DasBean125)
   @date 2026-10-19

   @brief Symbol table module.
*/

#include "include/symtable_impl.h"

constexpr size_t SYMT_INITIAL_SCOPE_SIZE = 64;
constexpr size_t SYMT_INITIAL_STANDBY_SCOPES = 10; /* If 0, a global scope is still allocated */

/*================
  HELPER FUNCTIONS
  ================*/

symt_stat_t symt_alloc_scope(symt_scope_t **const scope, const size_t scope_size) {
	if (scope == NULL || scope_size == 0)
		return SYMTSTAT_BAD_ARG;

	/* Allocate a new scope (stack element) */
	symt_scope_t *new_scope = malloc(sizeof(*new_scope));
	if (new_scope == NULL)
		return SYMTSTAT_BAD_ALLOC;

	/* Allocate the hashtable of that scope */
	if (HTBL_INIT(new_scope->scope, scope_size) != HTBLSTAT_OK) {
		free(new_scope);
		return SYMTSTAT_BAD_ALLOC;
	}

	new_scope->prev = NULL;
	*scope = new_scope; /* Giving the caller the scope pointer */
	return SYMTSTAT_OK;
}

symt_stat_t symt_clear_scope(symt_scope_t *const scope) {
	if (scope == NULL)
		return SYMTSTAT_BAD_ARG;

	HTBL_CLEAR(scope->scope);
	return SYMTSTAT_OK;
}

/*===========
  INIT & FREE
  ===========*/

symt_stat_t symt_init(symt_t **const sym_table) {
	if (sym_table == NULL)
		return SYMTSTAT_BAD_ARG;
	symt_stat_t result = SYMTSTAT_OK;
	symt_t *new_table = malloc(sizeof(symt_t));
	if (new_table == NULL)
		return SYMTSTAT_BAD_ALLOC;

	/* Initialize an empty symbol table */
	new_table->scope_top = NULL;
	new_table->standby_top = NULL;
	new_table->scope_depth = 0;
	new_table->uid_accum = 0;

	/* Add the global scope onto the main stack */
	symt_scope_t *new_global_scope = NULL;
	result = symt_alloc_scope(&new_global_scope, SYMT_INITIAL_SCOPE_SIZE);
	if (result != SYMTSTAT_OK)
		goto FAIL_ALLOC; /* boo */

	result = symt_push_scope(&new_table->scope_top, new_global_scope);
	if (result != SYMTSTAT_OK)
		goto FAIL_PUSH;

	/* Add the initial scope count to the standby stack */
	for (size_t i = 0; i < SYMT_INITIAL_STANDBY_SCOPES; i++) {
		symt_scope_t *new_scope = NULL;
		result = symt_alloc_scope(&new_scope, SYMT_INITIAL_SCOPE_SIZE);
		if (result != SYMTSTAT_OK)
			goto FAIL_ALLOC;

		result = symt_push_scope(&new_table->standby_top, new_scope);
		if (result != SYMTSTAT_OK)
			goto FAIL_PUSH;
	}

	*sym_table = new_table;
	return result;

FAIL_PUSH:
	fprintf(stderr, "symt_init: failed to push scope onto stack\n");
	*sym_table = new_table;
	return result;

FAIL_ALLOC:
	/* If more than 1 allocation fails, this frees all the allocated memory thus far before exitting */
	while (new_table->standby_top != NULL) {
		symt_scope_t *cur_scope = new_table->standby_top;
		new_table->standby_top = cur_scope->prev; /* Descend down the stack */
		symt_free_scope(cur_scope);
	}

	if (new_table->scope_top != NULL) {
		symt_free_scope(new_table->scope_top);
		new_table->scope_top = NULL;
	}
	free(new_table);
	return result;
}

symt_stat_t symt_free(symt_t *sym_table) {
	if (sym_table == NULL)
		return SYMTSTAT_BAD_ARG;

	/* Free the active scope stack */
	while (sym_table->scope_top != NULL) {
		symt_scope_t *cur_scope = sym_table->scope_top;
		sym_table->scope_top = cur_scope->prev; /* Descend down the stack, vol. 2 */
		symt_free_scope(cur_scope);
	}

	/* Same sh*t, different stack */
	while (sym_table->standby_top != NULL) {
		symt_scope_t *cur_scope = sym_table->standby_top;
		sym_table->standby_top = cur_scope->prev;
		symt_free_scope(cur_scope);
	}

	free(sym_table);
	return SYMTSTAT_OK;
}

/*======================
  SCOPE DEPTH ADJUSTMENT
  ======================*/

symt_stat_t symt_inc_scope_depth(symt_t *sym_table) {
	if (sym_table == NULL)
		return SYMTSTAT_BAD_ARG;
	symt_stat_t result = SYMTSTAT_OK;

	/* Moving a scope from the standby stack onto the global scope stack */
	symt_scope_t *cur_standby_scope = NULL;
	result = symt_pop_scope(&sym_table->standby_top, &cur_standby_scope);
	if (result != SYMTSTAT_OK)
		fprintf(stderr, "symt_inc_scope_depth: ERROR - standby scope is empty. This should never happen!\n");

	result = symt_push_scope(&sym_table->scope_top, cur_standby_scope);
	if (result != SYMTSTAT_OK)
		fprintf(stderr, "symt_inc_scope_depth: failed to push standby scope@%p onto global scope stack\n",
		        (void *)cur_standby_scope);
	sym_table->scope_depth++;

	/* The standby stack is empty (how); add another scope to it */
	if (sym_table->standby_top == NULL) {
		symt_scope_t *new_scope = NULL;
		result = symt_alloc_scope(&new_scope, SYMT_INITIAL_SCOPE_SIZE);
		if (result != SYMTSTAT_OK)
			return result;

		result = symt_push_scope(&sym_table->standby_top, new_scope);
		if (result != SYMTSTAT_OK)
			return result;
	}

	return result;
}

symt_stat_t symt_dec_scope_depth(symt_t *sym_table) {
	if (sym_table == NULL)
		return SYMTSTAT_BAD_ARG;
	symt_stat_t result = SYMTSTAT_OK;

	/* At the global scope already, do nothing */
	if (sym_table->scope_depth == 0)
		return result;

	/* Doing the inverse of symt_inc_scope_depth() */
	symt_scope_t *cur_global_scope = NULL;
	symt_pop_scope(&sym_table->scope_top, &cur_global_scope);
	symt_clear_scope(cur_global_scope); /* This is very important */

	result = symt_push_scope(&sym_table->standby_top, cur_global_scope);
	sym_table->scope_depth--;

	return result;
}

/*==========
  PREDICATES
  ==========*/
bool symt_exists_in_current_scope(const symt_t *sym_table, const char *ident) {
	if (sym_table == NULL || ident == NULL || ident[0] == '\0')
		return false;
	if (sym_table->scope_top == NULL)
		return false;

	return HTBL_EXISTS(sym_table->scope_top->scope, ident);
}

/*=============
  INSERT & FIND
  =============*/

symt_stat_t symt_insert(symt_t *sym_table, const char *ident, symt_symbol_t *symbol) {
	if (sym_table == NULL || ident == NULL || symbol == NULL)
		return SYMTSTAT_BAD_ARG;

	if (sym_table->scope_top == NULL)
		return SYMTSTAT_NO_SCOPE;

	/* Insert the symbol into the scope */
	symt_scope_t *const cur_scope = sym_table->scope_top;
	symbol->uid = sym_table->uid_accum;
	sym_table->uid_accum++;

	if (HTBL_INSERT(cur_scope->scope, ident, symbol) != HTBLSTAT_OK)
		return SYMTSTAT_BAD_ALLOC;

	return SYMTSTAT_OK;
}

symt_stat_t symt_find_symbol(symt_t *sym_table, const char *ident, symt_symbol_t *out_symbol) {
	if (sym_table == NULL || ident == NULL || out_symbol == NULL)
		return SYMTSTAT_BAD_ARG;

	symt_scope_t *aux_scope_top = sym_table->scope_top;

	/* Iterate through the global scope stack, starting at the current depth */
	while (aux_scope_top != NULL) {
		if (HTBL_RETRIEVE(aux_scope_top->scope, ident, *out_symbol))
			break;

		aux_scope_top = aux_scope_top->prev;
	}
	return aux_scope_top == NULL ? SYMTSTAT_NO_SYMBOL : SYMTSTAT_OK;
}
