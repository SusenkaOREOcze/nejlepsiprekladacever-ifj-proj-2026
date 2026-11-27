#ifndef IFJ_TAC_H
#define IFJ_TAC_H

/**
 * @file tac.h
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-11-15
 * @brief Doubly linked list of three-address instructions.
 *
 * The IR generator creates TAC from the AST.
 * TAC list can later be edited by an optimizer.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "symtable_shared.h"

typedef enum TacResult { TAC_OK, TAC_INVALID_ARGUMENT, TAC_MEMORY_ERROR, TAC_NOT_FOUND } tac_result_t;

typedef enum TacOperandKind {
	TAC_OPERAND_NONE,
	TAC_OPERAND_SYMBOL,
	TAC_OPERAND_TEMP,
	TAC_OPERAND_INTEGER,
	TAC_OPERAND_DOUBLE,
	TAC_OPERAND_STRING,
	TAC_OPERAND_BOOLEAN,
	TAC_OPERAND_UNIT
} tac_operand_kind_t;

/**
 * @brief One operand used by a three-address instruction.
 *
 * Symbol and temporary operands store their identifier in value.id.
 */
typedef struct TacOperand {
	tac_operand_kind_t kind;
	symt_data_type_t type;
	union {
		uint64_t id;
		int64_t integer;
		double decimal;
		const char *string;
		bool boolean;
	} value;
} tac_operand_t;

typedef enum TacOpcode {
	TAC_FUNCTION_BEGIN,
	TAC_FUNCTION_END,
	TAC_PARAM,
	TAC_MOVE,
	TAC_POSITIVE,
	TAC_NEGATIVE,
	TAC_NOT,
	TAC_ADD,
	TAC_SUBTRACT,
	TAC_MULTIPLY,
	TAC_DIVIDE,
	TAC_CONCATENATE,
	TAC_LESS,
	TAC_LESS_EQUAL,
	TAC_GREATER,
	TAC_GREATER_EQUAL,
	TAC_EQUAL,
	TAC_NOT_EQUAL,
	TAC_AND,
	TAC_OR,
	TAC_ARG,
	TAC_CALL,
	TAC_CHECK_NONZERO,
	TAC_RETURN,
	TAC_LABEL,
	TAC_JUMP,
	TAC_JUMP_IF_FALSE
} tac_opcode_t;

/**
 * @brief Instruction data.
 *
 * The list manages its links.
 * Names and STRINGS are list owned copies.
 * Callers must not modify an instruction returned by tac_first() or tac_last().
 * CALL uses name, dst, and src1 for the argument count.
 * PARAM uses dst; ARG, RETURN, and CHECK_NONZERO use src1. Jumps use label.
 */
typedef struct TacInstruction {
	struct TacInstruction *prev;
	struct TacInstruction *next;
	tac_opcode_t opcode;
	tac_operand_t dst;
	tac_operand_t src1;
	tac_operand_t src2;
	uint64_t label;
	const char *name;
} tac_instruction_t;

/**
 * @brief Opaque TAC DLL.
 */
typedef struct Tac tac_t;

/**
 * @brief Allocates an empty TAC instruction list.
 *
 * The output pointer is left unchanged if allocation fails.
 * @param[out] out Receives the newly created list.
 * @return TAC_OK on success, otherwise an error code.
 */
tac_result_t tac_create(tac_t **out);

/**
 * @brief Frees the list and sets its pointer to NULL.
 *
 * The function accepts a NULL pointer or a pointer containing NULL.
 * @param[in,out] list List to destroy.
 */
void tac_destroy(tac_t **list);

/**
 * @brief Copies an instruction to the end of the list.
 *
 * @param list List receiving the instruction.
 * @param instruction Instruction data to copy.
 * @return TAC_OK on success, otherwise an error code.
 */
tac_result_t tac_append(tac_t *list, const tac_instruction_t *instruction);

/**
 * @brief Copies an instruction to the beginning of the list.
 *
 * @param list List receiving the instruction.
 * @param instruction Instruction data to copy.
 * @return TAC_OK on success, otherwise an error code.
 */
tac_result_t tac_prepend(tac_t *list, const tac_instruction_t *instruction);

/**
 * @brief Copies an instruction immediately before a node in the list.
 *
 * @param list List receiving the instruction.
 * @param position Existing instruction that will follow the inserted one.
 * @param instruction Instruction data to copy.
 * @return TAC_OK on success, otherwise an error code.
 */
tac_result_t tac_insert_before(tac_t *list, const tac_instruction_t *position,
                               const tac_instruction_t *instruction);

/**
 * @brief Copies an instruction immediately after a node in the list.
 *
 * @param list List receiving the instruction.
 * @param position Existing instruction that will precede the inserted one.
 * @param instruction Instruction data to copy.
 * @return TAC_OK on success, otherwise an error code.
 */
tac_result_t tac_insert_after(tac_t *list, const tac_instruction_t *position,
                              const tac_instruction_t *instruction);

/**
 * @brief Replaces an instruction in the list with a copied instruction.
 *
 * The old instruction is freed, so its position pointer becomes invalid.
 * @param list List containing the instruction.
 * @param position Instruction to replace.
 * @param instruction Replacement instruction data to copy.
 * @return TAC_OK on success, otherwise an error code.
 */
tac_result_t tac_replace(tac_t *list, const tac_instruction_t *position,
                         const tac_instruction_t *instruction);

/**
 * @brief Removes and frees an instruction from the list.
 *
 * The position pointer becomes invalid after successful removal.
 * @param list List containing the instruction.
 * @param position Instruction to remove.
 * @return TAC_OK on success, otherwise an error code.
 */
tac_result_t tac_remove(tac_t *list, const tac_instruction_t *position);

/**
 * @brief Returns the first instruction in the list.
 *
 * The returned pointer is read-only and remains valid until list mutation.
 * @param list List to inspect.
 * @return First instruction, or NULL when the list is empty or NULL.
 */
const tac_instruction_t *tac_first(const tac_t *list);

/**
 * @brief Returns the last instruction in the list.
 *
 * The returned pointer is read-only and remains valid until list mutation.
 * @param list List to inspect.
 * @return Last instruction, or NULL when the list is empty or NULL.
 */
const tac_instruction_t *tac_last(const tac_t *list);

/**
 * @brief Returns the number of instructions in the list.
 *
 * A NULL list is treated as an empty list.
 * @param list List to inspect.
 * @return Number of instructions stored in the list.
 */
size_t tac_count(const tac_t *list);

#endif /* IFJ_TAC_H */
