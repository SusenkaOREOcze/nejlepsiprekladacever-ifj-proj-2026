/**
 * @file test_ast.c
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-11-08
 * @brief CMocka unit tests for the AST.
 */

#include "ast.h"
#include "cmocka/cmocka.h"

#include <stdint.h>

/**
 * @brief Creates a node with the supplied data.
 *
 * @param kind Kind of node to create.
 * @param text Optional node text.
 * @param data_type Resolved node data type.
 * @param symbol_id Resolved symbol ID or AST_UNRESOLVED_SYMBOL.
 * @return Newly allocated unattached node.
 */
static ast_node_t *create_semantic_node(ast_node_kind_t kind, char *text, symt_data_type_t data_type,
                                        uint64_t symbol_id) {
	ast_node_t source = {
	    .text = text,
	    .symbol_id = symbol_id,
	    .data_type = data_type,
	    .kind = kind,
	};
	ast_node_t *node = NULL;

	assert_int_equal(ast_node_create(&source, &node), AST_RESULT_OK);
	assert_non_null(node);

	return node;
}

/**
 * @brief Creates a node with neutral data for structural tests.
 *
 * @param kind Kind of node to create.
 * @return Newly allocated unattached node.
 */
static ast_node_t *create_node(ast_node_kind_t kind) {
	return create_semantic_node(kind, NULL, SYMT_TYPE_UNDEFINED, AST_UNRESOLVED_SYMBOL);
}

/**
 * @brief Creates a complete integer literal node.
 *
 * @param value Literal value.
 * @return Newly allocated unattached integer node.
 */
static ast_node_t *create_integer_node(int64_t value) {
	ast_node_t source = {
	    .value.integer = value,
	    .symbol_id = AST_UNRESOLVED_SYMBOL,
	    .data_type = SYMT_TYPE_INTEGER,
	    .kind = AST_NODE_INTEGER,
	};
	ast_node_t *node = NULL;

	assert_int_equal(ast_node_create(&source, &node), AST_RESULT_OK);
	assert_non_null(node);

	return node;
}

/**
 * @brief Tests creating and destroying an empty AST.
 */
static void test_ast_empty_lifecycle(void **state) {
	(void)state;
	ast_t *ast = NULL;

	assert_int_equal(ast_create(&ast), AST_RESULT_OK);
	assert_non_null(ast);
	assert_true(ast_is_empty(ast));
	assert_null(ast_root(ast));

	ast_destroy(&ast);

	assert_null(ast);
}

/**
 * @brief Tests NULL handling of the AST operations.
 */
static void test_ast_null_lifecycle(void **state) {
	(void)state;
	ast_t *ast = NULL;

	assert_int_equal(ast_create(NULL), AST_RESULT_INVALID_ARGUMENT);
	assert_true(ast_is_empty(NULL));
	assert_null(ast_root(NULL));

	ast_destroy(NULL);
	ast_destroy(&ast);

	assert_null(ast);
}

/**
 * @brief Tests transferring a program node to an empty AST.
 */
static void test_ast_accepts_program_root(void **state) {
	(void)state;
	ast_t *ast = NULL;
	ast_node_t *program = create_node(AST_NODE_PROGRAM);

	assert_int_equal(ast_create(&ast), AST_RESULT_OK);
	assert_int_equal(ast_set_root(ast, program), AST_RESULT_OK);
	assert_false(ast_is_empty(ast));
	assert_ptr_equal(ast_root(ast), program);

	ast_destroy(&ast);

	assert_null(ast);
}

/**
 * @brief Tests rejecting a non-program node as an AST root.
 */
static void test_ast_rejects_non_program_root(void **state) {
	(void)state;
	ast_t *ast = NULL;
	ast_node_t *identifier = create_node(AST_NODE_IDENTIFIER);

	assert_int_equal(ast_create(&ast), AST_RESULT_OK);
	assert_int_equal(ast_set_root(ast, identifier), AST_RESULT_INVALID_OPERATION);
	assert_true(ast_is_empty(ast));
	assert_int_equal(ast_node_destroy(&identifier), AST_RESULT_OK);

	ast_destroy(&ast);
}

/**
 * @brief Tests rejecting a second root without changing the first one.
 */
static void test_ast_rejects_second_root(void **state) {
	(void)state;
	ast_t *ast = NULL;
	ast_node_t *first = create_node(AST_NODE_PROGRAM);
	ast_node_t *second = create_node(AST_NODE_PROGRAM);

	assert_int_equal(ast_create(&ast), AST_RESULT_OK);
	assert_int_equal(ast_set_root(ast, first), AST_RESULT_OK);
	assert_int_equal(ast_set_root(ast, second), AST_RESULT_INVALID_OPERATION);
	assert_ptr_equal(ast_root(ast), first);
	assert_int_equal(ast_node_destroy(&second), AST_RESULT_OK);

	ast_destroy(&ast);
}

/**
 * @brief Tests copying every field while ignoring source tree links.
 */
static void test_node_create_copies_data_without_links(void **state) {
	(void)state;
	ast_node_t dummy_parent = {0};
	ast_node_t dummy_child = {0};
	ast_node_t dummy_sibling = {0};
	ast_node_t source = {
	    .parent = &dummy_parent,
	    .first_child = &dummy_child,
	    .next_sibling = &dummy_sibling,
	    .value.integer = -42,
	    .symbol_id = 1234,
	    .location =
	        {
	            .line = 17,
	            .column = 9,
	        },
	    .data_type = SYMT_TYPE_INTEGER,
	    .kind = AST_NODE_INTEGER,
	};
	ast_node_t *copy = NULL;

	assert_int_equal(ast_node_create(&source, &copy), AST_RESULT_OK);
	assert_null(copy->parent);
	assert_null(copy->first_child);
	assert_null(copy->next_sibling);
	assert_int_equal(copy->value.integer, -42);
	assert_int_equal(copy->symbol_id, 1234);
	assert_int_equal(copy->location.line, 17);
	assert_int_equal(copy->location.column, 9);
	assert_int_equal(copy->data_type, SYMT_TYPE_INTEGER);
	assert_int_equal(copy->kind, AST_NODE_INTEGER);

	assert_int_equal(ast_node_destroy(&copy), AST_RESULT_OK);
}

/**
 * @brief Tests that a node owns an independent copy of its text.
 */
static void test_node_create_deep_copies_text(void **state) {
	(void)state;
	char source_text[] = "variable";
	ast_node_t source = {
	    .text = source_text,
	    .symbol_id = 77,
	    .data_type = SYMT_TYPE_STRING,
	    .kind = AST_NODE_IDENTIFIER,
	};
	ast_node_t *copy = NULL;

	assert_int_equal(ast_node_create(&source, &copy), AST_RESULT_OK);
	assert_string_equal(copy->text, "variable");
	assert_true(copy->text != source_text);

	source_text[0] = 'X';

	assert_string_equal(copy->text, "variable");
	assert_int_equal(ast_node_destroy(&copy), AST_RESULT_OK);
}

/**
 * @brief Tests creating a node without optional text.
 */
static void test_node_create_accepts_missing_text(void **state) {
	(void)state;
	ast_node_t source = {
	    .text = NULL,
	    .symbol_id = AST_UNRESOLVED_SYMBOL,
	    .data_type = SYMT_TYPE_UNIT,
	    .kind = AST_NODE_BLOCK,
	};
	ast_node_t *copy = NULL;

	assert_int_equal(ast_node_create(&source, &copy), AST_RESULT_OK);
	assert_null(copy->text);
	assert_int_equal(ast_node_destroy(&copy), AST_RESULT_OK);
}

/**
 * @brief Tests rejecting invalid node creation arguments and kinds.
 */
static void test_node_create_rejects_invalid_input(void **state) {
	(void)state;
	ast_node_t invalid = {
	    .kind = AST_NODE_INVALID,
	};
	ast_node_t *node = NULL;

	assert_int_equal(ast_node_create(NULL, &node), AST_RESULT_INVALID_ARGUMENT);
	assert_int_equal(ast_node_create(&invalid, &node), AST_RESULT_INVALID_ARGUMENT);
	assert_int_equal(ast_node_create(&invalid, NULL), AST_RESULT_INVALID_ARGUMENT);
	assert_null(node);
}

/**
 * @brief Tests appending children in source order and maintaining parent links.
 */
static void test_children_keep_insertion_order(void **state) {
	(void)state;
	ast_node_t *block = create_node(AST_NODE_BLOCK);
	ast_node_t *first = create_node(AST_NODE_VAL_DECLARATION);
	ast_node_t *second = create_node(AST_NODE_ASSIGNMENT);
	ast_node_t *third = create_node(AST_NODE_RETURN);

	assert_int_equal(ast_node_add_child(block, first), AST_RESULT_OK);
	assert_int_equal(ast_node_add_child(block, second), AST_RESULT_OK);
	assert_int_equal(ast_node_add_child(block, third), AST_RESULT_OK);

	assert_ptr_equal(block->first_child, first);
	assert_ptr_equal(first->parent, block);
	assert_ptr_equal(second->parent, block);
	assert_ptr_equal(third->parent, block);
	assert_ptr_equal(first->next_sibling, second);
	assert_ptr_equal(second->next_sibling, third);
	assert_null(third->next_sibling);
	assert_int_equal(ast_node_child_count(block), 3);
	assert_ptr_equal(ast_node_child_at(block, 0), first);
	assert_ptr_equal(ast_node_child_at(block, 1), second);
	assert_ptr_equal(ast_node_child_at(block, 2), third);
	assert_null(ast_node_child_at(block, 3));

	assert_int_equal(ast_node_destroy(&block), AST_RESULT_OK);
}

/**
 * @brief Tests child access behavior for NULL nodes and empty parents.
 */
static void test_child_queries_handle_empty_input(void **state) {
	(void)state;
	ast_node_t *block = create_node(AST_NODE_BLOCK);

	assert_int_equal(ast_node_child_count(NULL), 0);
	assert_null(ast_node_child_at(NULL, 0));
	assert_int_equal(ast_node_child_count(block), 0);
	assert_null(ast_node_child_at(block, 0));

	assert_int_equal(ast_node_destroy(&block), AST_RESULT_OK);
}

/**
 * @brief Tests rejecting NULL and self-referential child attachments.
 */
static void test_add_child_rejects_invalid_arguments(void **state) {
	(void)state;
	ast_node_t *node = create_node(AST_NODE_BLOCK);

	assert_int_equal(ast_node_add_child(NULL, node), AST_RESULT_INVALID_ARGUMENT);
	assert_int_equal(ast_node_add_child(node, NULL), AST_RESULT_INVALID_ARGUMENT);
	assert_int_equal(ast_node_add_child(node, node), AST_RESULT_INVALID_OPERATION);

	assert_int_equal(ast_node_destroy(&node), AST_RESULT_OK);
}

/**
 * @brief Tests that one child cannot be owned by two parents.
 */
static void test_add_child_rejects_second_parent(void **state) {
	(void)state;
	ast_node_t *first_parent = create_node(AST_NODE_BLOCK);
	ast_node_t *second_parent = create_node(AST_NODE_BLOCK);
	ast_node_t *child = create_node(AST_NODE_RETURN);

	assert_int_equal(ast_node_add_child(first_parent, child), AST_RESULT_OK);
	assert_int_equal(ast_node_add_child(second_parent, child), AST_RESULT_INVALID_OPERATION);
	assert_ptr_equal(child->parent, first_parent);

	assert_int_equal(ast_node_destroy(&second_parent), AST_RESULT_OK);
	assert_int_equal(ast_node_destroy(&first_parent), AST_RESULT_OK);
}

/**
 * @brief Tests rejecting a cycle formed by attaching an ancestor below its descendant.
 */
static void test_add_child_rejects_cycle(void **state) {
	(void)state;
	ast_node_t *ancestor = create_node(AST_NODE_BLOCK);
	ast_node_t *descendant = create_node(AST_NODE_BLOCK);

	assert_int_equal(ast_node_add_child(ancestor, descendant), AST_RESULT_OK);
	assert_int_equal(ast_node_add_child(descendant, ancestor), AST_RESULT_INVALID_OPERATION);
	assert_null(ancestor->parent);
	assert_ptr_equal(descendant->parent, ancestor);

	assert_int_equal(ast_node_destroy(&ancestor), AST_RESULT_OK);
}

/**
 * @brief Tests that an attached child cannot be destroyed separately.
 */
static void test_destroy_rejects_attached_child(void **state) {
	(void)state;
	ast_node_t *parent = create_node(AST_NODE_BLOCK);
	ast_node_t *child = create_node(AST_NODE_RETURN);

	assert_int_equal(ast_node_add_child(parent, child), AST_RESULT_OK);
	assert_int_equal(ast_node_destroy(&child), AST_RESULT_INVALID_OPERATION);
	assert_non_null(child);
	assert_ptr_equal(child->parent, parent);

	assert_int_equal(ast_node_destroy(&parent), AST_RESULT_OK);
}

/**
 * @brief Tests that destroying an absent detached node is harmless.
 */
static void test_destroy_accepts_null_node(void **state) {
	(void)state;
	ast_node_t *node = NULL;

	assert_int_equal(ast_node_destroy(NULL), AST_RESULT_OK);
	assert_int_equal(ast_node_destroy(&node), AST_RESULT_OK);
	assert_null(node);
}

/**
 * @brief Tests the complete set of expression node kinds.
 */
static void test_expression_predicate(void **state) {
	(void)state;
	const ast_node_kind_t expression_kinds[] = {
	    AST_NODE_CALL,
		AST_NODE_IDENTIFIER,
		AST_NODE_INTEGER,
	    AST_NODE_DOUBLE,
		AST_NODE_STRING,
		AST_NODE_BOOLEAN,
	    AST_NODE_UNIT,
		AST_NODE_POSITIVE,
		AST_NODE_NEGATIVE,
	    AST_NODE_NOT,
		AST_NODE_ADD,
		AST_NODE_SUBTRACT,
	    AST_NODE_MULTIPLY,
		AST_NODE_DIVIDE,
		AST_NODE_CONCATENATE,
	    AST_NODE_LESS,
		AST_NODE_LESS_EQUAL,
		AST_NODE_GREATER,
	    AST_NODE_GREATER_EQUAL,
		AST_NODE_EQUAL,
		AST_NODE_NOT_EQUAL,
	    AST_NODE_AND,
		AST_NODE_OR,
	};

	for (size_t index = 0; index < sizeof(expression_kinds) / sizeof(expression_kinds[0]); ++index) {
		ast_node_t node = {
		    .kind = expression_kinds[index],
		};

		assert_true(ast_node_is_expression(&node));
	}

	ast_node_t block = {
	    .kind = AST_NODE_BLOCK,
	};

	assert_false(ast_node_is_expression(&block));
	assert_false(ast_node_is_expression(NULL));
}

/**
 * @brief Tests literal classification without accepting identifiers or operators.
 */
static void test_literal_predicate(void **state) {
	(void)state;
	const ast_node_kind_t literal_kinds[] = {
	    AST_NODE_INTEGER,
		AST_NODE_DOUBLE,
		AST_NODE_STRING,
		AST_NODE_BOOLEAN,
		AST_NODE_UNIT,
	};

	for (size_t index = 0; index < sizeof(literal_kinds) / sizeof(literal_kinds[0]); ++index) {
		ast_node_t node = {
		    .kind = literal_kinds[index],
		};

		assert_true(ast_node_is_literal(&node));
	}

	ast_node_t identifier = {
	    .kind = AST_NODE_IDENTIFIER,
	};
	ast_node_t addition = {
	    .kind = AST_NODE_ADD,
	};

	assert_false(ast_node_is_literal(&identifier));
	assert_false(ast_node_is_literal(&addition));
	assert_false(ast_node_is_literal(NULL));
}

/**
 * @brief Tests unary operator classification.
 */
static void test_unary_operator_predicate(void **state) {
	(void)state;
	const ast_node_kind_t unary_kinds[] = {
	    AST_NODE_POSITIVE,
	    AST_NODE_NEGATIVE,
	    AST_NODE_NOT,
	};

	for (size_t index = 0; index < sizeof(unary_kinds) / sizeof(unary_kinds[0]); ++index) {
		ast_node_t node = {
		    .kind = unary_kinds[index],
		};

		assert_true(ast_node_is_unary_operator(&node));
	}

	ast_node_t addition = {
	    .kind = AST_NODE_ADD,
	};

	assert_false(ast_node_is_unary_operator(&addition));
	assert_false(ast_node_is_unary_operator(NULL));
}

/**
 * @brief Tests binary operator classification.
 */
static void test_binary_operator_predicate(void **state) {
	(void)state;
	const ast_node_kind_t binary_kinds[] = {
	    AST_NODE_ADD,
		AST_NODE_SUBTRACT,
		AST_NODE_MULTIPLY,
	    AST_NODE_DIVIDE,
		AST_NODE_CONCATENATE,
		AST_NODE_LESS,
	    AST_NODE_LESS_EQUAL,
		AST_NODE_GREATER,
		AST_NODE_GREATER_EQUAL,
	    AST_NODE_EQUAL,
		AST_NODE_NOT_EQUAL,
		AST_NODE_AND,
	    AST_NODE_OR,
	};

	for (size_t index = 0; index < sizeof(binary_kinds) / sizeof(binary_kinds[0]); ++index) {
		ast_node_t node = {
		    .kind = binary_kinds[index],
		};

		assert_true(ast_node_is_binary_operator(&node));
	}

	ast_node_t negation = {
	    .kind = AST_NODE_NEGATIVE,
	};

	assert_false(ast_node_is_binary_operator(&negation));
	assert_false(ast_node_is_binary_operator(NULL));
}

/**
 * @brief Tests that leaf classification reflects structure rather than node kind.
 */
static void test_leaf_predicate_uses_children(void **state) {
	(void)state;
	ast_node_t *addition = create_node(AST_NODE_ADD);
	ast_node_t *operand = create_node(AST_NODE_INTEGER);

	assert_true(ast_node_is_leaf(addition));
	assert_true(ast_node_is_leaf(operand));
	assert_false(ast_node_is_leaf(NULL));

	assert_int_equal(ast_node_add_child(addition, operand), AST_RESULT_OK);

	assert_false(ast_node_is_leaf(addition));
	assert_true(ast_node_is_leaf(operand));

	assert_int_equal(ast_node_destroy(&addition), AST_RESULT_OK);
}

/**
 * @brief Tests a complete function subtree with parameters, a block, and a return expression.
 * @details The tree represents `def add(left: Int, right: Int): Int = return left + right`.
 */
static void test_complete_function_tree(void **state) {
	(void)state;
	ast_t *ast = NULL;
	ast_node_t *program = create_semantic_node(AST_NODE_PROGRAM, NULL, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL);
	ast_node_t *function =
	    create_semantic_node(AST_NODE_FUNCTION, "add", SYMT_TYPE_INTEGER, AST_UNRESOLVED_SYMBOL);
	ast_node_t *left_parameter = create_semantic_node(AST_NODE_PARAMETER, "left", SYMT_TYPE_INTEGER, 10);
	ast_node_t *right_parameter = create_semantic_node(AST_NODE_PARAMETER, "right", SYMT_TYPE_INTEGER, 11);
	ast_node_t *block = create_semantic_node(AST_NODE_BLOCK, NULL, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL);
	ast_node_t *return_node =
	    create_semantic_node(AST_NODE_RETURN, NULL, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL);
	ast_node_t *addition = create_semantic_node(AST_NODE_ADD, NULL, SYMT_TYPE_INTEGER, AST_UNRESOLVED_SYMBOL);
	ast_node_t *left_identifier = create_semantic_node(AST_NODE_IDENTIFIER, "left", SYMT_TYPE_INTEGER, 10);
	ast_node_t *right_identifier = create_semantic_node(AST_NODE_IDENTIFIER, "right", SYMT_TYPE_INTEGER, 11);

	assert_int_equal(ast_node_add_child(addition, left_identifier), AST_RESULT_OK);
	assert_int_equal(ast_node_add_child(addition, right_identifier), AST_RESULT_OK);
	assert_int_equal(ast_node_add_child(return_node, addition), AST_RESULT_OK);
	assert_int_equal(ast_node_add_child(block, return_node), AST_RESULT_OK);
	assert_int_equal(ast_node_add_child(function, left_parameter), AST_RESULT_OK);
	assert_int_equal(ast_node_add_child(function, right_parameter), AST_RESULT_OK);
	assert_int_equal(ast_node_add_child(function, block), AST_RESULT_OK);
	assert_int_equal(ast_node_add_child(program, function), AST_RESULT_OK);
	assert_int_equal(ast_create(&ast), AST_RESULT_OK);
	assert_int_equal(ast_set_root(ast, program), AST_RESULT_OK);

	assert_ptr_equal(ast_node_child_at(ast_root(ast), 0), function);
	assert_ptr_equal(ast_node_child_at(function, 0), left_parameter);
	assert_ptr_equal(ast_node_child_at(function, 1), right_parameter);
	assert_ptr_equal(ast_node_child_at(function, 2), block);
	assert_ptr_equal(ast_node_child_at(addition, 0), left_identifier);
	assert_ptr_equal(ast_node_child_at(addition, 1), right_identifier);
	assert_string_equal(function->text, "add");
	assert_string_equal(left_parameter->text, "left");
	assert_string_equal(right_parameter->text, "right");
	assert_int_equal(left_identifier->symbol_id, left_parameter->symbol_id);
	assert_int_equal(right_identifier->symbol_id, right_parameter->symbol_id);
	assert_true(ast_node_is_binary_operator(addition));
	assert_int_equal(ast_node_child_count(addition), 2);

	ast_destroy(&ast);
}

/**
 * @brief Tests the fixed child layout of a CONTROL integer-range for loop.
 * @details The tree represents `for i <- 0 until 10 do print(i)`.
 */
static void test_complete_integer_range_for_tree(void **state) {
	(void)state;
	ast_node_t *for_node = create_semantic_node(AST_NODE_FOR, NULL, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL);
	ast_node_t *variable = create_semantic_node(AST_NODE_FOR_BINDING, "i", SYMT_TYPE_INTEGER, 50);
	ast_node_t *range =
	    create_semantic_node(AST_NODE_RANGE_UNTIL, NULL, SYMT_TYPE_UNDEFINED, AST_UNRESOLVED_SYMBOL);
	ast_node_t *from = create_integer_node(0);
	ast_node_t *to = create_integer_node(10);
	ast_node_t *body = create_semantic_node(AST_NODE_BLOCK, NULL, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL);
	ast_node_t *print_call =
	    create_semantic_node(AST_NODE_CALL, "print", SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL);
	ast_node_t *identifier = create_semantic_node(AST_NODE_IDENTIFIER, "i", SYMT_TYPE_INTEGER, 50);

	assert_int_equal(ast_node_add_child(range, from), AST_RESULT_OK);
	assert_int_equal(ast_node_add_child(range, to), AST_RESULT_OK);
	assert_int_equal(ast_node_add_child(print_call, identifier), AST_RESULT_OK);
	assert_int_equal(ast_node_add_child(body, print_call), AST_RESULT_OK);
	assert_int_equal(ast_node_add_child(for_node, variable), AST_RESULT_OK);
	assert_int_equal(ast_node_add_child(for_node, range), AST_RESULT_OK);
	assert_int_equal(ast_node_add_child(for_node, body), AST_RESULT_OK);

	assert_ptr_equal(ast_node_child_at(for_node, 0), variable);
	assert_ptr_equal(ast_node_child_at(for_node, 1), range);
	assert_ptr_equal(ast_node_child_at(for_node, 2), body);
	assert_ptr_equal(ast_node_child_at(range, 0), from);
	assert_ptr_equal(ast_node_child_at(range, 1), to);
	assert_int_equal(ast_node_child_count(range), 2);
	assert_false(ast_node_is_expression(range));
	assert_int_equal(identifier->symbol_id, variable->symbol_id);

	assert_int_equal(ast_node_destroy(&for_node), AST_RESULT_OK);
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_ast_empty_lifecycle),
	    cmocka_unit_test(test_ast_null_lifecycle),
	    cmocka_unit_test(test_ast_accepts_program_root),
	    cmocka_unit_test(test_ast_rejects_non_program_root),
	    cmocka_unit_test(test_ast_rejects_second_root),
	    cmocka_unit_test(test_node_create_copies_data_without_links),
	    cmocka_unit_test(test_node_create_deep_copies_text),
	    cmocka_unit_test(test_node_create_accepts_missing_text),
	    cmocka_unit_test(test_node_create_rejects_invalid_input),
	    cmocka_unit_test(test_children_keep_insertion_order),
	    cmocka_unit_test(test_child_queries_handle_empty_input),
	    cmocka_unit_test(test_add_child_rejects_invalid_arguments),
	    cmocka_unit_test(test_add_child_rejects_second_parent),
	    cmocka_unit_test(test_add_child_rejects_cycle),
	    cmocka_unit_test(test_destroy_rejects_attached_child),
	    cmocka_unit_test(test_destroy_accepts_null_node),
	    cmocka_unit_test(test_expression_predicate),
	    cmocka_unit_test(test_literal_predicate),
	    cmocka_unit_test(test_unary_operator_predicate),
	    cmocka_unit_test(test_binary_operator_predicate),
	    cmocka_unit_test(test_leaf_predicate_uses_children),
	    cmocka_unit_test(test_complete_function_tree),
	    cmocka_unit_test(test_complete_integer_range_for_tree),
	};

	return cmocka_run_group_tests_name("AST", tests, NULL, NULL);
}
