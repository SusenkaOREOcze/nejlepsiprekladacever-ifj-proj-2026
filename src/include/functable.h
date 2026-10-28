/**
   @file functable.h
   @author xbosakd00 (DasBean125)
   @date 2026-10-25

   @brief Public interface for the functable module.

   Why is this not a part of the symtable.c module?
   1. Nobody told us that we can't do it like this, and clearly, what's not forbidden is allowed,
   2. The function table is different enough from the symbol table that this separation felt logical to us.
   Also to follow up on the question from @ref "symtable.h": this is the actual reason why hashtable is its
   own module.
*/

#ifndef FUNCTABLE_H
#define FUNCTABLE_H

#include "symtable_shared.h"

#include <stddef.h>
#include <stdint.h>

/*==========
  DATA TYPES
  ==========*/

/**
   A symbol from the function table.

   Slightly more involved, but do not be afraid, the code doesn't bite... not physically anyway.
*/
typedef struct FunctableSymbol {
    symt_data_type_t ret_type;
    size_t param_count;
    int8_t *def_param_type_array;
    int8_t *call_param_type_array;
} funct_symbol_t;

/**
   The function table type. Analogous to the symt_t type, but actually simpler :-].

   @note Once again, this structure has state initialized by @ref funct_init, so deal with it accordingly.
*/
typedef struct Functable funct_t;

/*====================
  DATA TYPE OPERATIONS
  ====================*/

/**
   Allocate and initialize a function symbol table in memory.

   @note On allocation failure, func_table is left unchanged.

   @param[out] func_table A pointer to the variable that'll store the table's address.
   @returns SYMTSTAT_OK on success, or an error.
*/
symt_stat_t funct_init(funct_t **const func_table);

/**
   Destroy any signs that a function table existed in memory.

   @param func_table The function table to free from memory.
   @returns SYMTSTAT_OK on success, or an error.
*/
symt_stat_t funct_free(funct_t *func_table);

/**
   Begin inserting a new function identifier into a function table.

   @param table The table into which to insert.
   @param ident The to-be-added function's identifier.
   @param ret The function's return type.
   @param is_def Specifies if the inserted symbol is a function call or a function definition.
   @returns SYMTSTAT_OK on success, SYMTSTAT_BAD_STATE if an insertion is already pending, SYMTSTAT_BAD_ARG if
   the given symbol already exists, or an error.
*/
symt_stat_t funct_insert_begin(funct_t *table, const char *ident, const symt_data_type_t ret,
                               const bool is_def);

/**
   Add a parameter type to the currently inserted function. See @ref funct_insert_begin.

   @param table The table in which to work.
   @param param_type The type of the inserted function's parameter.
   @returns SYMTSTAT_OK on success, SYMTSTAT_BAD_STATE if no insertion is pending, or an error.
*/
symt_stat_t funct_insert_add_param(funct_t *table, const symt_data_type_t param_type);

/**
   Finish inserting a function symbol (and leave the insertion state).

   @param table The table on which to operate.
   @returns SYMTSTAT_OK on success, SYMTSTAT_BAD_STATE if no insertion is pending, or an error.
*/
symt_stat_t funct_insert_end(funct_t *table);

/**
   A predicate to test if a symbol insertion is currently in progress.

   @param table The table to work on.
   @returns true if an insertion is in progress, false otherwise.
*/
inline bool funct_pending_insert(const funct_t *table);

/**
   A predicate to ascertain the existence of a function with a given name.

   @param table The table to look through.
   @param ident The sought-after function's name.
   @returns true if the function exists, false otherwise (including when an error occurs).
*/
inline bool funct_exists(const funct_t *table, const char *ident);

/**
   Find a function symbol with the given identifier and return its data.

   @param table The table to search through.
   @param ident The identifier of the sought-after symbol (the name of the function which you so adamantly
   crave).
   @param out_symbol The address of the variable into which the found symbol shall be copied.
   @returns SYMTSTAT_OK on success, SYMTSTAT_NO_SYMBOL if the identifier doesn't exist, or an error.
*/
symt_stat_t funct_find_symbol(const funct_t *table, const char *ident, funct_symbol_t *out_symbol);

/**
   Return the type data type of the nth parameter of a given function symbol.

   @param func_symbol A specific function symbol, as returned by one of the find functions.
   @param param_number The index of the function's parameter, from left to right, beginning at 0.
   @returns The data type of a parameter (see @ref symt_data_type_t). On failure, returns SYMT_TYPE_UNDEFINED.
*/
symt_data_type_t funct_nth_param_type(const funct_symbol_t *func_symbol, const size_t param_number);

/* When the compiler is suspicious */
extern inline bool funct_pending_insert(const funct_t *table);
extern inline bool funct_exists(const funct_t *table, const char *ident);

#endif
