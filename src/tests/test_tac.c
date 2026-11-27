/**
 * @file test_tac.c
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-11-15
 * @brief CMocka tests for TAC list operations and instruction data.
 */

#include "tac.h"
#include "cmocka/cmocka.h"

// This is a function for creating, 1 isntruction.
static tac_instruction_t instruction(tac_opcode_t opcode) {
	return (tac_instruction_t){.opcode = opcode};
}

/**
 * @brief Checks empty list creation and NULL-safe destruction.
 */
static void test_lifecycle(void **state) {
	(void)state;
	tac_t *list = NULL;
	assert_int_equal(tac_create(&list), TAC_OK);
	assert_int_equal(tac_count(list), 0);
	assert_null(tac_first(list));
	assert_null(tac_last(list));
	tac_destroy(&list);
	assert_null(list);
	tac_destroy(&list);
}

/**
 * @brief Appending owns independent copies of names and string operands.
 */
static void test_deep_copy(void **state) {
	(void)state;
	tac_t *list = NULL;
	char name[] = "run";
	char text[] = "hello";

	tac_instruction_t input = instruction(TAC_MOVE);
	input.name = name;
	input.src1 = (tac_operand_t){.kind = TAC_OPERAND_STRING, .type = SYMT_TYPE_STRING, .value.string = text};

	assert_int_equal(tac_create(&list), TAC_OK);
	assert_int_equal(tac_append(list, &input), TAC_OK);
	name[0] = 'X';
	text[0] = 'X';
	assert_string_equal(tac_first(list)->name, "run");
	assert_string_equal(tac_first(list)->src1.value.string, "hello");
	tac_destroy(&list);
}

/**
 * @brief Front, back, before, after and removal preserve both links.
 * TAC: FUNCTION_BEGIN -> PARAM -> ADD -> RETURN -> FUNCTION_END.
 */
static void test_link_operations(void **state) {
	(void)state;
	tac_t *list = NULL;
	tac_instruction_t middle = instruction(TAC_ADD);
	tac_instruction_t front = instruction(TAC_FUNCTION_BEGIN);
	tac_instruction_t back = instruction(TAC_FUNCTION_END);
	tac_instruction_t before = instruction(TAC_PARAM);
	tac_instruction_t after = instruction(TAC_RETURN);

	assert_int_equal(tac_create(&list), TAC_OK);
	assert_int_equal(tac_append(list, &middle), TAC_OK);
	const tac_instruction_t *anchor = tac_first(list);
	assert_int_equal(tac_prepend(list, &front), TAC_OK);
	assert_int_equal(tac_append(list, &back), TAC_OK);
	assert_int_equal(tac_insert_before(list, anchor, &before), TAC_OK);
	assert_int_equal(tac_insert_after(list, anchor, &after), TAC_OK);
	assert_int_equal(tac_count(list), 5);
	const tac_opcode_t expected[] = {
	    TAC_FUNCTION_BEGIN, TAC_PARAM, TAC_ADD, TAC_RETURN, TAC_FUNCTION_END,
	};
	size_t index = 0;
	for (const tac_instruction_t *it = tac_first(list); it != NULL; it = it->next) {
		assert_int_equal(it->opcode, expected[index++]);
		if (it->next != NULL)
			assert_ptr_equal(it->next->prev, it);
	}
	assert_int_equal(index, 5);
	assert_int_equal(tac_remove(list, anchor), TAC_OK);
	assert_int_equal(tac_count(list), 4);
	assert_ptr_equal(tac_first(list)->next->next, tac_last(list)->prev);
	tac_destroy(&list);
}

/**
 * @brief Replacement changes data while retaining neighboring nodes.
 * TAC: ADD -> RETURN becomes MULTIPLY -> RETURN.
 */
static void test_replace(void **state) {
	(void)state;
	tac_t *list = NULL;
	tac_instruction_t first = instruction(TAC_ADD);
	tac_instruction_t second = instruction(TAC_RETURN);
	tac_instruction_t replacement = instruction(TAC_MULTIPLY);

	assert_int_equal(tac_create(&list), TAC_OK);
	assert_int_equal(tac_append(list, &first), TAC_OK);
	assert_int_equal(tac_append(list, &second), TAC_OK);

	const tac_instruction_t *tail = tac_last(list);
	assert_int_equal(tac_replace(list, tac_first(list), &replacement), TAC_OK);
	assert_int_equal(tac_first(list)->opcode, TAC_MULTIPLY);
	assert_ptr_equal(tac_first(list)->next, tail);
	assert_ptr_equal(tail->prev, tac_first(list));
	assert_int_equal(tac_count(list), 2);
	tac_destroy(&list);
}

/**
 * @brief Foreign positions and invalid operands leave lists intact.
 * TAC: the valid list remains a single ADD instruction.
 */
static void test_reject_invalid_input(void **state) {
	(void)state;
	tac_t *left = NULL, *right = NULL;
	tac_instruction_t valid = instruction(TAC_ADD);
	tac_instruction_t invalid = instruction(TAC_MOVE);

	invalid.src1.kind = TAC_OPERAND_STRING;
	assert_int_equal(tac_create(&left), TAC_OK);
	assert_int_equal(tac_create(&right), TAC_OK);
	assert_int_equal(tac_append(left, &valid), TAC_OK);
	assert_int_equal(tac_insert_after(right, tac_first(left), &valid), TAC_NOT_FOUND);
	assert_int_equal(tac_remove(right, tac_first(left)), TAC_NOT_FOUND);
	assert_int_equal(tac_append(left, &invalid), TAC_INVALID_ARGUMENT);
	assert_int_equal(tac_count(left), 1);
	assert_int_equal(tac_count(right), 0);
	tac_destroy(&left);
	tac_destroy(&right);
}

/**
 * @brief Removing the sole instruction makes an empty list.
 */
static void test_remove_only_instruction(void **state) {
	(void)state;
	tac_t *list = NULL;
	tac_instruction_t input = instruction(TAC_LABEL);
	assert_int_equal(tac_create(&list), TAC_OK);
	assert_int_equal(tac_append(list, &input), TAC_OK);
	assert_int_equal(tac_remove(list, tac_first(list)), TAC_OK);
	assert_int_equal(tac_count(list), 0);
	assert_null(tac_first(list));
	assert_null(tac_last(list));
	tac_destroy(&list);
}

/**
 * @brief Replacement copies all three string operands independently.
 * TAC: MOVE becomes CONCATENATE dst, "left", "right".
 */
static void test_replace_copies_string_operands(void **state) {
	(void)state;
	tac_t *list = NULL;
	char left[] = "left";
	char right[] = "right";
	tac_instruction_t old = instruction(TAC_MOVE);
	tac_instruction_t replacement = instruction(TAC_CONCATENATE);
	replacement.src1 = (tac_operand_t){.kind = TAC_OPERAND_STRING, .type = SYMT_TYPE_STRING, .value.string = left};
	replacement.src2 = (tac_operand_t){.kind = TAC_OPERAND_STRING, .type = SYMT_TYPE_STRING, .value.string = right};

	assert_int_equal(tac_create(&list), TAC_OK);
	assert_int_equal(tac_append(list, &old), TAC_OK);
	assert_int_equal(tac_replace(list, tac_first(list), &replacement), TAC_OK);
	left[0] = 'X';
	right[0] = 'X';
	assert_string_equal(tac_first(list)->src1.value.string, "left");
	assert_string_equal(tac_first(list)->src2.value.string, "right");
	tac_destroy(&list);
}

/**
 * @brief A failed replacement preserves the original instruction and links.
 */
static void test_replace_failure_is_atomic(void **state) {
	(void)state;
	tac_t *list = NULL;
	tac_instruction_t first = instruction(TAC_ADD);
	tac_instruction_t second = instruction(TAC_RETURN);
	tac_instruction_t invalid = instruction(TAC_MOVE);
	invalid.dst.kind = TAC_OPERAND_STRING;

	assert_int_equal(tac_create(&list), TAC_OK);
	assert_int_equal(tac_append(list, &first), TAC_OK);
	assert_int_equal(tac_append(list, &second), TAC_OK);

	const tac_instruction_t *head = tac_first(list);
	assert_int_equal(tac_replace(list, head, &invalid), TAC_INVALID_ARGUMENT);
	assert_ptr_equal(tac_first(list), head);
	assert_int_equal(tac_first(list)->opcode, TAC_ADD);
	assert_ptr_equal(head->next, tac_last(list));
	assert_int_equal(tac_count(list), 2);
	tac_destroy(&list);
}

/**
 * @brief A longer mixed insertion sequence keeps forward and reverse traversal symmetric.
 * TAC: LABEL 0 -> LABEL 1 -> ... -> LABEL 31, traversed both ways.
 */
static void test_bidirectional_traversal(void **state) {
	(void)state;
	tac_t *list = NULL;
	assert_int_equal(tac_create(&list), TAC_OK);

	for (uint64_t id = 0; id < 32; ++id) {
		tac_instruction_t ins = instruction(TAC_LABEL);
		ins.label = id;
		assert_int_equal(tac_append(list, &ins), TAC_OK);
	}

	uint64_t expected = 0;
	for (const tac_instruction_t *it = tac_first(list); it; it = it->next)
		assert_int_equal(it->label, expected++);
	assert_int_equal(expected, 32);

	expected = 32;
	for (const tac_instruction_t *it = tac_last(list); it; it = it->prev)
		assert_int_equal(it->label, --expected);
	assert_int_equal(expected, 0);
	tac_destroy(&list);
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_lifecycle),
	    cmocka_unit_test(test_deep_copy),
	    cmocka_unit_test(test_link_operations),
	    cmocka_unit_test(test_replace),
	    cmocka_unit_test(test_reject_invalid_input),
	    cmocka_unit_test(test_remove_only_instruction),
	    cmocka_unit_test(test_replace_copies_string_operands),
	    cmocka_unit_test(test_replace_failure_is_atomic),
	    cmocka_unit_test(test_bidirectional_traversal),
	};
	return cmocka_run_group_tests_name("TAC list", tests, NULL, NULL);
}
