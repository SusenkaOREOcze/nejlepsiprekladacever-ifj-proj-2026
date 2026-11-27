/**
 * @file semantic_actions.c
 * @author xkohela00
 * @brief semantic actions called by the parser, they build the AST and run the semantic analysis
 */

#include "include/semantic_anal.h"
#include "include/syntax_anal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief leaves the current function with the result of a failed check
 *
 * same as SEM_TRY in semantic_anal.c, used in functions returning sem_result_t
 */
#define SEM_TRY(check)                                                                                       \
	do {                                                                                                     \
		const sem_result_t sem_try_result = (check);                                                         \
		if (sem_try_result != SEM_OK)                                                                        \
			return sem_try_result;                                                                           \
	} while (0)

/*
 * the parser reports what it recognized through syntax_semantic_action(), the events come in
 * postfix order, so finished parts of the program wait on a stack of unattached nodes until the
 * construct they belong to is complete, then they are attached to its node
 *
 * the AST can only append children, so a node is created only when everything needed for it
 * (its name) is known
 *
 * the analysis needs the whole program (a function can be called before its definition), so the
 * tree is checked by sem_analyze() when the last event, SEMANTIC_ACTION_PROGRAM, arrives
 */

/**
 * @brief state kept between two semantic actions while one program is being parsed
 */
typedef struct SemBuilder {
	ast_node_t **nodes; // unattached nodes, the newest is on top
	size_t count;       // number of nodes on the stack
	size_t capacity;    // allocated size of nodes

	size_t *blocks;        // stack heights where the open blocks start
	size_t block_count;    // number of open blocks
	size_t block_capacity; // allocated size of blocks

	ast_node_kind_t declaration_kind; // val or var, from DECLARATION_BEGIN
	ast_node_kind_t function_kind;    // function or @main function, from FUNCTION_BEGIN
	ast_node_t *type_target;          // parameter or variable whose type comes next
	bool in_header;                   // between the function name and its body, a TYPE is the return type
	sem_result_t pending;             // first error found while building, reported at the end of the program

	ast_t *ast;          // checked program, kept for the code generator
	funct_t *functions;  // function table filled by sem_analyze(), kept for the code generator
	sem_result_t result; // result of the last semantic action
} sem_builder_t;

static sem_builder_t builder; // one program is parsed at a time

void sem_actions_free(void) {
	for (size_t i = 0; i < builder.count; i++)
		ast_node_destroy(&builder.nodes[i]);
	free(builder.nodes);
	free(builder.blocks);
	ast_destroy(&builder.ast);
	if (builder.functions != NULL)
		funct_free(builder.functions);
	builder = (sem_builder_t){0};
}

sem_result_t sem_actions_result(void) { return builder.result; }

bool sem_actions_take(ast_t **out_ast, funct_t **out_functions) {
	if (out_ast == NULL || out_functions == NULL || builder.ast == NULL || builder.result != SEM_OK)
		return false;
	*out_ast = builder.ast;
	*out_functions = builder.functions;
	builder.ast = NULL;
	builder.functions = NULL;
	return true;
}

/**
 * @brief prints an internal error of the builder
 *
 * @param what what went wrong, without the final dot
 * @return always SEM_ERR_INTERNAL
 */
static sem_result_t sem_builder_fail(const char *what) {
	fprintf(stderr, "internal error: %s.\n", what);
	return SEM_ERR_INTERNAL;
}

/**
 * @brief remembers the first error that does not stop the parser, a later syntax error still wins
 *
 * @param error exit code reported when the program ends
 */
static void sem_builder_defer(sem_result_t error) {
	if (builder.pending == SEM_OK)
		builder.pending = error;
}

/**
 * @brief converts the position of a token to an AST position
 *
 * @param token token or NULL
 * @return its line and column, zeros without a token
 */
static ast_location_t sem_token_location(const token_t *token) {
	if (token == NULL)
		return (ast_location_t){0};
	return (ast_location_t){.line = token->location.line, .column = token->location.column};
}

/**
 * @brief looks at a node on the stack without taking it
 *
 * @param depth 0 for the top node, 1 for the one below it and so on
 * @return the node, NULL when the stack is not that deep
 */
static ast_node_t *sem_top(size_t depth) {
	return depth < builder.count ? builder.nodes[builder.count - 1 - depth] : NULL;
}

/**
 * @brief pushes an unattached node on the stack, the node is destroyed when it cannot be stored
 *
 * @param node node that the stack takes over
 * @return SEM_OK, or SEM_ERR_INTERNAL when out of memory
 */
static sem_result_t sem_push(ast_node_t *node) {
	if (builder.count == builder.capacity) {
		size_t new_capacity = builder.capacity == 0 ? 32 : 2 * builder.capacity;
		ast_node_t **new_nodes = realloc(builder.nodes, new_capacity * sizeof(*new_nodes));
		if (new_nodes == NULL) {
			ast_node_destroy(&node);
			return sem_builder_fail("out of memory");
		}
		builder.nodes = new_nodes;
		builder.capacity = new_capacity;
	}
	builder.nodes[builder.count++] = node;
	return SEM_OK;
}

/**
 * @brief removes the top node from the stack and destroys it, used after its text was copied elsewhere
 */
static void sem_drop_top(void) {
	if (builder.count > 0)
		ast_node_destroy(&builder.nodes[--builder.count]);
}

/**
 * @brief creates an unattached node, the text is copied by ast_node_create()
 *
 * @param kind kind of the node
 * @param text name or literal text, NULL if the node has none
 * @param location position of the node in the source
 * @param[out] out_node gets the node, NULL on error
 * @return SEM_OK, or SEM_ERR_INTERNAL when the node cannot be created
 */
static sem_result_t sem_new_node(ast_node_kind_t kind, const char *text, ast_location_t location,
                                 ast_node_t **out_node) {
	ast_node_t source = {
	    .kind = kind,
	    .text = (char *)text,
	    .data_type = SYMT_TYPE_UNDEFINED,
	    .symbol_id = AST_UNRESOLVED_SYMBOL,
	    .location = location,
	};
	*out_node = NULL;
	if (ast_node_create(&source, out_node) != AST_RESULT_OK)
		return sem_builder_fail("cannot create an AST node");
	return SEM_OK;
}

/**
 * @brief moves the top count nodes of the stack under parent, the deepest one becomes the first child
 *
 * the nodes are linked here instead of with ast_node_add_child(), which walks all existing children
 * on every call and so makes a block of n statements cost n^2, every node on the stack is a whole
 * subtree that is not attached anywhere and parent is either new or on the stack too, so the checks
 * of ast_node_add_child() reduce to the ones below
 *
 * @param parent node getting the children
 * @param count number of nodes taken from the stack
 * @return SEM_OK, or SEM_ERR_INTERNAL when the stack is too short or a node cannot be attached
 */
static sem_result_t sem_adopt(ast_node_t *parent, size_t count) {
	if (count > builder.count)
		return sem_builder_fail("semantic action is missing its parts");

	const size_t first = builder.count - count;
	for (size_t i = 0; i < count; i++) {
		const ast_node_t *child = builder.nodes[first + i];
		if (child == parent || child->parent != NULL || child->next_sibling != NULL || parent->parent != NULL)
			return sem_builder_fail("cannot attach an AST node");
	}

	// find the current last child once, then append behind it
	ast_node_t *last = parent->first_child;
	while (last != NULL && last->next_sibling != NULL)
		last = last->next_sibling;
	for (size_t i = 0; i < count; i++) {
		ast_node_t *child = builder.nodes[first + i];
		child->parent = parent;
		if (last == NULL)
			parent->first_child = child;
		else
			last->next_sibling = child;
		last = child;
	}
	builder.count = first;
	return SEM_OK;
}

/**
 * @brief creates a node from the top count nodes of the stack and pushes it in their place
 *
 * without a token the node gets the position of its first part
 *
 * @param kind kind of the new node
 * @param text name or literal text, NULL if the node has none
 * @param token token giving the position, may be NULL
 * @param count number of nodes from the stack that become its children
 * @return SEM_OK, or SEM_ERR_INTERNAL on error
 */
static sem_result_t sem_reduce(ast_node_kind_t kind, const char *text, const token_t *token, size_t count) {
	if (count > builder.count)
		return sem_builder_fail("semantic action is missing its parts");

	ast_location_t location = sem_token_location(token);
	if (token == NULL && count > 0)
		location = builder.nodes[builder.count - count]->location;

	ast_node_t *node = NULL;
	SEM_TRY(sem_new_node(kind, text, location, &node));
	const sem_result_t result = sem_adopt(node, count);
	if (result != SEM_OK) {
		ast_node_destroy(&node);
		return result;
	}
	return sem_push(node);
}

/**
 * @brief Wraps an inline body in a block before attaching it to its parent.
 * @param depth Position of the body below the top of the builder stack.
 * @return SEM_OK, or an internal error if the body cannot be wrapped.
 */
static sem_result_t sem_wrap_body(size_t depth) {
	if (depth >= builder.count)
		return sem_builder_fail("body is missing");

	const size_t index = builder.count - depth - 1;
	ast_node_t *body = builder.nodes[index];
	if (body->kind == AST_NODE_BLOCK)
		return SEM_OK;

	ast_node_t *block = NULL;
	SEM_TRY(sem_new_node(AST_NODE_BLOCK, NULL, body->location, &block));
	if (ast_node_add_child(block, body) != AST_RESULT_OK) {
		ast_node_destroy(&block);
		return sem_builder_fail("cannot wrap an inline body");
	}
	builder.nodes[index] = block;
	return SEM_OK;
}

// literals: the lexer gives the exact source spelling, here it is turned into the value

/**
 * @brief copies a number without the underscores and without the d or D suffix of a Double
 *
 * @param token integer or double token
 * @return newly allocated digits for strtoll or strtod, NULL when out of memory
 */
static char *sem_number_digits(const token_t *token) {
	char *digits = malloc(token->length + 1);
	if (digits == NULL)
		return NULL;
	size_t length = 0;
	for (size_t i = 0; i < token->length; i++) {
		const char character = token->lexeme[i];
		if (character != '_' && character != 'd' && character != 'D')
			digits[length++] = character;
	}
	digits[length] = '\0';
	return digits;
}

/**
 * @brief returns the value of a string literal without the quotes
 *
 * escape sequences are replaced in an ordinary string, a multiline string is taken as it is
 *
 * @param token string or multiline string token
 * @return newly allocated value, NULL when out of memory or the lexeme is malformed
 */
static char *sem_string_value(const token_t *token) {
	const bool multiline = token->type == TOKEN_MULTILINE_STRING;
	const size_t quotes = multiline ? 3 : 1;
	if (token->lexeme == NULL || token->length < 2 * quotes)
		return NULL;

	const char *body = token->lexeme + quotes;
	const size_t body_length = token->length - 2 * quotes;
	char *value = malloc(body_length + 1);
	if (value == NULL)
		return NULL;

	size_t length = 0;
	for (size_t i = 0; i < body_length; i++) {
		char character = body[i];
		if (!multiline && character == '\\' && i + 1 < body_length) {
			// the lexer allows only these five escape sequences
			switch (body[++i]) {
			case 'n':
				character = '\n';
				break;
			case 'r':
				character = '\r';
				break;
			case 't':
				character = '\t';
				break;
			default: // backslash or quote stand for themselves
				character = body[i];
				break;
			}
		}
		value[length++] = character;
	}
	value[length] = '\0';
	return value;
}

/**
 * @brief pushes an operand, an identifier or a literal with its value
 *
 * @param token identifier or literal token
 * @return SEM_OK, or SEM_ERR_INTERNAL on error
 */
static sem_result_t sem_action_primary(const token_t *token) {
	if (token == NULL || token->lexeme == NULL)
		return sem_builder_fail("operand without a token");

	const ast_location_t location = sem_token_location(token);
	ast_node_t *node = NULL;

	switch (token->type) {
	case TOKEN_IDENTIFIER:
		SEM_TRY(sem_new_node(AST_NODE_IDENTIFIER, token->lexeme, location, &node));
		break;
	case TOKEN_TRUE:
	case TOKEN_FALSE:
		SEM_TRY(sem_new_node(AST_NODE_BOOLEAN, NULL, location, &node));
		node->value.boolean = token->type == TOKEN_TRUE;
		break;
	case TOKEN_INTEGER:
	case TOKEN_DOUBLE: {
		char *digits = sem_number_digits(token);
		if (digits == NULL)
			return sem_builder_fail("out of memory");
		const bool is_integer = token->type == TOKEN_INTEGER;
		const sem_result_t result =
		    sem_new_node(is_integer ? AST_NODE_INTEGER : AST_NODE_DOUBLE, NULL, location, &node);
		if (result == SEM_OK) {
			// the range is not checked, see section 1.1 of the specification
			if (is_integer)
				node->value.integer = strtoll(digits, NULL, 10);
			else
				node->value.decimal = strtod(digits, NULL);
		}
		free(digits);
		SEM_TRY(result);
		break;
	}
	case TOKEN_STRING:
	case TOKEN_MULTILINE_STRING: {
		char *value = sem_string_value(token);
		if (value == NULL)
			return sem_builder_fail("cannot read a string literal");
		const sem_result_t result = sem_new_node(AST_NODE_STRING, value, location, &node);
		free(value);
		SEM_TRY(result);
		break;
	}
	default:
		return sem_builder_fail("unknown kind of operand");
	}
	return sem_push(node);
}

/**
 * @brief builds an operator node from its operands on the stack
 *
 * @param token operator token
 * @param unary true for a prefix operator with one operand, false for a binary one
 * @return SEM_OK, or SEM_ERR_INTERNAL on error
 */
static sem_result_t sem_action_operator(const token_t *token, bool unary) {
	if (token == NULL)
		return sem_builder_fail("operator without a token");

	ast_node_kind_t kind = AST_NODE_INVALID;
	switch (token->type) {
	case TOKEN_PLUS:
		kind = unary ? AST_NODE_POSITIVE : AST_NODE_ADD;
		break;
	case TOKEN_MINUS:
		kind = unary ? AST_NODE_NEGATIVE : AST_NODE_SUBTRACT;
		break;
	case TOKEN_NOT:
		kind = AST_NODE_NOT;
		break;
	case TOKEN_MULTIPLY:
		kind = AST_NODE_MULTIPLY;
		break;
	case TOKEN_DIVIDE:
		kind = AST_NODE_DIVIDE;
		break;
	case TOKEN_CONCAT:
		kind = AST_NODE_CONCATENATE;
		break;
	case TOKEN_LESS:
		kind = AST_NODE_LESS;
		break;
	case TOKEN_LESS_EQUAL:
		kind = AST_NODE_LESS_EQUAL;
		break;
	case TOKEN_GREATER:
		kind = AST_NODE_GREATER;
		break;
	case TOKEN_GREATER_EQUAL:
		kind = AST_NODE_GREATER_EQUAL;
		break;
	case TOKEN_EQUAL:
		kind = AST_NODE_EQUAL;
		break;
	case TOKEN_NOT_EQUAL:
		kind = AST_NODE_NOT_EQUAL;
		break;
	case TOKEN_AND:
		kind = AST_NODE_AND;
		break;
	case TOKEN_OR:
		kind = AST_NODE_OR;
		break;
	default:
		// ranges inside expressions belong to LISTS, which is not implemented
		return sem_builder_fail("unknown operator");
	}
	if (unary != (kind == AST_NODE_POSITIVE || kind == AST_NODE_NEGATIVE || kind == AST_NODE_NOT))
		return sem_builder_fail("operator used with a wrong number of operands");
	return sem_reduce(kind, NULL, token, unary ? 1 : 2);
}

/**
 * @brief builds a call node, name(arguments), the callee and the arguments are on the stack
 *
 * only a plain name can be called (a name in parentheses is still a name), anything else like
 * f(1)(2) is reported as a syntax error once the program is parsed
 *
 * @param argument_count number of arguments on top of the callee
 * @return SEM_OK, or SEM_ERR_INTERNAL on error
 */
static sem_result_t sem_action_call(size_t argument_count) {
	ast_node_t *callee = sem_top(argument_count);
	if (callee == NULL)
		return sem_builder_fail("call without a callee");

	if (callee->kind != AST_NODE_IDENTIFIER) {
		sem_builder_defer(SEM_ERR_SYNTAX);
		// the arguments are dropped and the callee stays as a placeholder of the call
		for (size_t i = 0; i < argument_count; i++)
			sem_drop_top();
		return SEM_OK;
	}

	ast_node_t *call = NULL;
	SEM_TRY(sem_new_node(AST_NODE_CALL, callee->text, callee->location, &call));
	sem_result_t result = sem_adopt(call, argument_count);
	if (result != SEM_OK) {
		ast_node_destroy(&call);
		return result;
	}
	sem_drop_top(); // the callee, its name is in the call now
	return sem_push(call);
}

/**
 * @brief builds value.member as a call of the built-in ".member" with the value as its only argument
 *
 * @param token member name token
 * @return SEM_OK, or SEM_ERR_INTERNAL on error
 */
static sem_result_t sem_action_member(const token_t *token) {
	if (token == NULL || token->lexeme == NULL)
		return sem_builder_fail("member without a name");

	char *name = malloc(token->length + 2);
	if (name == NULL)
		return sem_builder_fail("out of memory");
	name[0] = '.';
	memcpy(name + 1, token->lexeme, token->length + 1);

	const sem_result_t result = sem_reduce(AST_NODE_CALL, name, token, 1);
	free(name);
	return result;
}

/**
 * @brief sets a type on the parameter, variable or function it was written for
 *
 * @param token type token
 * @return SEM_OK, or SEM_ERR_INTERNAL when the type belongs to nothing
 */
static sem_result_t sem_action_type(const token_t *token) {
	if (token == NULL)
		return sem_builder_fail("type without a token");

	symt_data_type_t type = SYMT_TYPE_UNDEFINED;
	switch (token->type) {
	case TOKEN_TYPE_INT:
		type = SYMT_TYPE_INTEGER;
		break;
	case TOKEN_TYPE_DOUBLE:
		type = SYMT_TYPE_DOUBLE;
		break;
	case TOKEN_TYPE_STRING:
		type = SYMT_TYPE_STRING;
		break;
	case TOKEN_TYPE_UNIT:
		type = SYMT_TYPE_UNIT;
		break;
	case TOKEN_TYPE_BOOLEAN:
		type = SYMT_TYPE_BOOLEAN;
		break;
	default:
		// List[T] belongs to LISTS, which is not implemented
		return sem_builder_fail("unknown type");
	}

	if (builder.type_target != NULL) {
		builder.type_target->data_type = type;
		builder.type_target = NULL;
	} else if (builder.in_header && sem_top(0) != NULL) {
		sem_top(0)->data_type = type; // return type of the function
		builder.in_header = false;
	} else {
		return sem_builder_fail("type without a parameter, variable or function");
	}
	return SEM_OK;
}

/**
 * @brief tells if a node is a function definition, @main included
 *
 * @param node node or NULL
 * @return true for a function node
 */
static bool sem_is_function(const ast_node_t *node) {
	return node != NULL && (node->kind == AST_NODE_FUNCTION || node->kind == AST_NODE_MAIN_FUNCTION);
}

/**
 * @brief adds a parameter to the function on top of the stack
 *
 * @param token parameter name token
 * @return SEM_OK, or SEM_ERR_INTERNAL outside of a function header
 */
static sem_result_t sem_action_parameter(const token_t *token) {
	ast_node_t *function = sem_top(0);
	if (token == NULL || !builder.in_header || !sem_is_function(function))
		return sem_builder_fail("parameter outside of a function header");

	ast_node_t *parameter = NULL;
	SEM_TRY(sem_new_node(AST_NODE_PARAMETER, token->lexeme, sem_token_location(token), &parameter));
	// the parameter is complete except for its type, which may be set after attaching
	if (ast_node_add_child(function, parameter) != AST_RESULT_OK) {
		ast_node_destroy(&parameter);
		return sem_builder_fail("cannot attach an AST node");
	}
	builder.type_target = parameter;
	return SEM_OK;
}

/**
 * @brief builds name = value, the target name and the value are on the stack
 *
 * @return SEM_OK, or SEM_ERR_INTERNAL on error
 */
static sem_result_t sem_action_assignment(void) {
	ast_node_t *target = sem_top(1);
	if (target == NULL || target->kind != AST_NODE_IDENTIFIER)
		return sem_builder_fail("assignment without a target");
	return sem_reduce(AST_NODE_ASSIGNMENT, NULL, NULL, 2);
}

/**
 * @brief starts a for range, the range node takes its start now and the rest when the range ends
 *
 * @param token the to or until token
 * @return SEM_OK, or SEM_ERR_INTERNAL on error
 */
static sem_result_t sem_action_range_begin(const token_t *token) {
	if (token == NULL || token->lexeme == NULL)
		return sem_builder_fail("range without to or until");
	const bool until = strcmp(token->lexeme, "until") == 0;
	return sem_reduce(until ? AST_NODE_RANGE_UNTIL : AST_NODE_RANGE_TO, NULL, token, 1);
}

/**
 * @brief finishes a for range and attaches it to the for variable
 *
 * @param part_count 1 for the end only, 2 for the end and the by step
 * @return SEM_OK, or SEM_ERR_INTERNAL on error
 */
static sem_result_t sem_action_range(size_t part_count) {
	ast_node_t *range = sem_top(part_count);
	ast_node_t *binding = sem_top(part_count + 1);
	if (range == NULL || (range->kind != AST_NODE_RANGE_TO && range->kind != AST_NODE_RANGE_UNTIL) ||
	    binding == NULL || binding->kind != AST_NODE_FOR_BINDING)
		return sem_builder_fail("range outside of a for loop");

	SEM_TRY(sem_adopt(range, part_count));
	return sem_adopt(binding, 1);
}

/**
 * @brief puts all definitions under a program node and runs the semantic analysis on it
 *
 * @return SEM_OK, or the exit code of the first error
 */
static sem_result_t sem_action_program(void) {
	// printed only now, because a later syntax error would have won
	if (builder.pending == SEM_ERR_SYNTAX)
		fprintf(stderr, "syntax error: only a function name can be called.\n");
	if (builder.pending != SEM_OK)
		return builder.pending;

	ast_node_t *program = NULL;
	ast_node_t *import = NULL;
	SEM_TRY(sem_new_node(AST_NODE_PROGRAM, NULL, (ast_location_t){.line = 1, .column = 1}, &program));
	sem_result_t result = sem_new_node(AST_NODE_IMPORT, NULL, program->location, &import);
	if (result == SEM_OK && ast_node_add_child(program, import) != AST_RESULT_OK) {
		ast_node_destroy(&import);
		result = sem_builder_fail("cannot attach an AST node");
	}
	if (result == SEM_OK)
		result = sem_adopt(program, builder.count);
	if (result == SEM_OK &&
	    (ast_create(&builder.ast) != AST_RESULT_OK || ast_set_root(builder.ast, program) != AST_RESULT_OK))
		result = sem_builder_fail("cannot create the AST");
	if (result != SEM_OK) {
		ast_node_destroy(&program);
		return result;
	}

	if (funct_init(&builder.functions) != SYMTSTAT_OK)
		return sem_builder_fail("cannot create the function table");
	return sem_analyze(builder.ast, builder.functions);
}

/**
 * @brief handles one event from the parser
 *
 * @param action what was recognized
 * @param token token of it or NULL
 * @param item_count see syntax_semantic_action_t
 * @return SEM_OK, or the exit code of an error
 */
static sem_result_t sem_dispatch(syntax_semantic_action_t action, const token_t *token, size_t item_count) {
	// a TYPE right after a parameter, a variable name or a function header belongs to it
	if (action != SEMANTIC_ACTION_TYPE) {
		builder.type_target = NULL;
		if (action != SEMANTIC_ACTION_PARAMETER)
			builder.in_header = false;
	}

	switch (action) {
	case SEMANTIC_ACTION_PROGRAM_BEGIN:
		sem_actions_free();
		return SEM_OK;
	case SEMANTIC_ACTION_PROGRAM:
		return sem_action_program();

	// expressions
	case SEMANTIC_ACTION_PRIMARY:
	case SEMANTIC_ACTION_ASSIGNMENT_TARGET: // an identifier until an ASSIGNMENT makes it a target
		return sem_action_primary(token);
	case SEMANTIC_ACTION_UNIT: {
		ast_node_t *unit = NULL;
		SEM_TRY(sem_new_node(AST_NODE_UNIT, NULL, sem_token_location(token), &unit));
		return sem_push(unit);
	}
	case SEMANTIC_ACTION_UNARY:
		return sem_action_operator(token, true);
	case SEMANTIC_ACTION_BINARY:
		return sem_action_operator(token, false);
	case SEMANTIC_ACTION_MEMBER:
		return sem_action_member(token);
	case SEMANTIC_ACTION_CALL:
		return sem_action_call(item_count);
	case SEMANTIC_ACTION_IF:
		if (item_count != 2 && item_count != 3)
			return sem_builder_fail("if with a wrong number of parts");
		for (size_t depth = 0; depth < item_count - 1; depth++)
			SEM_TRY(sem_wrap_body(depth));
		return sem_reduce(AST_NODE_IF, NULL, NULL, item_count);

	// functions
	case SEMANTIC_ACTION_FUNCTION_BEGIN:
		builder.function_kind = item_count != 0 ? AST_NODE_MAIN_FUNCTION : AST_NODE_FUNCTION;
		return SEM_OK;
	case SEMANTIC_ACTION_FUNCTION_NAME: {
		if (token == NULL)
			return sem_builder_fail("function without a name");
		ast_node_t *function = NULL;
		SEM_TRY(sem_new_node(builder.function_kind, token->lexeme, sem_token_location(token), &function));
		SEM_TRY(sem_push(function));
		builder.in_header = true;
		return SEM_OK;
	}
	case SEMANTIC_ACTION_PARAMETER:
		return sem_action_parameter(token);
	case SEMANTIC_ACTION_TYPE:
		return sem_action_type(token);
	case SEMANTIC_ACTION_FUNCTION:
		// the body is on top and the function with its parameters right below it
		if (!sem_is_function(sem_top(1)))
			return sem_builder_fail("function body without a function");
		SEM_TRY(sem_wrap_body(0));
		return sem_adopt(sem_top(1), 1);

	// statements
	case SEMANTIC_ACTION_DECLARATION_BEGIN:
		if (token == NULL)
			return sem_builder_fail("declaration without val or var");
		builder.declaration_kind =
		    token->type == TOKEN_VAR ? AST_NODE_VAR_DECLARATION : AST_NODE_VAL_DECLARATION;
		return SEM_OK;
	case SEMANTIC_ACTION_DECLARATION_NAME: {
		if (token == NULL)
			return sem_builder_fail("declaration without a name");
		ast_node_t *declaration = NULL;
		SEM_TRY(
		    sem_new_node(builder.declaration_kind, token->lexeme, sem_token_location(token), &declaration));
		SEM_TRY(sem_push(declaration));
		builder.type_target = declaration;
		return SEM_OK;
	}
	case SEMANTIC_ACTION_DECLARATION: {
		// the initializer is on top and the declaration right below it
		ast_node_t *declaration = sem_top(1);
		if (declaration == NULL ||
		    (declaration->kind != AST_NODE_VAL_DECLARATION && declaration->kind != AST_NODE_VAR_DECLARATION))
			return sem_builder_fail("initializer without a declaration");
		return sem_adopt(declaration, 1);
	}
	case SEMANTIC_ACTION_ASSIGNMENT:
		return sem_action_assignment();
	case SEMANTIC_ACTION_RETURN:
		if (item_count > 1)
			return sem_builder_fail("return with more than one value");
		return sem_reduce(AST_NODE_RETURN, NULL, NULL, item_count);
	case SEMANTIC_ACTION_WHILE:
		SEM_TRY(sem_wrap_body(0));
		return sem_reduce(AST_NODE_WHILE, NULL, NULL, 2);
	case SEMANTIC_ACTION_FOR_VARIABLE: {
		if (token == NULL)
			return sem_builder_fail("for without a variable");
		ast_node_t *binding = NULL;
		SEM_TRY(sem_new_node(AST_NODE_FOR_BINDING, token->lexeme, sem_token_location(token), &binding));
		return sem_push(binding);
	}
	case SEMANTIC_ACTION_RANGE_BEGIN:
		return sem_action_range_begin(token);
	case SEMANTIC_ACTION_RANGE:
		if (item_count != 1 && item_count != 2)
			return sem_builder_fail("range with a wrong number of parts");
		return sem_action_range(item_count);
	case SEMANTIC_ACTION_FOR:
		// generator, guards and body
		SEM_TRY(sem_wrap_body(0));
		return sem_reduce(AST_NODE_FOR, NULL, NULL, item_count + 2);

	// blocks
	case SEMANTIC_ACTION_BLOCK_BEGIN:
		if (builder.block_count == builder.block_capacity) {
			size_t new_capacity = builder.block_capacity == 0 ? 16 : 2 * builder.block_capacity;
			size_t *new_blocks = realloc(builder.blocks, new_capacity * sizeof(*new_blocks));
			if (new_blocks == NULL)
				return sem_builder_fail("out of memory");
			builder.blocks = new_blocks;
			builder.block_capacity = new_capacity;
		}
		builder.blocks[builder.block_count++] = builder.count;
		return SEM_OK;
	case SEMANTIC_ACTION_BLOCK: {
		if (builder.block_count == 0 || builder.blocks[builder.block_count - 1] > builder.count)
			return sem_builder_fail("end of a block that was not opened");
		const size_t statement_count = builder.count - builder.blocks[--builder.block_count];
		return sem_reduce(AST_NODE_BLOCK, NULL, NULL, statement_count);
	}
	}
	return sem_builder_fail("unknown semantic action");
}

sem_result_t syntax_semantic_action(syntax_semantic_action_t action, const token_t *token,
                                    size_t item_count) {
	builder.result = sem_dispatch(action, token, item_count);
	return builder.result;
}
