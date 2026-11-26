/**
 * @file test_irgen.c
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-11-08
 * @brief CMocka unit tests for AST-to-TAC generation.
 */

#include "irgen.h"
#include "cmocka/cmocka.h"

/**
 * @brief Creates one AST node.
 * @param kind AST kind.
 * @param type Resolved type.
 * @param id Resolved symbol ID.
 * @param text Optional name.
 * @return Unattached node.
 */
static ast_node_t *node(ast_node_kind_t kind, symt_data_type_t type, uint64_t id, char *text) {
	ast_node_t source = {.kind = kind, .data_type = type, .symbol_id = id, .text = text};
	ast_node_t *copy = NULL;
	assert_int_equal(ast_node_create(&source, &copy), AST_RESULT_OK);
	return copy;
}

/**
 * @brief Appends a child to a parent.
 * @param parent Parent node.
 * @param child Unattached child.
 */
static void add(ast_node_t *parent, ast_node_t *child) {
	assert_int_equal(ast_node_add_child(parent, child), AST_RESULT_OK);
}

/**
 * @brief Creates a program with one function and returns its body.
 * @param type Function return type.
 * @param[out] body Receives the body node.
 * @return Owned AST tree.
 */
static ast_t *program(symt_data_type_t type, ast_node_t **body) {
	ast_t *tree = NULL;
	ast_node_t *root = node(AST_NODE_PROGRAM, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *fn = node(AST_NODE_FUNCTION, type, AST_UNRESOLVED_SYMBOL, "work");
	*body = node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	add(fn, *body);
	add(root, fn);
	assert_int_equal(ast_create(&tree), AST_RESULT_OK);
	assert_int_equal(ast_set_root(tree, root), AST_RESULT_OK);
	return tree;
}

/**
 * @brief Creates an integer literal node.
 * @param number Literal value.
 * @return Unattached node.
 */
static ast_node_t *integer(int64_t number) {
	ast_node_t *literal = node(AST_NODE_INTEGER, SYMT_TYPE_INTEGER, AST_UNRESOLVED_SYMBOL, NULL);
	literal->value.integer = number;
	return literal;
}

/**
 * @brief Returns the instruction at a zero-based index.
 * @param code TAC list.
 * @param index Instruction index.
 * @return Instruction at that position.
 */
static const tac_instruction_t *at(const tac_t *code, size_t index) {
	const tac_instruction_t *it = tac_first(code);
	while (index-- && it)
		it = it->next;
	assert_non_null(it);
	return it;
}

/**
 * @brief Rejects missing inputs and keeps the output pointer unchanged.
 *
 * @par Input
 * @code
 * Empty AST with no PROGRAM root.
 * @endcode
 * @par Expected result
 * @code
 * IRGEN_INVALID_AST; no TAC list.
 * @endcode
 */
static void test_invalid_arguments(void **state) {
	(void)state;
	tac_t *code = NULL;
	ast_t *tree = NULL;
	assert_int_equal(irgen_generate(NULL, &code), IRGEN_INVALID_ARGUMENT);
	assert_int_equal(ast_create(&tree), AST_RESULT_OK);
	assert_int_equal(irgen_generate(tree, NULL), IRGEN_INVALID_ARGUMENT);
	assert_int_equal(irgen_generate(tree, &code), IRGEN_INVALID_AST);
	assert_null(code);
	ast_destroy(&tree);
}

/**
 * @brief An empty Unit function has a Unit return between begin and end.
 *
 * @par Source
 * @code
 * def work(): Unit = {}
 * @endcode
 * @par Expected TAC
 * @code
 * FUNCTION_BEGIN work
 * RETURN Unit
 * FUNCTION_END work
 * @endcode
 */
static void test_empty_unit_function(void **state) {
	(void)state;
	ast_node_t *body;
	ast_t *tree = program(SYMT_TYPE_UNIT, &body);
	tac_t *code = NULL;
	assert_int_equal(irgen_generate(tree, &code), IRGEN_OK);
	assert_int_equal(tac_count(code), 3);
	assert_int_equal(at(code, 0)->opcode, TAC_FUNCTION_BEGIN);
	assert_int_equal(at(code, 1)->opcode, TAC_RETURN);
	assert_int_equal(at(code, 1)->src1.kind, TAC_OPERAND_UNIT);
	assert_int_equal(at(code, 2)->opcode, TAC_FUNCTION_END);
	tac_destroy(&code);
	ast_destroy(&tree);
}

/**
 * @brief A final expression becomes the function's implicit return value.
 *
 * @par Source
 * @code
 * def work(): Int = 2 + 3
 * @endcode
 * @par Expected TAC
 * @code
 * FUNCTION_BEGIN work
 * t1 = ADD 2, 3
 * RETURN t1
 * FUNCTION_END work
 * @endcode
 */
static void test_implicit_expression_return(void **state) {
	(void)state;
	ast_node_t *body;
	ast_t *tree = program(SYMT_TYPE_INTEGER, &body);
	ast_node_t *sum = node(AST_NODE_ADD, SYMT_TYPE_INTEGER, AST_UNRESOLVED_SYMBOL, NULL);
	add(sum, integer(2));
	add(sum, integer(3));
	add(body, sum);
	tac_t *code = NULL;
	assert_int_equal(irgen_generate(tree, &code), IRGEN_OK);
	assert_int_equal(tac_count(code), 4);
	assert_int_equal(at(code, 1)->opcode, TAC_ADD);
	assert_int_equal(at(code, 1)->src1.value.integer, 2);
	assert_int_equal(at(code, 1)->src2.value.integer, 3);
	assert_int_equal(at(code, 2)->opcode, TAC_RETURN);
	assert_int_equal(at(code, 2)->src1.value.id, at(code, 1)->dst.value.id);
	tac_destroy(&code);
	ast_destroy(&tree);
}

/**
 * @brief A declaration moves a literal to its resolved symbol ID.
 *
 * @par Source
 * @code
 * def work(): Unit = { val x: Int = 9 }
 * @endcode
 * @par Expected TAC
 * @code
 * FUNCTION_BEGIN work
 * s42 = MOVE 9
 * RETURN Unit
 * FUNCTION_END work
 * @endcode
 */
static void test_declaration_uses_symbol_id(void **state) {
	(void)state;
	ast_node_t *body;
	ast_t *tree = program(SYMT_TYPE_UNIT, &body);
	ast_node_t *decl = node(AST_NODE_VAL_DECLARATION, SYMT_TYPE_INTEGER, 42, "x");
	add(decl, integer(9));
	add(body, decl);
	tac_t *code = NULL;
	assert_int_equal(irgen_generate(tree, &code), IRGEN_OK);
	assert_int_equal(at(code, 1)->opcode, TAC_MOVE);
	assert_int_equal(at(code, 1)->dst.kind, TAC_OPERAND_SYMBOL);
	assert_int_equal(at(code, 1)->dst.value.id, 42);
	assert_int_equal(at(code, 1)->src1.value.integer, 9);
	tac_destroy(&code);
	ast_destroy(&tree);
}

/**
 * @brief An unresolved declaration is rejected without returning partial TAC.
 *
 * @par Input
 * @code
 * def work(): Unit = { val x: Int = 1 }
 * x has no resolved symbol ID.
 * @endcode
 * @par Expected result
 * @code
 * IRGEN_INVALID_AST; partial TAC is discarded.
 * @endcode
 */
static void test_reject_unresolved_symbol(void **state) {
	(void)state;
	ast_node_t *body;
	ast_t *tree = program(SYMT_TYPE_UNIT, &body);
	ast_node_t *decl = node(AST_NODE_VAL_DECLARATION, SYMT_TYPE_INTEGER, AST_UNRESOLVED_SYMBOL, "x");
	add(decl, integer(1));
	add(body, decl);
	tac_t *code = NULL;
	assert_int_equal(irgen_generate(tree, &code), IRGEN_INVALID_AST);
	assert_null(code);
	ast_destroy(&tree);
}

/**
 * @brief A while loop tests its condition again after the body.
 *
 * @par Source
 * @code
 * def work(): Unit = { while true do {} }
 * @endcode
 * @par Expected TAC (loop body is empty)
 * @code
 * L1:
 * JUMP_IF_FALSE true, L2
 * JUMP L1
 * L2:
 * @endcode
 */
static void test_while_branch_shape(void **state) {
	(void)state;
	ast_node_t *body;
	ast_t *tree = program(SYMT_TYPE_UNIT, &body);
	ast_node_t *while_node = node(AST_NODE_WHILE, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *truth = node(AST_NODE_BOOLEAN, SYMT_TYPE_BOOLEAN, AST_UNRESOLVED_SYMBOL, NULL);
	truth->value.boolean = true;
	add(while_node, truth);
	add(while_node, node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL));
	add(body, while_node);
	tac_t *code = NULL;
	assert_int_equal(irgen_generate(tree, &code), IRGEN_OK);
	assert_int_equal(at(code, 1)->opcode, TAC_LABEL);
	assert_int_equal(at(code, 2)->opcode, TAC_JUMP_IF_FALSE);
	assert_int_equal(at(code, 3)->opcode, TAC_JUMP);
	assert_int_equal(at(code, 3)->label, at(code, 1)->label);
	assert_int_equal(at(code, 4)->label, at(code, 2)->label);
	tac_destroy(&code);
	ast_destroy(&tree);
}

/**
 * @brief The right side of AND is placed behind a conditional branch.
 *
 * @par Source
 * @code
 * def work(): Boolean = false && side_effect()
 * @endcode
 * @par Expected TAC
 * @code
 * JUMP_IF_FALSE false, L1
 * t2 = CALL side_effect, 0
 * t1 = MOVE t2
 * JUMP L2
 * L1:
 * t1 = MOVE false
 * L2:
 * RETURN t1
 * @endcode
 */
static void test_and_short_circuits(void **state) {
	(void)state;
	ast_node_t *body;
	ast_t *tree = program(SYMT_TYPE_BOOLEAN, &body);
	ast_node_t *and_node = node(AST_NODE_AND, SYMT_TYPE_BOOLEAN, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *first = node(AST_NODE_BOOLEAN, SYMT_TYPE_BOOLEAN, AST_UNRESOLVED_SYMBOL, NULL);
	first->value.boolean = false;
	add(and_node, first);
	add(and_node, node(AST_NODE_CALL, SYMT_TYPE_BOOLEAN, AST_UNRESOLVED_SYMBOL, "side_effect"));
	add(body, and_node);
	tac_t *code = NULL;
	assert_int_equal(irgen_generate(tree, &code), IRGEN_OK);
	assert_int_equal(at(code, 1)->opcode, TAC_JUMP_IF_FALSE);
	assert_int_equal(at(code, 2)->opcode, TAC_CALL);
	assert_int_equal(at(code, 2)->dst.kind, TAC_OPERAND_TEMP);
	tac_destroy(&code);
	ast_destroy(&tree);
}

/**
 * @brief Call arguments are emitted in order and one symbol is frozen first.
 *
 * @par Source
 * @code
 * def work(): Unit = { print(x, 8) }
 * @endcode
 * @par Expected TAC
 * @code
 * t1 = MOVE s7
 * ARG t1
 * ARG 8
 * CALL print, 2
 * @endcode
 */
static void test_call_argument_order(void **state) {
	(void)state;
	ast_node_t *body;
	ast_t *tree = program(SYMT_TYPE_UNIT, &body);
	ast_node_t *call_node = node(AST_NODE_CALL, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, "print");
	add(call_node, node(AST_NODE_IDENTIFIER, SYMT_TYPE_INTEGER, 7, "x"));
	add(call_node, integer(8));
	add(body, call_node);
	tac_t *code = NULL;
	assert_int_equal(irgen_generate(tree, &code), IRGEN_OK);
	assert_int_equal(at(code, 1)->opcode, TAC_MOVE);
	assert_int_equal(at(code, 1)->src1.value.id, 7);
	assert_int_equal(at(code, 2)->opcode, TAC_ARG);
	assert_int_equal(at(code, 3)->opcode, TAC_ARG);
	assert_int_equal(at(code, 3)->src1.value.integer, 8);
	assert_int_equal(at(code, 4)->opcode, TAC_CALL);
	assert_int_equal(at(code, 4)->src1.value.integer, 2);
	tac_destroy(&code);
	ast_destroy(&tree);
}

/**
 * @brief A symbol on the left is captured before a right-hand call executes.
 *
 * @par Source
 * @code
 * def work(): Int = x + change_x()
 * @endcode
 * @par Expected TAC
 * @code
 * t1 = MOVE s3
 * t2 = CALL change_x, 0
 * t3 = ADD t1, t2
 * RETURN t3
 * @endcode
 */
static void test_binary_freezes_left_symbol(void **state) {
	(void)state;
	ast_node_t *body;
	ast_t *tree = program(SYMT_TYPE_INTEGER, &body);
	ast_node_t *sum = node(AST_NODE_ADD, SYMT_TYPE_INTEGER, AST_UNRESOLVED_SYMBOL, NULL);
	add(sum, node(AST_NODE_IDENTIFIER, SYMT_TYPE_INTEGER, 3, "x"));
	add(sum, node(AST_NODE_CALL, SYMT_TYPE_INTEGER, AST_UNRESOLVED_SYMBOL, "change_x"));
	add(body, sum);
	tac_t *code = NULL;
	assert_int_equal(irgen_generate(tree, &code), IRGEN_OK);
	assert_int_equal(at(code, 1)->opcode, TAC_MOVE);
	assert_int_equal(at(code, 1)->src1.value.id, 3);
	assert_int_equal(at(code, 2)->opcode, TAC_CALL);
	assert_int_equal(at(code, 3)->opcode, TAC_ADD);
	assert_int_equal(at(code, 3)->src1.kind, TAC_OPERAND_TEMP);
	assert_int_equal(at(code, 3)->src1.value.id, at(code, 1)->dst.value.id);
	tac_destroy(&code);
	ast_destroy(&tree);
}

/**
 * @brief Both value-producing if branches write to one returned temporary.
 *
 * @par Source
 * @code
 * def work(): Int = if true then 1 else 2
 * @endcode
 * @par Expected TAC
 * @code
 * JUMP_IF_FALSE true, L1
 * t1 = MOVE 1
 * JUMP L2
 * L1:
 * t1 = MOVE 2
 * L2:
 * RETURN t1
 * @endcode
 */
static void test_if_expression_joins_values(void **state) {
	(void)state;
	ast_node_t *body;
	ast_t *tree = program(SYMT_TYPE_INTEGER, &body);
	ast_node_t *choice = node(AST_NODE_IF, SYMT_TYPE_INTEGER, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *condition = node(AST_NODE_BOOLEAN, SYMT_TYPE_BOOLEAN, AST_UNRESOLVED_SYMBOL, NULL);
	condition->value.boolean = true;
	ast_node_t *yes = node(AST_NODE_BLOCK, SYMT_TYPE_INTEGER, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *no = node(AST_NODE_BLOCK, SYMT_TYPE_INTEGER, AST_UNRESOLVED_SYMBOL, NULL);
	add(yes, integer(1));
	add(no, integer(2));
	add(choice, condition);
	add(choice, yes);
	add(choice, no);
	add(body, choice);
	tac_t *code = NULL;
	assert_int_equal(irgen_generate(tree, &code), IRGEN_OK);
	const tac_instruction_t *first_move = NULL;
	const tac_instruction_t *second_move = NULL;
	for (const tac_instruction_t *it = tac_first(code); it; it = it->next)
		if (it->opcode == TAC_MOVE) {
			if (!first_move)
				first_move = it;
			else
				second_move = it;
		}
	assert_non_null(first_move);
	assert_non_null(second_move);
	assert_int_equal(first_move->dst.value.id, second_move->dst.value.id);
	assert_int_equal(tac_last(code)->prev->src1.value.id, first_move->dst.value.id);
	tac_destroy(&code);
	ast_destroy(&tree);
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_invalid_arguments),
		cmocka_unit_test(test_empty_unit_function),
	    cmocka_unit_test(test_implicit_expression_return),
		cmocka_unit_test(test_declaration_uses_symbol_id),
	    cmocka_unit_test(test_reject_unresolved_symbol),
		cmocka_unit_test(test_while_branch_shape),
	    cmocka_unit_test(test_and_short_circuits),
		cmocka_unit_test(test_call_argument_order),
	    cmocka_unit_test(test_binary_freezes_left_symbol),
		cmocka_unit_test(test_if_expression_joins_values),
	};
	return cmocka_run_group_tests_name("IR generator", tests, NULL, NULL);
}
