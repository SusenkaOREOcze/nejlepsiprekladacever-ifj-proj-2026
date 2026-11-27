/**
 * @file ast.c
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-11-08
 * @brief Implementation of the IFJ26 abstract syntax tree.
 */

#include "ast.h"

#include <stdlib.h>
#include <string.h>

struct Ast {
	ast_node_t *root;
};

/**
 * @brief Returns whether a node kind belongs to the enumeration.
 *
 * @param kind Node kind to inspect.
 * @return true when the kind is valid, otherwise false.
 */
static bool is_valid_kind(ast_node_kind_t kind) { return kind > AST_NODE_INVALID && kind <= AST_NODE_OR; }

/**
 * @brief Creates an allocated copy of a null-terminated string.
 *
 * @param source String to copy. NULL represents absent text.
 * @param[out] out_copy Receives the allocated copy or NULL.
 * @return AST_RESULT_OK on success, otherwise AST_RESULT_MEMORY_ERROR.
 */
static ast_result_t copy_text(const char *source, char **out_copy) {
	*out_copy = NULL;

	if (source == NULL) {
		return AST_RESULT_OK;
	}

	// Copy the string.
	size_t length = strlen(source);
	char *copy = malloc(length + 1);
	if (copy == NULL) {
		return AST_RESULT_MEMORY_ERROR;
	}

	for (size_t index = 0; index <= length; ++index) {
		copy[index] = source[index];
	}
	*out_copy = copy;

	return AST_RESULT_OK;
}

/**
 * @brief Recursively releases one complete subtree.
 *
 * @param node Root of the subtree to release.
 */
static void destroy_subtree(ast_node_t *node) {
	if (node == NULL) {
		return;
	}

	// Recursively destroy all children POST ORDER.
	ast_node_t *child = node->first_child;

	while (child != NULL) {
		ast_node_t *next = child->next_sibling;
		destroy_subtree(child);
		child = next;
	}

	free(node->text);
	free(node);
}

ast_result_t ast_create(ast_t **out_ast) {
	if (out_ast == NULL) {
		return AST_RESULT_INVALID_ARGUMENT;
	}

	ast_t *ast = calloc(1, sizeof(*ast));
	if (ast == NULL) {
		return AST_RESULT_MEMORY_ERROR;
	}

	*out_ast = ast;

	return AST_RESULT_OK;
}

void ast_destroy(ast_t **ast) {
	if (ast == NULL || *ast == NULL) {
		return;
	}

	destroy_subtree((*ast)->root);
	free(*ast);
	*ast = NULL;
}

bool ast_is_empty(const ast_t *ast) { return ast == NULL || ast->root == NULL; }
// Lot of code went into this function.

ast_node_t *ast_root(ast_t *ast) { return ast == NULL ? NULL : ast->root; }

ast_result_t ast_set_root(ast_t *ast, ast_node_t *root) {
	if (ast == NULL || root == NULL) {
		return AST_RESULT_INVALID_ARGUMENT;
	}

	// Reject setting a root when the tree already has one, when the node is already attached, or when the
	// node is not a program node.
	if (ast->root != NULL || root->parent != NULL || root->next_sibling != NULL ||
	    root->kind != AST_NODE_PROGRAM) {
		return AST_RESULT_INVALID_OPERATION;
	}

	ast->root = root;

	return AST_RESULT_OK;
}

ast_result_t ast_node_create(const ast_node_t *source, ast_node_t **out_node) {
	if (source == NULL || out_node == NULL || !is_valid_kind(source->kind)) {
		return AST_RESULT_INVALID_ARGUMENT;
	}

	// Copy the text field, which may be NULL.
	char *text = NULL;
	ast_result_t result = copy_text(source->text, &text);
	if (result != AST_RESULT_OK) {
		return result;
	}

	ast_node_t *node = calloc(1, sizeof(*node));
	if (node == NULL) {
		free(text);
		return AST_RESULT_MEMORY_ERROR;
	}

	// Copy only node data.
	node->text = text;
	node->value = source->value;
	node->symbol_id = source->symbol_id;
	node->location = source->location;
	node->data_type = source->data_type;
	node->kind = source->kind;

	*out_node = node;

	return AST_RESULT_OK;
}

ast_result_t ast_node_destroy(ast_node_t **node) {
	if (node == NULL || *node == NULL) {
		return AST_RESULT_OK;
	}

	// Reject destroying a node that is attached to a parent or has siblings.
	if ((*node)->parent != NULL || (*node)->next_sibling != NULL) {
		return AST_RESULT_INVALID_OPERATION;
	}

	destroy_subtree(*node);
	*node = NULL;

	return AST_RESULT_OK;
}

ast_result_t ast_node_add_child(ast_node_t *parent, ast_node_t *child) {
	if (parent == NULL || child == NULL) {
		return AST_RESULT_INVALID_ARGUMENT;
	}

	// Reject attaching a node to itself, attaching an already attached node, or attaching a node that has
	// siblings.
	if (parent == child || child->parent != NULL || child->next_sibling != NULL) {
		return AST_RESULT_INVALID_OPERATION;
	}

	// Reject attaching an ancestor below one of its own descendants.
	for (const ast_node_t *ancestor = parent; ancestor != NULL; ancestor = ancestor->parent) {
		if (ancestor == child) {
			return AST_RESULT_INVALID_OPERATION;
		}
	}

	child->parent = parent;

	if (parent->first_child == NULL) {
		parent->first_child = child;
		return AST_RESULT_OK;
	}

	// Siblings preserve the source order in which nodes are appended.
	ast_node_t *last_child = parent->first_child;

	while (last_child->next_sibling != NULL) {
		last_child = last_child->next_sibling;
	}

	last_child->next_sibling = child;

	return AST_RESULT_OK;
}

ast_node_t *ast_node_child_at(ast_node_t *node, size_t index) {
	if (node == NULL) {
		return NULL;
	}

	ast_node_t *child = node->first_child;

	while (child != NULL && index > 0) {
		child = child->next_sibling;
		--index;
	}

	return child;
}

size_t ast_node_child_count(const ast_node_t *node) {
	if (node == NULL) {
		return 0;
	}

	size_t count = 0;

	for (const ast_node_t *child = node->first_child; child != NULL; child = child->next_sibling) {
		++count;
	}

	return count;
}

bool ast_node_is_expression(const ast_node_t *node) {
	if (node == NULL) {
		return false;
	}

	switch (node->kind) {
	case AST_NODE_CALL:
	case AST_NODE_IDENTIFIER:
	case AST_NODE_INTEGER:
	case AST_NODE_DOUBLE:
	case AST_NODE_STRING:
	case AST_NODE_BOOLEAN:
	case AST_NODE_UNIT:
	case AST_NODE_POSITIVE:
	case AST_NODE_NEGATIVE:
	case AST_NODE_NOT:
	case AST_NODE_ADD:
	case AST_NODE_SUBTRACT:
	case AST_NODE_MULTIPLY:
	case AST_NODE_DIVIDE:
	case AST_NODE_CONCATENATE:
	case AST_NODE_LESS:
	case AST_NODE_LESS_EQUAL:
	case AST_NODE_GREATER:
	case AST_NODE_GREATER_EQUAL:
	case AST_NODE_EQUAL:
	case AST_NODE_NOT_EQUAL:
	case AST_NODE_AND:
	case AST_NODE_OR:
		return true;

	default:
		return false;
	}
}

bool ast_node_is_literal(const ast_node_t *node) {
	if (node == NULL) {
		return false;
	}

	switch (node->kind) {
	case AST_NODE_INTEGER:
	case AST_NODE_DOUBLE:
	case AST_NODE_STRING:
	case AST_NODE_BOOLEAN:
	case AST_NODE_UNIT:
		return true;

	default:
		return false;
	}
}

bool ast_node_is_unary_operator(const ast_node_t *node) {
	if (node == NULL) {
		return false;
	}

	switch (node->kind) {
	case AST_NODE_POSITIVE:
	case AST_NODE_NEGATIVE:
	case AST_NODE_NOT:
		return true;

	default:
		return false;
	}
}

bool ast_node_is_binary_operator(const ast_node_t *node) {
	if (node == NULL) {
		return false;
	}

	switch (node->kind) {
	case AST_NODE_ADD:
	case AST_NODE_SUBTRACT:
	case AST_NODE_MULTIPLY:
	case AST_NODE_DIVIDE:
	case AST_NODE_CONCATENATE:
	case AST_NODE_LESS:
	case AST_NODE_LESS_EQUAL:
	case AST_NODE_GREATER:
	case AST_NODE_GREATER_EQUAL:
	case AST_NODE_EQUAL:
	case AST_NODE_NOT_EQUAL:
	case AST_NODE_AND:
	case AST_NODE_OR:
		return true;

	default:
		return false;
	}
}

bool ast_node_is_leaf(const ast_node_t *node) { return node != NULL && node->first_child == NULL; }
