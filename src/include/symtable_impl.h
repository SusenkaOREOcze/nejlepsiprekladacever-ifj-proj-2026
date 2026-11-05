/**
   @file symtable_impl.h
   @author xbosakd00 (DasBean125)
   @date 2026-10-19

   @brief Private interface for the symbol table data structure.
*/

#ifndef SYMTABLE_IMPL_H
#define SYMTABLE_IMPL_H

#include "hashtable.h"
#include "symtable.h"

#include <stdio.h>
#include <stdlib.h>

/*==========
  DATA TYPES
  ==========*/

/* Can we appreciate how simple this is for a second? */
DEFTYPE_HASHTABLE(symt_symbol_t) symt_search_table_t;

/**
   A single element from the symbol table stack. The element in question is a hash table.
*/
typedef struct SymtableScope {
	symt_search_table_t scope;
	struct SymtableScope *prev; /* Stack goes from top to bottom in my head, cope */
} symt_scope_t;

/**
   The table of symbols (or should I say symbol scopes?).
*/
struct Symtable {
	symt_scope_t *scope_top; //!< This is simultaneously the current scope.
	symt_scope_t *standby_top;
	size_t scope_depth;
	unsigned long long uid_accum;
};

/*=========
  FUNCTIONS
  =========*/

/**
   Erase the contents of a given scope.

   @param scope The scope to apply class B amnestics to.
   @returns SYMTSTAT_OK on success or SYMTSTAT_BAD_ARG if the arguments don't make sense.
 */
symt_stat_t symt_clear_scope(symt_scope_t *const scope);

/**
   Allocate a new scope (hash table) and return a pointer to it.

   @param scope A pointer to the variable that shall hold the new scope's address.
   @param scope_size The initial size of the new scope (how many identifiers it can hold).
   @returns SYMTSTAT_OK on success, SYMTSTAT_BAD_ARG if the arguments don't make sense or SYMTSTAT_BAD_ALLOC
   on allocation failure.
*/
symt_stat_t symt_alloc_scope(symt_scope_t **const scope, const size_t scope_size);

/**
   Push a scope onto the given scope stack. This is essentially just a linked list 'insert' function.

   @param stack_top The address of the pointer to the top of the stack.
   @param scope The scope to push onto the stack.
   @returns SYMTSTAT_OK on successful push or SYMTSTAT_BAD_ARG if the arguments don't make sense.
*/
inline symt_stat_t symt_push_scope(symt_scope_t **const stack_top, symt_scope_t *scope) {
	if (stack_top == NULL || scope == NULL)
		return SYMTSTAT_BAD_ARG;

	scope->prev = *stack_top; /* Remember the old top elemenet */
	(*stack_top) = scope;     /* Move the top onto the new element */
	return SYMTSTAT_OK;
}

/**
   Pop a scope off of the given scope stack and save its address.

   @param stack_top The address of the pointer to the top of the stack.
   @param[out] scope An address of a variable in which to store the popped scope address.
   @returns SYMTSTAT_OK on success, SYMTSTAT_NO_SCOPE if the stack is empty or SYMTSTAT_BAD_ARG if the
   arguments don't make sense.
 */
inline symt_stat_t symt_pop_scope(symt_scope_t **const stack_top, symt_scope_t **const scope) {
	if (stack_top == NULL || scope == NULL)
		return SYMTSTAT_BAD_ARG;

	if (*stack_top == NULL)
		return SYMTSTAT_NO_SCOPE;

	(*scope) = *stack_top;           /* Save the top element */
	*stack_top = (*stack_top)->prev; /* Move stack top to the next (lower) element */
	(*scope)->prev = NULL;		 /* Circumvent trolling */
	return SYMTSTAT_OK;
}

/**
   Free a given scope from memory.

   @param scope The scope to remove from memory. If NULL, this does nothing.
*/
inline void symt_free_scope(symt_scope_t *scope) {
	if (scope == NULL)
		return;
	HTBL_FREE(scope->scope);
	free(scope);
}

#ifndef UNIT_UNDER_TEST

/* In case of missing -O2 flag or some other goofy compilation stuff */
extern inline symt_stat_t symt_push_scope(symt_scope_t **const stack_top, symt_scope_t *scope);
extern inline symt_stat_t symt_pop_scope(symt_scope_t **const stack_top, symt_scope_t **const scope);
extern inline void symt_free_scope(symt_scope_t *scope);

#endif /* UNIT_UNDER_TEST not defined */

#endif /* symtable.h included */
