/**
   @file codegen_shared.h
   @author xbosakd00 (DasBean125)
   @date 2026-11-24
   @brief Shared functions and data structures for the optimizer and code generator modules.

   Named "codegen_shared" because the code generator is the more important guy in this club.
*/

#ifndef IFJ_CODEGEN_SHARED_H
#define IFJ_CODEGEN_SHARED_H

#include "tac.h"
#include <stdio.h>

/**
   @brief Determine whether the opcode is an assignment instruction or not.
   @note This relies on tac_opcode_t having a _single_ linear range of these instructions in its definition.
   @param opcode The opcode to test.
   @returns true if the opcode is an assignment instruction, otherwise false.
*/
inline bool tac_is_assignment_opcode(const tac_opcode_t opcode) {
	return (opcode >= TAC_MOVE && opcode <= TAC_OR);
}

/**
   @brief Determine whether the operand type is a symbol (TEMP or SYMBOL).
*/
inline bool tac_is_symbol_operand(const tac_operand_kind_t op) {
	return (op == TAC_OPERAND_TEMP || op == TAC_OPERAND_SYMBOL);
}

/**
   @brief Determine whether the operand type is a literal or not.
   @note This relies on tac_operand_kind_t having a _single_ linear range of these literals in its definition.
   @param op The operand type to test.
   @returns true if the operand is a literal (an integer, double, etc.), otherwise false.
*/
inline bool tac_is_literal_operand(const tac_operand_kind_t op) {
	return (op >= TAC_OPERAND_INTEGER && op <= TAC_OPERAND_UNIT);
}

/**
   @brief Determine whether the operand type is a numeric literal (INTEGER or DOUBLE).
*/
inline bool tac_is_numeric_literal_operand(const tac_operand_kind_t op) {
	return (op == TAC_OPERAND_INTEGER || op == TAC_OPERAND_DOUBLE);
}

/**
   @brief Determine whether the opcode is a goto (branch, jump, indirection, whatever) instruction.
   @param opcode The opcode to test.
   @returns true if opcode is a jump, false otherwise.
*/
inline bool tac_is_goto(const tac_opcode_t opcode) {
	return (opcode == TAC_JUMP || opcode == TAC_JUMP_IF_FALSE || opcode == TAC_CALL || opcode == TAC_RETURN ||
	        opcode == TAC_FUNCTION_BEGIN || opcode == TAC_FUNCTION_END);
}

/**
   @brief Create a string identifier from an operand's numerical identifier and type.

   @param[out] dst The destination string. Remains unmodified if this function fails.
   @param maxlen The capacity of the string. Should be at least 23 chars.
   @param operand The operand whose identifier to convert.
   @param internal Adds an 'i' to the operand name, indicating that it's an internal operand.
   @returns true on success with dst receiving the identifier string, or false when arguments are invalid,
   when snprintf fails (returns <0) or when attempting to write more characters than the capacity of the
   string. In the last case, dst is still written to and is guaranteed to be null-terminated.
*/
inline bool make_operand_ident(char *dst, const size_t maxlen, const tac_operand_t *operand, bool internal) {
	if (dst == NULL || maxlen == 0 || operand == NULL)
		return false;

	/* Internal temps: ti#, normal syms: {s,t}# */
	char *fmt = (internal ? "%ci%lu" : "%c%lu");
	int written_chars =
	    snprintf(dst, maxlen, fmt, (operand->kind == TAC_OPERAND_SYMBOL) ? 's' : 't', operand->value.id);
	return written_chars >= 0 && (size_t)written_chars < maxlen;
}

/* Extern definitions for the module that calls for it (see codegen.c) */
#ifdef IFJ_CODEGEN_EXTERN_PROVIDER

extern inline bool tac_is_assignment_opcode(const tac_opcode_t opcode);
extern inline bool tac_is_symbol_operand(const tac_operand_kind_t op);
extern inline bool tac_is_literal_operand(const tac_operand_kind_t op);
extern inline bool tac_is_numeric_literal_operand(const tac_operand_kind_t op);
extern inline bool tac_is_goto(const tac_opcode_t opcode);
extern inline bool make_operand_ident(char *dst, const size_t maxlen, const tac_operand_t *operand,
                                      bool internal);

#endif /* IFJ_CODEGEN_EXTERN_PROVIDER defined */

#endif
