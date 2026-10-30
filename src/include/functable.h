/**
   @file functable.h
   @author xbosakd00 (DasBean125)
   @date 2026-10-25

   @brief Public interface for the functable module.
*/

#ifndef FUNCTABLE_H
#define FUNCTABLE_H

#include "symtable_shared.h"

#include <stddef.h>
#include <stdint.h>

/*======================
  DATA TYPES & CONSTANTS
  ======================*/

/**
   A symbol from the function table.

   Slightly more involved, but do not be afraid, the code doesn't bite... not physically anyway.
*/
typedef struct FunctableSymbol {
	symt_data_type_t ret_type;
	int param_count; //!< If <0, undefined, otherwise specifies the number of function parameters.
	int8_t *def_param_type_array;
	int8_t *call_param_type_array;
} funct_symbol_t; // sizeof(funct_symbol_t) = 24

/**
   The function table type. Analogous to the symt_t type, but actually simpler :-).

   @note This structure has state initialized by @ref funct_init, so deal with it accordingly.
*/
typedef struct Functable funct_t;

/* Maybe remove these? I don't know right now */
static constexpr bool IS_FUNC_CALL = false;
static constexpr bool IS_FUNC_DEF = true;

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
   Destroy any signs that a function table existed in memory. This is safe to call when
   @ref funct_pending_insert is true.

   @param func_table The function table to free from memory.
   @returns SYMTSTAT_OK on success, or an error.
*/
symt_stat_t funct_free(funct_t *func_table);

/**
   Begin inserting a new function identifier into a function table.

   @param table The table into which to insert.
   @returns SYMTSTAT_OK on success, SYMTSTAT_BAD_STATE if an insertion is already pending, or an error.
*/
symt_stat_t funct_insert_begin(funct_t *table);

/**
   Add a parameter type to the currently inserted function. See @ref funct_insert_begin.

   @param table The table in which to work.
   @param param_type The type of the inserted function's parameter.
   @param is_def Specifies if the inserted symbol will be a function call or a function definition.
   @returns SYMTSTAT_OK on success, SYMTSTAT_BAD_STATE if no insertion is pending, or an error.
*/
symt_stat_t funct_insert_add_param(funct_t *table, const symt_data_type_t param_type, const bool is_def);

/**
   Finish inserting a function symbol (and leave the insertion state).

   @param table The table on which to operate.
   @param ident The function's identifier.
   @param ret The function's return type.
   @param is_def Specifies whether to insert a function definition symbol (true) or call symbol (false).

   @returns SYMTSTAT_OK if no symbol of the same name exists, otherwise:
       + SYMTSTAT_FUNC_BAD_PARAM_CNT if the existing symbol's parameter count differs,
       + SYMTSTAT_FUNC_DIFF_CALL_SIGN if the existing symbol's call signature differs and no definition
       signature exists,
       + SYMTSTAT_FUNC_DIFF_DEF_SIGN if the existing symbol's definition signature differs;
       .
   otherwise if this is a function call symbol (is_def == false):
       + SYMTSTAT_FUNC_BAD_RETURN if the existing symbol's return type differs and isn't SYMT_TYPE_UNDEFINED;
       .
   otherwise if this is a function definition symbol (is_def == true):
       + SYMTSTAT_BAD_ARG if a definition already exists (redefinition),
       + SYMTSTAT_FUNC_BAD_RETURN if the existing symbol's return type differs or if the inserted type is
       SYMT_TYPE_UNDEFINED;
       .
    otherwise SYMTSTAT_OK. Unless this function ends with any other unmentioned error (such as *_BAD_ARG),
    it attempts to update an existing symbol as much as possible (without overwriting existing data).
*/
symt_stat_t funct_insert_end(funct_t *table, const char *ident, const symt_data_type_t ret,
                             const bool is_def);

/**
   A predicate to test if a symbol insertion is currently in progress.

   @param table The table to work on.
   @returns true if an insertion is in progress, false otherwise.
*/
bool funct_pending_insert(const funct_t *table);

/**
   A predicate to ascertain the existence of a function with a given name.

   @param table The table to look through.
   @param ident The sought-after function's name.
   @returns true if the function exists, false otherwise (including when an error occurs).
*/
bool funct_exists(const funct_t *table, const char *ident);

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
   @param[out] out_type The variable into which to store the parameter type.
   @returns SYMTSTAT_OK on success, SYMTSTAT_FUNC_NO_PARAMS if the symbol has no parameters, or an error.
*/
symt_stat_t funct_nth_param_type(const funct_symbol_t *func_symbol, const int param_number,
                                 symt_data_type_t *out_type);

#endif
