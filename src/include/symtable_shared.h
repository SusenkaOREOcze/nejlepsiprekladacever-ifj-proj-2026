/**
   @file symtable_shared.h
   @author xbosakd00 (DasBean125)
   @date 2026-10-25

   @brief Shared definitions between the functable.c and symtable.c modules.
*/

#ifndef SYMTABLE_SHARED_H
#define SYMTABLE_SHARED_H

/**
   A set of status codes pertinent to the symbol table data structure.

   You're an excellent programmer, so you'll probably only need the first code from this list.
*/
typedef enum SymtableStatusCode {
	SYMTSTAT_OK = 0,
	SYMTSTAT_BAD_ARG,
	SYMTSTAT_BAD_ALLOC,
	SYMTSTAT_BAD_STATE,
	SYMTSTAT_NO_SYMBOL,
	SYMTSTAT_NO_SCOPE,

	SYMTSTAT_FUNC_BAD_RETURN, /* 6 */
	SYMTSTAT_FUNC_BAD_PARAM_CNT,
	SYMTSTAT_FUNC_DIFF_CALL_SIGN,
	SYMTSTAT_FUNC_DIFF_DEF_SIGN,
	SYMTSTAT_FUNC_NO_PARAMS,
	SYMTSTAT_FUNC_REDEFINITION,
} symt_stat_t;

/**
   Identifies what kind of data a symbol shall hold.
*/
typedef enum SymtableDataType {
	SYMT_TYPE_UNIT,
	SYMT_TYPE_INTEGER,
	SYMT_TYPE_DOUBLE,
	SYMT_TYPE_STRING,
	SYMT_TYPE_BOOLEAN,
	SYMT_TYPE_UNDEFINED,
} symt_data_type_t;

#endif
