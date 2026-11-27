/**
 * @file tac.c
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-11-15
 * @brief Double linked list (DLL) for THE three-address code.
 */

// Not a Dynamically Linked Library

#include "include/tac.h"

#include <stdlib.h>
#include <string.h>

struct Tac {
	tac_instruction_t *first;
	tac_instruction_t *last;
	size_t count;
};

/**
 * @brief Copies a string.
 *
 * @param source Null-terminated string to copy, or NULL.
 * @return Allocated copy, or NULL when input is NULL or allocation fails.
 */
static char *copy_string(const char *source) {
	if (source == NULL)
		return NULL;

	size_t size = strlen(source) + 1;
	char *copy = malloc(size);
	if (copy != NULL)
		memcpy(copy, source, size);
	return copy;
}

/**
 * @brief Releases an instruction and its owned strings.
 *
 * The function accepts NULL.
 * @param instruction Instruction to release.
 */
static void release_instruction(tac_instruction_t *instruction) {
	if (instruction == NULL)
		return;

	free((void *)instruction->name);
	if (instruction->dst.kind == TAC_OPERAND_STRING)
		free((void *)instruction->dst.value.string);
	if (instruction->src1.kind == TAC_OPERAND_STRING)
		free((void *)instruction->src1.value.string);
	if (instruction->src2.kind == TAC_OPERAND_STRING)
		free((void *)instruction->src2.value.string);
	free(instruction);
}

/**
 * @brief Copies an operand and duplicates its string data when present.
 *
 * A string operand without a string value is rejected.
 * @param[out] out Receives the copied operand.
 * @param source Operand to copy.
 * @return TAC_OK on success, otherwise an error code.
 */
static tac_result_t copy_operand(tac_operand_t *out, const tac_operand_t *source) {
	*out = *source;
	if (source->kind != TAC_OPERAND_STRING)
		return TAC_OK;

	if (source->value.string == NULL) {
		out->kind = TAC_OPERAND_NONE;
		return TAC_INVALID_ARGUMENT;
	}

	out->value.string = copy_string(source->value.string);
	return out->value.string == NULL ? TAC_MEMORY_ERROR : TAC_OK;
}

/**
 * @brief Creates a copy of instruction data.
 *
 * List links from the source are not copied.
 *
 * @param source Instruction to copy.
 * @param[out] out Receives the allocated instruction copy.
 * @return TAC_OK on success, otherwise an error code.
 */
static tac_result_t make_instruction(const tac_instruction_t *source, tac_instruction_t **out) {
	if (source == NULL || (unsigned)source->opcode > TAC_JUMP_IF_FALSE ||
	    (unsigned)source->dst.kind > TAC_OPERAND_UNIT || (unsigned)source->src1.kind > TAC_OPERAND_UNIT ||
	    (unsigned)source->src2.kind > TAC_OPERAND_UNIT)
		return TAC_INVALID_ARGUMENT;

	tac_instruction_t *copy = calloc(1, sizeof(*copy));
	if (copy == NULL)
		return TAC_MEMORY_ERROR;

	copy->opcode = source->opcode;
	copy->label = source->label;
	copy->is_entrypoint = source->is_entrypoint; /* this was missing */

	if (source->name != NULL) {
		copy->name = copy_string(source->name);
		if (copy->name == NULL) {
			release_instruction(copy);
			return TAC_MEMORY_ERROR;
		}
	}

	tac_result_t result = copy_operand(&copy->dst, &source->dst);

	if (result == TAC_OK)
		result = copy_operand(&copy->src1, &source->src1);
	if (result == TAC_OK)
		result = copy_operand(&copy->src2, &source->src2);
	if (result != TAC_OK) {
		release_instruction(copy);
		return result;
	}
	*out = copy;
	return TAC_OK;
}

/**
 * @brief Checks whether an instruction belongs to a list.
 *
 * A NULL position is never considered a list member.
 * @param list List to search.
 * @param instruction Instruction to locate.
 * @return true when position is in list, otherwise false.
 */
static bool contains(const tac_t *list, const tac_instruction_t *instruction) {
	if (instruction == NULL)
		return false;

	for (const tac_instruction_t *it = list->first; it != NULL; it = it->next)
		if (it == instruction)
			return true;
	return false;
}

/**
 * @brief Inserts a instruction between two list nodes.
 *
 * Updates both neighboring links and the list endpoints as needed.
 * @param list List receiving the instruction.
 * @param left Previous instruction, or NULL for the new first node.
 * @param right Next instruction, or NULL for the new last node.
 * @param node Detached instruction to link into the list.
 */
static void link_between(tac_t *list, tac_instruction_t *left, tac_instruction_t *right,
                         tac_instruction_t *node) {
	node->prev = left;
	node->next = right;

	if (left != NULL)
		left->next = node;
	else
		list->first = node;
	if (right != NULL)
		right->prev = node;
	else
		list->last = node;
	++list->count;
}

tac_result_t tac_create(tac_t **out) {
	if (out == NULL)
		return TAC_INVALID_ARGUMENT;

	tac_t *list = calloc(1, sizeof(*list));
	if (list == NULL)
		return TAC_MEMORY_ERROR;
	*out = list;
	return TAC_OK;
}

void tac_destroy(tac_t **list) {
	if (list == NULL || *list == NULL)
		return;

	tac_instruction_t *it = (*list)->first;
	while (it != NULL) {
		tac_instruction_t *next = it->next;
		release_instruction(it);
		it = next;
	}
	free(*list);
	*list = NULL;
}

tac_result_t tac_insert_before(tac_t *list, const tac_instruction_t *position,
                               const tac_instruction_t *instruction) {
	if (list == NULL || instruction == NULL)
		return TAC_INVALID_ARGUMENT;
	if (!contains(list, position))
		return TAC_NOT_FOUND;
	if (list->count == SIZE_MAX)
		return TAC_MEMORY_ERROR;

	tac_instruction_t *copy;
	tac_result_t result = make_instruction(instruction, &copy);
	if (result != TAC_OK)
		return result;

	link_between(list, position->prev, (tac_instruction_t *)position, copy);
	return TAC_OK;
}

tac_result_t tac_insert_after(tac_t *list, const tac_instruction_t *position,
                              const tac_instruction_t *instruction) {
	if (list == NULL || instruction == NULL)
		return TAC_INVALID_ARGUMENT;
	if (!contains(list, position))
		return TAC_NOT_FOUND;
	if (list->count == SIZE_MAX)
		return TAC_MEMORY_ERROR;

	tac_instruction_t *copy;
	tac_result_t result = make_instruction(instruction, &copy);
	if (result != TAC_OK)
		return result;

	link_between(list, (tac_instruction_t *)position, position->next, copy);
	return TAC_OK;
}

tac_result_t tac_append(tac_t *list, const tac_instruction_t *instruction) {
	if (list == NULL || instruction == NULL)
		return TAC_INVALID_ARGUMENT;
	if (list->count == SIZE_MAX)
		return TAC_MEMORY_ERROR;

	tac_instruction_t *copy;
	tac_result_t result = make_instruction(instruction, &copy);
	if (result != TAC_OK)
		return result;

	link_between(list, list->last, NULL, copy);
	return TAC_OK;
}

tac_result_t tac_prepend(tac_t *list, const tac_instruction_t *instruction) {
	if (list == NULL || instruction == NULL)
		return TAC_INVALID_ARGUMENT;
	if (list->count == SIZE_MAX)
		return TAC_MEMORY_ERROR;

	tac_instruction_t *copy;
	tac_result_t result = make_instruction(instruction, &copy);
	if (result != TAC_OK)
		return result;

	link_between(list, NULL, list->first, copy);
	return TAC_OK;
}

tac_result_t tac_replace(tac_t *list, const tac_instruction_t *position,
                         const tac_instruction_t *instruction) {
	if (list == NULL || instruction == NULL)
		return TAC_INVALID_ARGUMENT;
	if (!contains(list, position))
		return TAC_NOT_FOUND;

	tac_instruction_t *copy;
	tac_result_t result = make_instruction(instruction, &copy);
	if (result != TAC_OK)
		return result;

	// You should read this again
	tac_instruction_t *node = (tac_instruction_t *)position;
	copy->prev = node->prev;
	copy->next = node->next;
	if (node->prev != NULL)
		node->prev->next = copy;
	else
		list->first = copy;
	if (node->next != NULL)
		node->next->prev = copy;
	else
		list->last = copy;

	release_instruction(node);
	return TAC_OK;
}

tac_result_t tac_remove(tac_t *list, const tac_instruction_t *position) {
	if (list == NULL)
		return TAC_INVALID_ARGUMENT;
	if (!contains(list, position))
		return TAC_NOT_FOUND;

	// This is better then the thing before
	tac_instruction_t *node = (tac_instruction_t *)position;
	if (node->prev != NULL)
		node->prev->next = node->next;
	else
		list->first = node->next;
	if (node->next != NULL)
		node->next->prev = node->prev;
	else
		list->last = node->prev;
	--list->count;

	release_instruction(node);
	return TAC_OK;
}

// some other super important really highend complex inovative functionz

const tac_instruction_t *tac_first(const tac_t *list) { return list == NULL ? NULL : list->first; }

const tac_instruction_t *tac_last(const tac_t *list) { return list == NULL ? NULL : list->last; }

size_t tac_count(const tac_t *list) { return list == NULL ? 0 : list->count; }
