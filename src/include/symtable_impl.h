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

   @note Because this uses the hashtable.c module, it can be inferred that this scope is automatically
   inflatable.
*/
typedef struct SymtableScope {
	symt_search_table_t scope;
	struct SymtableScope *prev; /* Stack goes from top to bottom in my head, cope */
} symt_scope_t;

/**
   The table of symbols (or should I say symbol scopes?).

   This is implemented as a stack of hash tables (which act as search tables). The stack is implemented as a
   linked list. There is also a secondary stack of "standby tables" - these are pre-allocated empty hash
   tables ready for immediate use.
*/
struct Symtable {
	symt_scope_t *scope_top;   //!< Top of the active scope stack. This is simultaneously the current scope.
	symt_scope_t *standby_top; //!< Top of the standby scope stack.
	size_t scope_depth;
	unsigned long long uid_accum;
};

/*=========
  FUNCTIONS
  =========*/

#ifndef TEST_SYMTABLE
/**
   Erase the contents of a given scope.

   @param scope The scope to apply class B amnestics to.
   @returns SYMTSTAT_OK on success, otherwise an error.
 */
symt_stat_t symt_clear_scope(symt_scope_t *const scope);

/**
   Allocate a new scope (hash table) and return a pointer to it.

   @note If this function fails, the value of 'scope' is left unchanged.

   @param scope A pointer to the variable that shall hold the new scope's address.
   @param scope_size The initial size of the new scope (how many identifiers it can hold).
   @returns SYMTSTAT_OK on success, or an error on failure.
*/
symt_stat_t symt_alloc_scope(symt_scope_t **const scope, const size_t scope_size);

/**
   Push a scope onto the given scope stack. This is essentially just a linked list 'insert' function.

   @param stack_top The address of the pointer to the top of the stack.
   @param scope The scope to push onto the stack.
   @returns SYMTSTAT_OK on successful push, otherwise an error.
*/
inline symt_stat_t symt_push_scope(symt_scope_t **const stack_top, symt_scope_t *scope);

/**
   Pop a scope off of the given scope stack and save its address. This is essentially a linked list 'pop'
   function combined with a 'first' function.

   @param stack_top The address of the pointer to the top of the stack.
   @param[out] scope An address of a variable in which to store the popped scope address.
   @returns SYMTSTAT_OK on success, SYMTSTAT_NO_SCOPE if the stack is empty, or an error.
 */
inline symt_stat_t symt_pop_scope(symt_scope_t **const stack_top, symt_scope_t **const scope);

/**
   Free a given scope from memory.

   @param scope The scope to remove from memory. If NULL, this does nothing.
*/
inline void symt_free_scope(symt_scope_t *scope);

/* In case of missing -O2 flag or some other goofy compilation stuff */
extern inline symt_stat_t symt_push_scope(symt_scope_t **const stack_top, symt_scope_t *scope);
extern inline symt_stat_t symt_pop_scope(symt_scope_t **const stack_top, symt_scope_t **const scope);
extern inline void symt_free_scope(symt_scope_t *scope);

#endif /* TEST_SYMTABLE no defined */

#endif /* symtable.h included */
