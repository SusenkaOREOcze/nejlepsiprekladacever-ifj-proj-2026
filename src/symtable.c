/**
   @file symtable.c
   @author xbosakd00 (DasBean125)
   @date 2026-10-19

   @brief Symbol table module.
*/

#include "include/symtable_impl.h"

constexpr size_t SYMT_INITIAL_SCOPE_SIZE = 64;
constexpr size_t SYMT_INITIAL_STANDBY_SCOPES = 10; /* Note that at least one (global) scope is always
                                                    * allocated, no matter the value here */

/*================
  Helper functions
  ================*/

symt_stat_t symt_alloc_scope(symt_scope_t **const scope, const size_t scope_size) {
	if (scope == NULL || scope_size == 0)
		return SYMTSTAT_BAD_ARG;

	/* Allocate a new scope (stack element) */
	symt_scope_t *new_scope = malloc(sizeof(*new_scope));
	if (new_scope == NULL)
		return SYMTSTAT_BAD_ALLOC;

	/* Allocate the hashtable of that scope */
	if (htbl_init(new_scope->scope, scope_size) != HTBLSTAT_OK) {
		free(new_scope);
		return SYMTSTAT_BAD_ALLOC;
	}

	new_scope->prev = NULL;
	*scope = new_scope; /* Giving the caller the scope pointer */
	return SYMTSTAT_OK;
}

inline symt_stat_t symt_push_scope(symt_scope_t **const stack_top, symt_scope_t *scope) {
	if (stack_top == NULL || scope == NULL)
		return SYMTSTAT_BAD_ARG;

	scope->prev = *stack_top; /* Remember the old top elemenet */
	(*stack_top) = scope;     /* Move the top onto the new element */
	return SYMTSTAT_OK;
}

inline symt_stat_t symt_pop_scope(symt_scope_t **const stack_top, symt_scope_t **const scope) {
	if (stack_top == NULL || scope == NULL)
		return SYMTSTAT_BAD_ARG;

	if (*stack_top == NULL)
		return SYMTSTAT_NO_SCOPE;

	(*scope) = *stack_top;       /* Save the top element */
	*stack_top = (*scope)->prev; /* Move stack top to the next element */
	return SYMTSTAT_OK;
}

inline void symt_free_scope(symt_scope_t *scope) {
	if (scope == NULL)
		return;
	htbl_free(scope->scope);
	free(scope);
}

symt_stat_t symt_clear_scope(symt_scope_t *const scope) {
	if (scope == NULL)
		return SYMTSTAT_BAD_ARG;

	htbl_clear(scope->scope);
	return SYMTSTAT_OK;
}

/*====================
  Symtable init & free
  ====================*/

symt_stat_t symt_init(symt_t *sym_table) {
	if (sym_table == NULL)
		return SYMTSTAT_BAD_ARG;
	symt_stat_t result = SYMTSTAT_OK;

	/* Initialize an empty symbol table */
	sym_table->scope_top = NULL;
	sym_table->standby_top = NULL;
	sym_table->scope_depth = 0;
	sym_table->uid_accum = 0;

	/* Add the global scope onto the main stack */
	symt_scope_t *new_global_scope = NULL;
	result = symt_alloc_scope(&new_global_scope, SYMT_INITIAL_SCOPE_SIZE);
	if (result != SYMTSTAT_OK)
		goto FAIL_ALLOC; /* Scary goto statement */

	result = symt_push_scope(&sym_table->scope_top, new_global_scope);
	if (result != SYMTSTAT_OK)
		goto FAIL_PUSH;

	/* Add the initial scope count to the standby stack */
	for (size_t i = 0; i < SYMT_INITIAL_STANDBY_SCOPES; i++) {
		symt_scope_t *new_scope = NULL;
		result = symt_alloc_scope(&new_scope, SYMT_INITIAL_SCOPE_SIZE);
		if (result != SYMTSTAT_OK)
			goto FAIL_ALLOC;

		result = symt_push_scope(&sym_table->standby_top, new_scope);
		if (result != SYMTSTAT_OK)
			goto FAIL_PUSH;
	}
	return result;

FAIL_PUSH:
	fprintf(stderr, "symt_init: failed to push scope onto stack\n");
	return result;

FAIL_ALLOC:
	/* If more than 1 allocation fails, this frees all the allocated memory thus far before exitting */
	while (sym_table->standby_top != NULL) {
		symt_scope_t *cur_scope = sym_table->standby_top;
		sym_table->standby_top = cur_scope->prev; /* Descend down the stack */
		symt_free_scope(cur_scope);
	}

	if (sym_table->scope_top != NULL) {
		symt_free_scope(sym_table->scope_top);
		sym_table->scope_top = NULL;
	}
	return result;
}

symt_stat_t symt_free(symt_t *sym_table) {
	if (sym_table == NULL)
		return SYMTSTAT_BAD_ARG;

	/* Free the active scope stack */
	while (sym_table->scope_top != NULL) {
		symt_scope_t *cur_scope = sym_table->scope_top;
		sym_table->scope_top = cur_scope->prev; /* Descend down the stack */
		symt_free_scope(cur_scope);
	}

	/* Same shit, different stack */
	while (sym_table->standby_top != NULL) {
		symt_scope_t *cur_scope = sym_table->standby_top;
		sym_table->standby_top = cur_scope->prev;
		symt_free_scope(cur_scope);
	}

	sym_table->scope_depth = 0;
	sym_table->uid_accum = 0;
	return SYMTSTAT_OK;
}

/*===============================
  Symtable scope depth adjustment
  ===============================*/

symt_stat_t symt_inc_scope_depth(symt_t *sym_table) {
	if (sym_table == NULL)
		return SYMTSTAT_BAD_ARG;
	symt_stat_t result = SYMTSTAT_OK;

	/* Moving a scope from the standby stack onto the global scope stack, like this:
	   ... <- scope_0  <--MOVE--  standby_0 -> ...
	    scope_top^                  ^standby_top
	*/
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

	/* Doing the inverse of symt_inc_scope_depth(), so just go there and flip the arrow in the comment */
	symt_scope_t *cur_global_scope = NULL;
	result = symt_pop_scope(&sym_table->scope_top, &cur_global_scope);
	symt_clear_scope(cur_global_scope); /* This is very important */
	result = symt_push_scope(&sym_table->standby_top, cur_global_scope);
	sym_table->scope_depth--;

	return result;
}

/*===================================
  Symtable insert and find operations
  ===================================*/

symt_stat_t symt_insert(symt_t *sym_table, const char *ident, symt_symbol_t *symbol) {
	if (sym_table == NULL || ident == NULL || symbol == NULL)
		return SYMTSTAT_BAD_ARG;

	if (sym_table->scope_top == NULL)
		return SYMTSTAT_NO_SCOPE;

	/* Insert the symbol into the scope */
	symt_scope_t *const cur_scope = sym_table->scope_top;
	symbol->uid = sym_table->uid_accum;
	sym_table->uid_accum++;

	if (htbl_insert(cur_scope->scope, ident, symbol) != HTBLSTAT_OK)
		return SYMTSTAT_BAD_ALLOC;

	return SYMTSTAT_OK;
}

symt_stat_t symt_find_symbol(symt_t *sym_table, const char *ident, symt_symbol_t *out_symbol) {
	if (sym_table == NULL || ident == NULL || out_symbol == NULL)
		return SYMTSTAT_BAD_ARG;

	symt_scope_t *aux_scope_top = sym_table->scope_top;

	/* Iterate through the global scope stack, starting at the current depth */
	while (aux_scope_top != NULL) {
	    if (htbl_retrieve(aux_scope_top->scope, ident, *out_symbol))
		break;

	    aux_scope_top = aux_scope_top->prev;
	}
	return aux_scope_top == NULL ? SYMTSTAT_NO_SYMBOL : SYMTSTAT_OK;
}
