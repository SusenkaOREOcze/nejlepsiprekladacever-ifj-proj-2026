/**
 * @file test_ast_irgen_tac.c
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-11-08
 * @brief Integration tests of AST, IR generation, and TAC.
 */

#include "irgen.h"
#include "cmocka/cmocka.h"

#include <inttypes.h>
#include <stdio.h>

/**
 * @brief Creates an AST node with resolved metadata.
 * @param kind Node kind.
 * @param type Resolved type.
 * @param id Symbol ID.
 * @param text Optional text.
 * @return New unattached node.
 */
static ast_node_t *make_node(ast_node_kind_t kind, symt_data_type_t type, uint64_t id, char *text) {
	ast_node_t source = {.kind = kind, .data_type = type, .symbol_id = id, .text = text};
	ast_node_t *copy = NULL;
	assert_int_equal(ast_node_create(&source, &copy), AST_RESULT_OK);
	return copy;
}

/**
 * @brief Attaches one child.
 * @param parent Parent node.
 * @param child Unattached child.
 */
static void child(ast_node_t *parent, ast_node_t *child) {
	assert_int_equal(ast_node_add_child(parent, child), AST_RESULT_OK);
}

/**
 * @brief Creates an integer literal.
 * @param value Integer value.
 * @return New literal node.
 */
static ast_node_t *number(int64_t value) {
	ast_node_t *n = make_node(AST_NODE_INTEGER, SYMT_TYPE_INTEGER, AST_UNRESOLVED_SYMBOL, NULL);
	n->value.integer = value;
	return n;
}

/**
 * @brief Wraps a function in a complete program tree.
 * @param fn Function node.
 * @return Owning AST.
 */
static ast_t *tree_with(ast_node_t *fn) {
	ast_t *tree = NULL;
	ast_node_t *root = make_node(AST_NODE_PROGRAM, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	child(root, fn);
	assert_int_equal(ast_create(&tree), AST_RESULT_OK);
	assert_int_equal(ast_set_root(tree, root), AST_RESULT_OK);
	return tree;
}

/**
 * @brief Prints one TAC operand in a readable form.
 *
 * @param operand Operand to print.
 */
static void print_operand(const tac_operand_t *operand) {
	switch (operand->kind) {
	case TAC_OPERAND_NONE:
		fputs("_", stdout);
		break;
	case TAC_OPERAND_SYMBOL:
		printf("s%" PRIu64, operand->value.id);
		break;
	case TAC_OPERAND_TEMP:
		printf("t%" PRIu64, operand->value.id);
		break;
	case TAC_OPERAND_INTEGER:
		printf("%" PRId64, operand->value.integer);
		break;
	case TAC_OPERAND_DOUBLE:
		printf("%.17g", operand->value.decimal);
		break;
	case TAC_OPERAND_STRING:
		putchar('"');
		for (const unsigned char *it = (const unsigned char *)operand->value.string; it && *it; ++it) {
			switch (*it) {
			case '\\': fputs("\\\\", stdout); break;
			case '"': fputs("\\\"", stdout); break;
			case '\n': fputs("\\n", stdout); break;
			case '\r': fputs("\\r", stdout); break;
			case '\t': fputs("\\t", stdout); break;
			default:
				if (*it < 0x20 || *it == 0x7f)
					printf("\\x%02X", (unsigned int)*it);
				else
					putchar(*it);
			}
		}
		putchar('"');
		break;
	case TAC_OPERAND_BOOLEAN:
		fputs(operand->value.boolean ? "true" : "false", stdout);
		break;
	case TAC_OPERAND_UNIT:
		fputs("Unit", stdout);
		break;
	default:
		fputs("<?>", stdout);
		break;
	}
}

/**
 * @brief Returns the text of an arithmetic, comparison, or unary operation.
 *
 * @param opcode Operator opcode.
 * @return Name printed between the operands.
 */
static const char *operator_name(tac_opcode_t opcode) {
	switch (opcode) {
	case TAC_POSITIVE: return "POSITIVE";
	case TAC_NEGATIVE: return "NEGATIVE";
	case TAC_NOT: return "NOT";
	case TAC_ADD: return "ADD";
	case TAC_SUBTRACT: return "SUBTRACT";
	case TAC_MULTIPLY: return "MULTIPLY";
	case TAC_DIVIDE: return "DIVIDE";
	case TAC_CONCATENATE: return "CONCATENATE";
	case TAC_LESS: return "LESS";
	case TAC_LESS_EQUAL: return "LESS_EQUAL";
	case TAC_GREATER: return "GREATER";
	case TAC_GREATER_EQUAL: return "GREATER_EQUAL";
	case TAC_EQUAL: return "EQUAL";
	case TAC_NOT_EQUAL: return "NOT_EQUAL";
	case TAC_AND: return "AND";
	case TAC_OR: return "OR";
	default: return "UNKNOWN";
	}
}

/**
 * @brief Prints a TAC list as one three-address instruction per line.
 *
 * @param title Heading identifying the test and the list state.
 * @param code List to print.
 */
static void print_tac(const char *title, const tac_t *code) {
	printf("\n--- %s (%zu instructions) ---\n", title, tac_count(code));
	size_t index = 0;
	for (const tac_instruction_t *it = tac_first(code); it; it = it->next) {
		printf("%3zu  ", index++);
		switch (it->opcode) {
		case TAC_FUNCTION_BEGIN:
		case TAC_FUNCTION_END:
			printf("FUNCTION_%s %s", it->opcode == TAC_FUNCTION_BEGIN ? "BEGIN" : "END",
			       it->name ? it->name : "<?>");
			break;
		case TAC_PARAM:
			fputs("PARAM ", stdout);
			print_operand(&it->dst);
			break;
		case TAC_MOVE:
			print_operand(&it->dst);
			fputs(" = MOVE ", stdout);
			print_operand(&it->src1);
			break;
		case TAC_ARG:
		case TAC_RETURN:
		case TAC_CHECK_NONZERO:
			fputs(it->opcode == TAC_ARG ? "ARG " : it->opcode == TAC_RETURN ? "RETURN " : "CHECK_NONZERO ", stdout);
			print_operand(&it->src1);
			break;
		case TAC_CALL:
			if (it->dst.kind != TAC_OPERAND_NONE) {
				print_operand(&it->dst);
				fputs(" = ", stdout);
			}
			printf("CALL %s, ", it->name ? it->name : "<?>");
			print_operand(&it->src1);
			break;
		case TAC_LABEL:
			printf("L%" PRIu64 ":", it->label);
			break;
		case TAC_JUMP:
			printf("JUMP L%" PRIu64, it->label);
			break;
		case TAC_JUMP_IF_FALSE:
			fputs("JUMP_IF_FALSE ", stdout);
			print_operand(&it->src1);
			printf(", L%" PRIu64, it->label);
			break;
		default:
			print_operand(&it->dst);
			printf(" = %s ", operator_name(it->opcode));
			print_operand(&it->src1);
			if (it->opcode != TAC_POSITIVE && it->opcode != TAC_NEGATIVE && it->opcode != TAC_NOT) {
				fputs(", ", stdout);
				print_operand(&it->src2);
			}
			break;
		}
		putchar('\n');
	}
	fflush(stdout);
}

/**
 * @brief Parameter IDs survive AST copying and appear in PARAM, ADD, and RETURN.
 *
 * @par Source
 * @code
 * def sum(left: Int, right: Int): Int = left + right
 * @endcode
 */
static void test_function_parameters_to_tac(void **state) {
	(void)state;
	ast_node_t *fn = make_node(AST_NODE_FUNCTION, SYMT_TYPE_INTEGER, AST_UNRESOLVED_SYMBOL, "sum");
	child(fn, make_node(AST_NODE_PARAMETER, SYMT_TYPE_INTEGER, 10, "left"));
	child(fn, make_node(AST_NODE_PARAMETER, SYMT_TYPE_INTEGER, 11, "right"));
	ast_node_t *body = make_node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *sum = make_node(AST_NODE_ADD, SYMT_TYPE_INTEGER, AST_UNRESOLVED_SYMBOL, NULL);
	child(sum, make_node(AST_NODE_IDENTIFIER, SYMT_TYPE_INTEGER, 10, "left"));
	child(sum, make_node(AST_NODE_IDENTIFIER, SYMT_TYPE_INTEGER, 11, "right"));
	child(body, sum);
	child(fn, body);
	ast_t *tree = tree_with(fn);
	tac_t *code = NULL;
	assert_int_equal(irgen_generate(tree, &code), IRGEN_OK);
	const tac_instruction_t *it = tac_first(code);
	assert_int_equal(it->opcode, TAC_FUNCTION_BEGIN);
	assert_string_equal(it->name, "sum");
	it = it->next;
	assert_int_equal(it->opcode, TAC_PARAM);
	assert_int_equal(it->dst.value.id, 10);
	it = it->next;
	assert_int_equal(it->opcode, TAC_PARAM);
	assert_int_equal(it->dst.value.id, 11);
	it = it->next;
	assert_int_equal(it->opcode, TAC_MOVE);
	assert_int_equal(it->src1.value.id, 10);
	uint64_t captured_id = it->dst.value.id;
	it = it->next;
	assert_int_equal(it->opcode, TAC_ADD);
	assert_int_equal(it->src1.value.id, captured_id);
	assert_int_equal(it->src2.value.id, 11);
	uint64_t result_id = it->dst.value.id;
	it = it->next;
	assert_int_equal(it->opcode, TAC_RETURN);
	assert_int_equal(it->src1.value.id, result_id);
	assert_int_equal(tac_count(code), 7);
	ast_destroy(&tree);
	assert_string_equal(tac_first(code)->name, "sum");
	tac_destroy(&code);
}

/**
 * @brief A generated list can be edited without changing the original AST.
 *
 * @par Source
 * @code
 * @main def main(): Unit = { val unused: Int = 5 }
 * @endcode
 */
static void test_generated_tac_can_be_optimized(void **state) {
	(void)state;
	ast_node_t *fn = make_node(AST_NODE_MAIN_FUNCTION, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, "main");
	ast_node_t *body = make_node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *decl = make_node(AST_NODE_VAL_DECLARATION, SYMT_TYPE_INTEGER, 99, "unused");
	child(decl, number(5));
	child(body, decl);
	child(fn, body);
	ast_t *tree = tree_with(fn);
	tac_t *code = NULL;
	assert_int_equal(irgen_generate(tree, &code), IRGEN_OK);
	assert_int_equal(tac_count(code), 4);
	assert_int_equal(tac_remove(code, tac_first(code)->next), TAC_OK);
	assert_int_equal(tac_count(code), 3);
	assert_ptr_equal(tac_first(code)->next->prev, tac_first(code));
	assert_int_equal(ast_node_child_count(body), 1);
	tac_destroy(&code);
	ast_destroy(&tree);
}


/**
 * @brief An until range and a guard produce linked exit and advance labels.
 *
 * @par Source
 * @code
 * def iterate(): Unit =
 *     for i <- 0 until 3 if i < 2 do {}
 * @endcode
 * @par Expected TAC
 * @code
 * FUNCTION_BEGIN iterate
 * s17 = MOVE 0
 * t1 = MOVE 3
 * t2 = MOVE 1
 * L1:
 * t3 = LESS s17, t1
 * JUMP_IF_FALSE t3, L3
 * t4 = MOVE s17
 * t5 = LESS t4, 2
 * JUMP_IF_FALSE t5, L2
 * L2:
 * t6 = ADD s17, t2
 * s17 = MOVE t6
 * JUMP L1
 * L3:
 * RETURN Unit
 * FUNCTION_END iterate
 * @endcode
 */
static void test_range_guard_labels(void **state) {
	(void)state;
	ast_node_t *fn = make_node(AST_NODE_FUNCTION, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, "iterate");
	ast_node_t *body = make_node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *loop = make_node(AST_NODE_FOR, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	child(loop, make_node(AST_NODE_FOR_BINDING, SYMT_TYPE_INTEGER, 17, "i"));
	ast_node_t *range = make_node(AST_NODE_RANGE_UNTIL, SYMT_TYPE_UNDEFINED, AST_UNRESOLVED_SYMBOL, NULL);
	child(range, number(0));
	child(range, number(3));
	child(loop, range);
	ast_node_t *guard = make_node(AST_NODE_LESS, SYMT_TYPE_BOOLEAN, AST_UNRESOLVED_SYMBOL, NULL);
	child(guard, make_node(AST_NODE_IDENTIFIER, SYMT_TYPE_INTEGER, 17, "i"));
	child(guard, number(2));
	child(loop, guard);
	child(loop, make_node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL));
	child(body, loop);
	child(fn, body);
	ast_t *tree = tree_with(fn);
	tac_t *code = NULL;
	assert_int_equal(irgen_generate(tree, &code), IRGEN_OK);
	print_tac("iterate with a range guard", code);
	size_t branch_count = 0;
	size_t label_count = 0;
	for (const tac_instruction_t *it = tac_first(code); it; it = it->next) {
		if (it->opcode == TAC_LABEL)
			++label_count;
		if (it->opcode == TAC_JUMP || it->opcode == TAC_JUMP_IF_FALSE) {
			++branch_count;
			bool found = false;
			for (const tac_instruction_t *target = tac_first(code); target; target = target->next)
				if (target->opcode == TAC_LABEL && target->label == it->label)
					found = true;
			assert_true(found);
		}
	}
	assert_int_equal(label_count, 3);
	assert_int_equal(branch_count, 3);
	tac_destroy(&code);
	ast_destroy(&tree);
}

/**
 * @brief Nested blocks and conditions keep branch targets and statement order.
 *
 * @par Source
 * @code
 * @main def main(): Unit =
 *     var x: Int = 0
 *     if x < 3 then
 *         if x == 0 then
 *             x = 1
 *         else
 *             x = 2
 *     else
 *         x = 3
 * @endcode
 * @par Expected TAC
 * @code
 * FUNCTION_BEGIN main
 * s21 = MOVE 0
 * t1 = MOVE s21
 * t2 = LESS t1, 3
 * JUMP_IF_FALSE t2, L1
 * t3 = MOVE s21
 * t4 = EQUAL t3, 0
 * JUMP_IF_FALSE t4, L2
 * s21 = MOVE 1
 * JUMP L3
 * L2:
 * s21 = MOVE 2
 * L3:
 * JUMP L4
 * L1:
 * s21 = MOVE 3
 * L4:
 * RETURN Unit
 * FUNCTION_END main
 * @endcode
 */
static void test_nested_blocks_and_conditions(void **state) {
	(void)state;
	ast_node_t *fn = make_node(AST_NODE_MAIN_FUNCTION, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, "main");
	ast_node_t *body = make_node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *decl = make_node(AST_NODE_VAR_DECLARATION, SYMT_TYPE_INTEGER, 21, "x");
	child(decl, number(0));
	child(body, decl);

	ast_node_t *outer = make_node(AST_NODE_IF, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *outer_test = make_node(AST_NODE_LESS, SYMT_TYPE_BOOLEAN, AST_UNRESOLVED_SYMBOL, NULL);
	child(outer_test, make_node(AST_NODE_IDENTIFIER, SYMT_TYPE_INTEGER, 21, "x"));
	child(outer_test, number(3));
	child(outer, outer_test);
	ast_node_t *outer_yes = make_node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *nested = make_node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *inner = make_node(AST_NODE_IF, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *inner_test = make_node(AST_NODE_EQUAL, SYMT_TYPE_BOOLEAN, AST_UNRESOLVED_SYMBOL, NULL);
	child(inner_test, make_node(AST_NODE_IDENTIFIER, SYMT_TYPE_INTEGER, 21, "x"));
	child(inner_test, number(0));
	child(inner, inner_test);

	ast_node_t *inner_yes = make_node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *assign_one = make_node(AST_NODE_ASSIGNMENT, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	child(assign_one, make_node(AST_NODE_IDENTIFIER, SYMT_TYPE_INTEGER, 21, "x"));
	child(assign_one, number(1));
	child(inner_yes, assign_one);
	child(inner, inner_yes);

	ast_node_t *inner_no = make_node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *assign_two = make_node(AST_NODE_ASSIGNMENT, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	child(assign_two, make_node(AST_NODE_IDENTIFIER, SYMT_TYPE_INTEGER, 21, "x"));
	child(assign_two, number(2));
	child(inner_no, assign_two);
	child(inner, inner_no);
	child(nested, inner);
	child(outer_yes, nested);
	child(outer, outer_yes);

	ast_node_t *outer_no = make_node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *assign_three = make_node(AST_NODE_ASSIGNMENT, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	child(assign_three, make_node(AST_NODE_IDENTIFIER, SYMT_TYPE_INTEGER, 21, "x"));
	child(assign_three, number(3));
	child(outer_no, assign_three);
	child(outer, outer_no);
	child(body, outer);
	child(fn, body);
	ast_t *tree = tree_with(fn);
	tac_t *code = NULL;
	assert_int_equal(irgen_generate(tree, &code), IRGEN_OK);
	print_tac("nested blocks and conditions", code);

	const tac_opcode_t expected[] = {
	    TAC_FUNCTION_BEGIN,
	    TAC_MOVE,
	    TAC_MOVE,
	    TAC_LESS,
	    TAC_JUMP_IF_FALSE,
	    TAC_MOVE,
	    TAC_EQUAL,
	    TAC_JUMP_IF_FALSE,
	    TAC_MOVE,
	    TAC_JUMP,
	    TAC_LABEL,
	    TAC_MOVE,
	    TAC_LABEL,
	    TAC_JUMP,
	    TAC_LABEL,
	    TAC_MOVE,
	    TAC_LABEL,
	    TAC_RETURN,
	    TAC_FUNCTION_END,
	};
	const tac_instruction_t *instructions[sizeof(expected) / sizeof(expected[0])];
	size_t count = 0;
	for (const tac_instruction_t *it = tac_first(code); it; it = it->next) {
		assert_true(count < sizeof(expected) / sizeof(expected[0]));
		assert_int_equal(it->opcode, expected[count]);
		instructions[count++] = it;
	}

	assert_int_equal(count, sizeof(expected) / sizeof(expected[0]));
	assert_int_equal(instructions[4]->label, instructions[14]->label);
	assert_int_equal(instructions[7]->label, instructions[10]->label);
	assert_int_equal(instructions[9]->label, instructions[12]->label);
	assert_int_equal(instructions[13]->label, instructions[16]->label);
	assert_int_not_equal(instructions[4]->label, instructions[7]->label);
	assert_int_equal(instructions[8]->dst.value.id, 21);
	assert_int_equal(instructions[11]->src1.value.integer, 2);
	assert_int_equal(instructions[15]->src1.value.integer, 3);
	tac_destroy(&code);
	ast_destroy(&tree);
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_function_parameters_to_tac),
	    cmocka_unit_test(test_generated_tac_can_be_optimized),
	    cmocka_unit_test(test_range_guard_labels),
	    cmocka_unit_test(test_nested_blocks_and_conditions),
	};
	return cmocka_run_group_tests_name("AST-IR-TAC integration", tests, NULL, NULL);
}
