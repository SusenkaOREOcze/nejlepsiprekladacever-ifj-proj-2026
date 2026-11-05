/**
   @file symtable.h
   @author xbosakd00 (DasBean125)
   @date 2026-10-17

   @brief Public interface definition for the symbol table data structure.

   Why did you implement the hashtable as a separate, generic abstract data type, I hear you ask? Crack. The
   answer is crack.
*/

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "symtable_shared.h"

#include <stddef.h>
#include <stdint.h>

/*==========
  DATA TYPES
  ==========*/

/**
   A symbol from the symbol table. How symbolic.
*/
typedef struct SymtableSymbol {
	symt_data_type_t data_type;
	/**
	   States whether this symbol is of the IFJ26 storage class `var` (true) or `val` (false).
	*/
	bool mutable;
	/**
	   The symbol's unique identifier, to distinguish same symbol names in different scopes.
	*/
	unsigned long long uid;
} symt_symbol_t;

/**
   The actual symbol table type. This is _just_ a single table, trust (thousand-yard stare).

   @note This structure has state initialized by @ref symt_init, so deal with it accordingly.
*/
typedef struct Symtable symt_t;

/*====================
  DATA TYPE OPERATIONS
  ====================*/

/**
   Allocate and initialize a symbol table in memory. The scope depth is initialized to 0 (global).

   @param[out] sym_table A pointer to the variable that'll store the new table.
   @returns SYMTSTAT_OK on success, SYMTSTAT_BAD_ARG if arguments don't make sense or SYMTSTAT_BAD_ALLOC when
   ionizing radiation decides to flip a bit.
*/
symt_stat_t symt_init(symt_t **const sym_table);

/**
   Rid the memory of a symbol table.

   @param[out] sym_table The symbol table to destroy.
   @returns SYMTSTAT_OK on success or SYMTSTAT_BAD_ARG if the arguments don't make sense.
*/
symt_stat_t symt_free(symt_t *sym_table);

/**
   Increment the current scope depth, potentially allocating a new scope in the process.

   @note If this function returns SYMTSTAT_BAD_ALLOC, the scope depth is still increased.

   @param sym_table The symbol table to work with.
   @returns SYMTSTAT_OK on success, SYMTSTAT_BAD_ARG if the arguments don't make sense or SYMTSTAT_BAD_ALLOC
   if allocating a new scope fails.
*/
symt_stat_t symt_inc_scope_depth(symt_t *sym_table);

/**
   Decrement the current scope depth _without_ deallocating the previous scope (you paid for your memory,
   you're gonna use your memory).

   @param sym_table The symbol table to work with.
   @returns SYMTSTAT_OK on success (or when the scope depth is already 0) or SYMTSTAT_BAD_ARG if the arguments
   don't make sense.
*/
symt_stat_t symt_dec_scope_depth(symt_t *sym_table);

/**
   A predicate which shall, when prayed to sufficiently, return a truthful answer to the question "Does this
   symbol exist in my scope?".

   @param sym_table The symbol table which to inspect.
   @param ident The name of the symbol which you (most likely) hope isn't there.
   @returns true if the symbol exists, or false if the symbol doesn't exist or if the arguments are FUBAR
   (including the absense of a scope).
*/
bool symt_exists_in_current_scope(const symt_t *sym_table, const char *ident);

/**
   Insert an identifier-symbol pair into a symbol table.

   @note The uid field of the symbol is set automatically. This function has update semantics, and you know
   what that means... FISH.

   @param sym_table The symbol table to insert into.
   @param ident The identifier (string) of the symbol.
   @param[in] symbol A structure containing the symbol's data.
   @returns SYMTSTAT_OK on success, SYMTSTAT_BAD_ARG if the arguments don't make sense, SYMTSTAT_NO_SCOPE if
   there is no active scope to push into, or SYMTSTAT_BAD_ALLOC if symbol allocation explodes.
*/
symt_stat_t symt_insert(symt_t *sym_table, const char *ident, symt_symbol_t *symbol);

/**
   Find a symbol with the given identifier in a symbol table and return its data.

   @param sym_table The table to search through.
   @param ident The symbol identifier to look for.
   @param[out] out_symbol A pointer to a variable that will receive the symbol data. If this function fails,
   this variable will remain unmodified.
   @returns SYMTSTAT_OK on success, SYMTSTAT_NO_SYMBOL if the identifier has no symbol assigned or
   SYMTSTAT_BAD_ARG if the arguments don't make sense.
*/
symt_stat_t symt_find_symbol(symt_t *sym_table, const char *ident, symt_symbol_t *out_symbol);

#endif
