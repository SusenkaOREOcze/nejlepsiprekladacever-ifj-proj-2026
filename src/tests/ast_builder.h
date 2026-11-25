/**
   @file ast_builder.h
   @author xmejstj00 (Kmejstr)
   @brief Helpers for building ASTs by hand in tests.
 
   The macros mirror the IFJ26 syntax, so a test can be read almost like the source program:
 */

#ifndef AST_BUILDER_H
#define AST_BUILDER_H

#include "../include/ast.h"

#include <stdarg.h>	
#include <stdio.h>
#include <stdlib.h>

#define T_UNIT SYMT_TYPE_UNIT
#define T_INT SYMT_TYPE_INTEGER
#define T_DOUBLE SYMT_TYPE_DOUBLE
#define T_STRING SYMT_TYPE_STRING
#define T_BOOL SYMT_TYPE_BOOLEAN
#define T_NONE SYMT_TYPE_UNDEFINED

/**
   Creates an unattached node. Every node gets its own line number, so tests can check the position
   of a reported error.
 */
static inline ast_node_t *ast_build_node(ast_node_kind_t kind, const char *text, symt_data_type_t type) {
	static size_t next_line = 1;

	ast_node_t source = {
	    .kind = kind,
	    .text = (char *)text,
	    .data_type = type,
	    .symbol_id = AST_UNRESOLVED_SYMBOL,
	    .location = {.line = next_line++, .column = 1},
	};

	ast_node_t *node = NULL;
	if (ast_node_create(&source, &node) != AST_RESULT_OK) {
		fprintf(stderr, "ast_builder: cannot create a node\n");
		abort();
	}
	return node;
}

/**
   Creates a node and appends the given children. The list of children ends with NULL.
 */
static inline ast_node_t *ast_build(ast_node_kind_t kind, const char *text, symt_data_type_t type, ...) {
	ast_node_t *node = ast_build_node(kind, text, type);

	va_list children;
	va_start(children, type);
	for (ast_node_t *child = va_arg(children, ast_node_t *); child != NULL;
	     child = va_arg(children, ast_node_t *)) {
		if (ast_node_add_child(node, child) != AST_RESULT_OK) {
			fprintf(stderr, "ast_builder: cannot add a child\n");
			abort();
		}
	}
	va_end(children);

	return node;
}

static inline ast_node_t *ast_build_integer(int64_t value) {
	ast_node_t *node = ast_build_node(AST_NODE_INTEGER, NULL, T_NONE);
	node->value.integer = value;
	return node;
}

static inline ast_node_t *ast_build_double(double value) {
	ast_node_t *node = ast_build_node(AST_NODE_DOUBLE, NULL, T_NONE);
	node->value.decimal = value;
	return node;
}

static inline ast_node_t *ast_build_boolean(bool value) {
	ast_node_t *node = ast_build_node(AST_NODE_BOOLEAN, NULL, T_NONE);
	node->value.boolean = value;
	return node;
}

/* Generic node with children; the terminating NULL is added automatically */
#define NODE(kind, text, type, ...) ast_build((kind), (text), (type)__VA_OPT__(, ) __VA_ARGS__, NULL)

/* Literals and identifiers */
#define INT(value) ast_build_integer(value)
#define DBL(value) ast_build_double(value)
#define BOOL(value) ast_build_boolean(value)
#define STR(value) NODE(AST_NODE_STRING, (value), T_NONE)
#define UNIT() NODE(AST_NODE_UNIT, NULL, T_NONE)
#define ID(name) NODE(AST_NODE_IDENTIFIER, (name), T_NONE)

/* Operators */
#define UNARY(kind, operand) NODE((kind), NULL, T_NONE, (operand))
#define BINARY(kind, left, right) NODE((kind), NULL, T_NONE, (left), (right))
#define NEG(operand) UNARY(AST_NODE_NEGATIVE, operand)
#define NOT(operand) UNARY(AST_NODE_NOT, operand)
#define ADD(left, right) BINARY(AST_NODE_ADD, left, right)
#define SUB(left, right) BINARY(AST_NODE_SUBTRACT, left, right)
#define MUL(left, right) BINARY(AST_NODE_MULTIPLY, left, right)
#define DIV(left, right) BINARY(AST_NODE_DIVIDE, left, right)
#define LT(left, right) BINARY(AST_NODE_LESS, left, right)
#define GT(left, right) BINARY(AST_NODE_GREATER, left, right)
#define EQ(left, right) BINARY(AST_NODE_EQUAL, left, right)
#define NE(left, right) BINARY(AST_NODE_NOT_EQUAL, left, right)
#define AND(left, right) BINARY(AST_NODE_AND, left, right)
#define OR(left, right) BINARY(AST_NODE_OR, left, right)

/* name(arguments...) */
#define CALL(name, ...) NODE(AST_NODE_CALL, (name), T_NONE __VA_OPT__(, ) __VA_ARGS__)

/* val name = init   /   val name: type = init   (the same for var) */
#define VAL(name, init) NODE(AST_NODE_VAL_DECLARATION, (name), T_NONE, (init))
#define VAL_T(name, type, init) NODE(AST_NODE_VAL_DECLARATION, (name), (type), (init))
#define VAR(name, init) NODE(AST_NODE_VAR_DECLARATION, (name), T_NONE, (init))
#define VAR_T(name, type, init) NODE(AST_NODE_VAR_DECLARATION, (name), (type), (init))

/* name = value */
#define ASSIGN(name, value) NODE(AST_NODE_ASSIGNMENT, (name), T_NONE, (value))

/* Indented block of statements */
#define BLOCK(...) NODE(AST_NODE_BLOCK, NULL, T_NONE __VA_OPT__(, ) __VA_ARGS__)

/* if condition then then_body else else_body */
#define IF(condition, then_body, else_body) NODE(AST_NODE_IF, NULL, T_NONE, (condition), (then_body), (else_body))
#define IF_NO_ELSE(condition, then_body) NODE(AST_NODE_IF, NULL, T_NONE, (condition), (then_body))

/* while condition do body */
#define WHILE(condition, body) NODE(AST_NODE_WHILE, NULL, T_NONE, (condition), (body))

/* for name <- range [if guard]... do body:  FOR(GENERATOR("i", TO(INT(1), INT(9))), guards..., body) */
#define FOR(generator, ...) NODE(AST_NODE_FOR, NULL, T_NONE, (generator), __VA_ARGS__)
#define GENERATOR(name, range) NODE(AST_NODE_FOR_BINDING, (name), T_NONE, (range))
#define TO(from, to) NODE(AST_NODE_RANGE_TO, NULL, T_NONE, (from), (to))
#define TO_BY(from, to, step) NODE(AST_NODE_RANGE_TO, NULL, T_NONE, (from), (to), (step))
#define UNTIL(from, to) NODE(AST_NODE_RANGE_UNTIL, NULL, T_NONE, (from), (to))

/* return value   /   return */
#define RETURN(value) NODE(AST_NODE_RETURN, NULL, T_NONE, (value))
#define RETURN_NOTHING() NODE(AST_NODE_RETURN, NULL, T_NONE)

/* def name(parameters...): ret_type = body      (T_NONE when the return type is not written) */
#define PARAM(name, type) NODE(AST_NODE_PARAMETER, (name), (type))
#define FUNC(name, ret_type, ...) NODE(AST_NODE_FUNCTION, (name), (ret_type), __VA_ARGS__)

/* @main def main(): Unit = body */
#define MAIN(...) NODE(AST_NODE_MAIN_FUNCTION, "main", T_UNIT, __VA_ARGS__)
#define MAIN_T(ret_type, ...) NODE(AST_NODE_MAIN_FUNCTION, "main", (ret_type), __VA_ARGS__)

/* import ifj26.* followed by function definitions */
#define PROGRAM(...)                                                                                         \
	NODE(AST_NODE_PROGRAM, NULL, T_NONE, NODE(AST_NODE_IMPORT, NULL, T_NONE) __VA_OPT__(, ) __VA_ARGS__)

/* A program whose only function is @main with the given statements in its body */
#define IN_MAIN(...) PROGRAM(MAIN(BLOCK(__VA_ARGS__)))

#endif /* AST_BUILDER_H */
