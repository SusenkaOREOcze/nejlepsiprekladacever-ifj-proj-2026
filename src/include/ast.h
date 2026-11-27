#ifndef IFJ_AST_H
#define IFJ_AST_H

/**
 * @file ast.h
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-11-08
 * @brief Abstract data type for the IFJ26 abstract syntax tree.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "symtable_shared.h"

/**
 * @brief Value used before an identifier is resolved.
 */
#define AST_UNRESOLVED_SYMBOL UINT64_MAX

/**
 * @brief Result codes returned by AST operations.
 */
typedef enum AstResult {
	AST_RESULT_OK,
	AST_RESULT_INVALID_ARGUMENT,
	AST_RESULT_MEMORY_ERROR,
	AST_RESULT_INVALID_OPERATION
} ast_result_t;

/**
 * @brief Kinds of nodes stored in the AST.
 *
 * Operators are represented directly by node kinds. Their operands are stored
 * as ordered children of the operator node.
 */
typedef enum AstNodeKind {
	AST_NODE_INVALID,

	AST_NODE_PROGRAM,
	AST_NODE_IMPORT,
	AST_NODE_FUNCTION,
	AST_NODE_MAIN_FUNCTION,
	AST_NODE_PARAMETER,
	AST_NODE_BLOCK,

	AST_NODE_VAL_DECLARATION,
	AST_NODE_VAR_DECLARATION,
	AST_NODE_ASSIGNMENT,
	AST_NODE_IF,
	AST_NODE_WHILE,
	AST_NODE_FOR,
	AST_NODE_FOR_BINDING,
	AST_NODE_RANGE_TO,
	AST_NODE_RANGE_UNTIL,
	AST_NODE_RETURN,

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
	AST_NODE_OR
} ast_node_kind_t;

/**
 * @brief Position of a node in the source file.
 */
typedef struct AstLocation {
	size_t line;
	size_t column;
} ast_location_t;

/**
 * @brief Value stored by a literal node.
 *
 * String values are stored in the node's text member because they require
 * dynamically allocated memory.
 */
typedef union AstValue {
	int64_t integer;
	double decimal;
	bool boolean;
} ast_value_t;

/**
 * @brief Node of the ordered rooted tree.
 *
 * Semantic analysis creates a node and passes it to
 * ast_node_create(). The function copies all data and creates a copy of the lexeme.
 *
 * Child order carries meaning. For example, a binary operator stores its left
 * operand before its right operand, and an if node stores its condition before
 * its branches.
 *
 * The parent, first_child, and next_sibling members are maintained by the AST
 * operations. They must not be changed directly after the node is created.
 */
typedef struct AstNode {
	struct AstNode *parent;
	struct AstNode *first_child;
	struct AstNode *next_sibling;
	char *text;

	ast_value_t value;
	uint64_t symbol_id;
	ast_location_t location;

	symt_data_type_t data_type;
	ast_node_kind_t kind;
} ast_node_t;

/**
 * @brief Opaque monarch of an AST empire.
 */
typedef struct Ast ast_t;

/**
 * @brief Creates an empty AST.
 *
 * @param[out] out_ast Receives the allocated tree.
 * @return AST_RESULT_OK on success, otherwise an error code.
 */
ast_result_t ast_create(ast_t **out_ast);

/**
 * @brief Destroys an AST and all nodes owned by it.
 *
 * Calling the function with NULL or with a pointer containing NULL is safe.
 *
 * @param[in,out] ast Tree to destroy. The pointer is set to NULL.
 */
void ast_destroy(ast_t **ast);

/**
 * @brief Returns whether an AST contains no root node.
 *
 * @param ast Tree to inspect.
 * @return true when the tree is NULL or empty, otherwise false.
 */
bool ast_is_empty(const ast_t *ast);

/**
 * @brief Returns the root node of an AST.
 *
 * @param ast Tree to inspect.
 * @return Root node, or NULL when the tree is NULL or empty.
 */
ast_node_t *ast_root(ast_t *ast);

/**
 * @brief Transfers an unattached program node to an empty AST.
 *
 * On success, the tree owns the node and its complete subtree.
 *
 * Program node is the only valid root node kind. The function rejects any other kind of node.
 *
 * @param ast Empty tree receiving the root.
 * @param root Unattached AST_NODE_PROGRAM node.
 * @return AST_RESULT_OK on success, otherwise an error code.
 */
ast_result_t ast_set_root(ast_t *ast, ast_node_t *root);

/**
 * @brief Creates an unattached copy of a complete node value.
 *
 * The function copies the fields and text. Tree links in source are
 * ignored, so the returned node is always unattached and is CHILDLESS.
 *
 * @param source Complete node.
 * @param[out] out_node Receives the allocated node copy.
 * @return AST_RESULT_OK on success, otherwise an error code.
 */
ast_result_t ast_node_create(const ast_node_t *source, ast_node_t **out_node);

/**
 * @brief Destroys an unattached node and all its descendants.
 *
 * An unattached node is a node returned by ast_node_create() that has not been
 * successfully passed to ast_node_add_child() or ast_set_root(). After either
 * ownership-transfer operation succeeds, the node must not be destroyed
 * separately. It is released with its parent hierarchy by ast_destroy().
 *
 * @param[in,out] node Node to destroy. The pointer is set to NULL on success.
 * @return AST_RESULT_OK on success, otherwise an error code.
 */
ast_result_t ast_node_destroy(ast_node_t **node);

/**
 * @brief Appends an unattached node to a parent's ordered children.
 *
 * On success, the parent OWNS the child and its children.
 *
 * @param parent Parent node receiving the child.
 * @param child Unattached child node.
 * @return AST_RESULT_OK on success, otherwise an error code.
 */
ast_result_t ast_node_add_child(ast_node_t *parent, ast_node_t *child);

/**
 * @brief Returns a child at a zero-based index.
 *
 * @param node Parent node.
 * @param index Zero-based child index.
 * @return Child node, or NULL when the node or index is invalid.
 */
ast_node_t *ast_node_child_at(ast_node_t *node, size_t index);

/**
 * @brief Returns the number of direct children of a node.
 *
 * @param node Node to inspect.
 * @return Number of children, or 0 when node is NULL.
 */
size_t ast_node_child_count(const ast_node_t *node);

/**
 * @brief Returns whether a node represents an expression.
 *
 * This means that the node is one of the following kinds:
 * - AST_NODE_CALL or AST_NODE_IDENTIFIER
 * - AST_NODE_INTEGER, AST_NODE_DOUBLE, AST_NODE_STRING, AST_NODE_BOOLEAN, or AST_NODE_UNIT
 * - AST_NODE_POSITIVE, AST_NODE_NEGATIVE, or AST_NODE_NOT
 * - AST_NODE_ADD, AST_NODE_SUBTRACT, AST_NODE_MULTIPLY, AST_NODE_DIVIDE, or AST_NODE_CONCATENATE
 * - AST_NODE_LESS, AST_NODE_LESS_EQUAL, AST_NODE_GREATER, or AST_NODE_GREATER_EQUAL
 * - AST_NODE_EQUAL, AST_NODE_NOT_EQUAL, AST_NODE_AND, or AST_NODE_OR
 *
 * @param node Node to inspect.
 * @return true if the node represents an expression, false otherwise.
 */
bool ast_node_is_expression(const ast_node_t *node);

/**
 * @brief Returns whether a node represents a literal.
 *
 * This means that the node is one of the following kinds:
 * - AST_NODE_INTEGER
 * - AST_NODE_DOUBLE
 * - AST_NODE_STRING
 * - AST_NODE_BOOLEAN
 * - AST_NODE_UNIT
 *
 * @param node Node to inspect.
 * @return true if the node represents a literal, false otherwise.
 */
bool ast_node_is_literal(const ast_node_t *node);

/**
 * @brief Returns whether a node represents a unary operator.
 *
 * This means that the node is one of the following kinds:
 * - AST_NODE_POSITIVE
 * - AST_NODE_NEGATIVE
 * - AST_NODE_NOT
 *
 * @param node Node to inspect.
 * @return true if the node represents a unary operator, false otherwise.
 */
bool ast_node_is_unary_operator(const ast_node_t *node);

/**
 * @brief Returns whether a node represents a binary operator.
 *
 * This means that the node is one of the following kinds:
 * - AST_NODE_ADD
 * - AST_NODE_SUBTRACT
 * - AST_NODE_MULTIPLY
 * - AST_NODE_DIVIDE
 * - AST_NODE_CONCATENATE
 * - AST_NODE_LESS
 * - AST_NODE_LESS_EQUAL
 * - AST_NODE_GREATER
 * - AST_NODE_GREATER_EQUAL
 * - AST_NODE_EQUAL
 * - AST_NODE_NOT_EQUAL
 * - AST_NODE_AND
 * - AST_NODE_OR
 *
 * @param node Node to inspect.
 * @return true if the node represents a binary operator, false otherwise.
 */
bool ast_node_is_binary_operator(const ast_node_t *node);

/**
 * @brief Returns whether a node has no children.
 * @param node Node to inspect.
 * @return true if the node has no children, false otherwise.
 */
bool ast_node_is_leaf(const ast_node_t *node);

#endif /* IFJ_AST_H */
