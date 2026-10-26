#include "include/syntax_anal.h"

#include <string.h>

bool syntax_parser_init(syntax_parser_t *parser, lexer_t *lexer) {
	if (parser == NULL || lexer == NULL || lexer_current(lexer) == NULL || lexer_next(lexer) == NULL) {
		return false;
	}

	*parser = (syntax_parser_t){.lexer = lexer};
	return true;
}

const token_t *syntax_parser_current(const syntax_parser_t *parser) {
	if (parser == NULL || parser->failed || parser->lexer == NULL) {
		return NULL;
	}
	return lexer_current(parser->lexer);
}

const token_t *syntax_parser_next(const syntax_parser_t *parser) {
	if (parser == NULL || parser->failed || parser->lexer == NULL) {
		return NULL;
	}
	return lexer_next(parser->lexer);
}

bool syntax_parser_advance(syntax_parser_t *parser) {
	if (parser == NULL || parser->failed || parser->lexer == NULL ||
	    lexer_advance(parser->lexer) != LEXER_RESULT_OK) {
		if (parser != NULL) {
			parser->failed = true;
		}
		return false;
	}
	return true;
}

bool syntax_parser_expect(syntax_parser_t *parser, token_type_t expected) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL || token->type != expected) {
		if (parser != NULL) {
			parser->failed = true;
		}
		return false;
	}
	return syntax_parser_advance(parser);
}

bool syntax_parser_accept(syntax_parser_t *parser, token_type_t expected, bool *accepted) {
	if (parser == NULL || accepted == NULL || parser->failed) {
		return false;
	}
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		parser->failed = true;
		return false;
	}
	*accepted = token->type == expected;
	return !*accepted || syntax_parser_advance(parser);
}

/** Parse an expression from minimum_precedence upward. */
static bool parse_expression_prec(syntax_parser_t *parser, int minimum_precedence);

/** Parse a literal, identifier, Unit literal, or parenthesized expression. */
static bool parse_primary(syntax_parser_t *parser) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		if (parser != NULL) {
			parser->failed = true;
		}
		return false;
	}

	switch (token->type) {
	case TOKEN_IDENTIFIER:
	case TOKEN_INTEGER:
	case TOKEN_DOUBLE:
	case TOKEN_STRING:
	case TOKEN_MULTILINE_STRING:
	case TOKEN_TRUE:
	case TOKEN_FALSE: {
		token_t recognized = *token;
		syntax_semantic_action(SEMANTIC_ACTION_PRIMARY, &recognized, 0);
		return syntax_parser_advance(parser);
	}
	case TOKEN_LPAREN: {
		const token_t *next = syntax_parser_next(parser);
		if (next != NULL && next->type == TOKEN_RPAREN) {
			token_t unit = *token;
			syntax_semantic_action(SEMANTIC_ACTION_UNIT, &unit, 0);
			if (!syntax_parser_expect(parser, TOKEN_LPAREN) || !syntax_parser_expect(parser, TOKEN_RPAREN)) {
				return false;
			}
			return true;
		}
		if (!syntax_parser_expect(parser, TOKEN_LPAREN) || !syntax_parse_expression(parser) ||
		    !syntax_parser_expect(parser, TOKEN_RPAREN)) {
			return false;
		}
		return true;
	}
	default:
		parser->failed = true;
		return false;
	}
}

/** Parse a primary and any following member-access or call suffixes. */
static bool parse_postfix(syntax_parser_t *parser) {
	if (!parse_primary(parser)) {
		return false;
	}
	for (;;) {
		const token_t *token = syntax_parser_current(parser);
		if (token == NULL) {
			parser->failed = true;
			return false;
		}
		if (token->type == TOKEN_DOT) {
			if (!syntax_parser_expect(parser, TOKEN_DOT)) {
				return false;
			}
			const token_t *member = syntax_parser_current(parser);
			if (member == NULL || member->type != TOKEN_IDENTIFIER) {
				parser->failed = true;
				return false;
			}
			token_t recognized = *member;
			syntax_semantic_action(SEMANTIC_ACTION_MEMBER, &recognized, 0);
			if (!syntax_parser_advance(parser)) {
				return false;
			}
			continue;
		}
		if (token->type == TOKEN_LPAREN) {
			token_t call_token = {.type = token->type, .location = token->location};
			size_t argument_count = 0;
			if (!syntax_parser_expect(parser, TOKEN_LPAREN)) {
				return false;
			}
			const token_t *argument_start = syntax_parser_current(parser);
			if (argument_start == NULL) {
				parser->failed = true;
				return false;
			}
			if (argument_start->type != TOKEN_RPAREN) {
				for (;;) {
					if (!syntax_parse_expression(parser)) {
						return false;
					}
					argument_count++;
					bool comma = false;
					if (!syntax_parser_accept(parser, TOKEN_COMMA, &comma)) {
						return false;
					}
					if (!comma) {
						break;
					}
				}
			}
			if (!syntax_parser_expect(parser, TOKEN_RPAREN)) {
				return false;
			}
			syntax_semantic_action(SEMANTIC_ACTION_CALL, &call_token, argument_count);
			continue;
		}
		return true;
	}
}

/** Return whether type can begin a prefix expression. */
static bool unary_operator(token_type_t type) {
	return type == TOKEN_PLUS || type == TOKEN_MINUS || type == TOKEN_NOT;
}

/** Parse recursive unary operators before a postfix expression. */
static bool parse_prefix(syntax_parser_t *parser) {
	const token_t *token = syntax_parser_current(parser);
	if (token != NULL && unary_operator(token->type)) {
		token_t operator_token = {.type = token->type, .location = token->location};
		if (!syntax_parser_advance(parser) || !parse_prefix(parser)) {
			return false;
		}
		syntax_semantic_action(SEMANTIC_ACTION_UNARY, &operator_token, 0);
		return true;
	}
	return parse_postfix(parser);
}

/** Map a binary operator to precedence-climbing rank; larger ranks bind tighter. */
static int binary_precedence(token_type_t type) {
	switch (type) {
	case TOKEN_MULTIPLY:
	case TOKEN_DIVIDE:
		return 6;
	case TOKEN_PLUS:
	case TOKEN_MINUS:
	case TOKEN_CONCAT:
		return 5;
	case TOKEN_LESS:
	case TOKEN_LESS_EQUAL:
	case TOKEN_GREATER:
	case TOKEN_GREATER_EQUAL:
		return 4;
	case TOKEN_EQUAL:
	case TOKEN_NOT_EQUAL:
		return 3;
	case TOKEN_AND:
		return 2;
	case TOKEN_OR:
		return 1;
	default:
		return 0;
	}
}

/** Identify relational and equality groups, which cannot chain unparenthesized. */
static int non_associative_group(token_type_t type) {
	if (type == TOKEN_LESS || type == TOKEN_LESS_EQUAL || type == TOKEN_GREATER || type == TOKEN_GREATER_EQUAL) {
		return 1;
	}
	if (type == TOKEN_EQUAL || type == TOKEN_NOT_EQUAL) {
		return 2;
	}
	return 0;
}

/** Parse the left-associative binary expression at or above the requested rank. */
static bool parse_expression_prec(syntax_parser_t *parser, int minimum_precedence) {
	if (!parse_prefix(parser)) {
		return false;
	}
	int previous_non_associative_group = 0;
	for (;;) {
		const token_t *token = syntax_parser_current(parser);
		if (token == NULL) {
			parser->failed = true;
			return false;
		}
		int precedence = binary_precedence(token->type);
		if (precedence == 0 || precedence < minimum_precedence) {
			return true;
		}
		int group = non_associative_group(token->type);
		if (group != 0 && group == previous_non_associative_group) {
			parser->failed = true;
			return false;
		}

		token_t operator_token = {.type = token->type, .location = token->location};
		if (!syntax_parser_advance(parser) || !parse_expression_prec(parser, precedence + 1)) {
			return false;
		}
		syntax_semantic_action(SEMANTIC_ACTION_BINARY, &operator_token, 0);
		previous_non_associative_group = group;
	}
}

/** Public expression entry point; delimiters are left for the caller to consume. */
bool syntax_parse_expression(syntax_parser_t *parser) {
	if (parser == NULL || parser->failed) {
		return false;
	}
	return parse_expression_prec(parser, 1);
}

/** Check whether the current identifier has the requested keyword-like spelling. */
static bool identifier_is(const syntax_parser_t *parser, const char *text) {
	const token_t *token = syntax_parser_current(parser);
	return token != NULL && token->type == TOKEN_IDENTIFIER && token->lexeme != NULL &&
	       token->length == strlen(text) && memcmp(token->lexeme, text, token->length) == 0;
}

/** Return whether type can begin an expression supported by this parser. */
static bool expression_start(token_type_t type) {
	switch (type) {
	case TOKEN_IDENTIFIER:
	case TOKEN_INTEGER:
	case TOKEN_DOUBLE:
	case TOKEN_STRING:
	case TOKEN_MULTILINE_STRING:
	case TOKEN_TRUE:
	case TOKEN_FALSE:
	case TOKEN_LPAREN:
	case TOKEN_PLUS:
	case TOKEN_MINUS:
	case TOKEN_NOT:
		return true;
	default:
		return false;
	}
}

/** Send a grammar-level action to the semantic analyzer's placeholder dispatcher. */
static void semantic_action_construct(syntax_semantic_action_t action, const token_t *token) {
	syntax_semantic_action(action, token, 0);
}

/** Parse a primitive type and notify semantic analysis of the token. */
static bool parse_type(syntax_parser_t *parser) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		parser->failed = true;
		return false;
	}
	switch (token->type) {
	case TOKEN_TYPE_INT:
	case TOKEN_TYPE_BOOLEAN:
	case TOKEN_TYPE_DOUBLE:
	case TOKEN_TYPE_STRING:
	case TOKEN_TYPE_UNIT:
		syntax_semantic_action(SEMANTIC_ACTION_TYPE, token, 0);
		return syntax_parser_advance(parser);
	default:
		parser->failed = true;
		return false;
	}
}

/** Parse the optional comma-separated remainder of a parameter list. */
static bool parse_params_tail(syntax_parser_t *parser);

/** Parse an optional first parameter and its remaining parameters. */
static bool parse_param_list(syntax_parser_t *parser) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		parser->failed = true;
		return false;
	}
	if (token->type == TOKEN_RPAREN) {
		return true;
	}
	if (token->type != TOKEN_IDENTIFIER) {
		parser->failed = true;
		return false;
	}
	syntax_semantic_action(SEMANTIC_ACTION_PARAMETER, token, 0);
	if (!syntax_parser_advance(parser) || !syntax_parser_expect(parser, TOKEN_COLON) || !parse_type(parser)) {
		return false;
	}
	return parse_params_tail(parser);
}

static bool parse_params_tail(syntax_parser_t *parser) {
	bool comma = false;
	if (!syntax_parser_accept(parser, TOKEN_COMMA, &comma)) {
		return false;
	}
	if (!comma) {
		return true;
	}
	const token_t *token = syntax_parser_current(parser);
	if (token != NULL && token->type == TOKEN_RPAREN) {
		return true;
	}
	if (token == NULL || token->type != TOKEN_IDENTIFIER) {
		parser->failed = true;
		return false;
	}
	syntax_semantic_action(SEMANTIC_ACTION_PARAMETER, token, 0);
	if (!syntax_parser_advance(parser) || !syntax_parser_expect(parser, TOKEN_COLON) || !parse_type(parser)) {
		return false;
	}
	return parse_params_tail(parser);
}

/** Parse an optional colon and type annotation. */
static bool parse_type_opt(syntax_parser_t *parser) {
	bool colon = false;
	return syntax_parser_accept(parser, TOKEN_COLON, &colon) && (!colon || parse_type(parser));
}

/** Consume a statement terminator (newline or semicolon). */
static bool parse_stmt_end(syntax_parser_t *parser) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		parser->failed = true;
		return false;
	}
	if (token->type == TOKEN_NEWLINE) {
		return syntax_parser_advance(parser);
	}
	if (token->type == TOKEN_SEMICOLON) {
		return syntax_parser_advance(parser);
	}
	parser->failed = true;
	return false;
}

/** Forward declarations for mutually recursive statement and value rules. */
static bool parse_if(syntax_parser_t *parser);
static bool parse_block(syntax_parser_t *parser);
static bool parse_value(syntax_parser_t *parser);
static bool parse_statement(syntax_parser_t *parser);

/** Parse a function body as either an indented block or an inline value. */
static bool parse_body(syntax_parser_t *parser) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		parser->failed = true;
		return false;
	}
	if (token->type == TOKEN_NEWLINE) {
		return syntax_parser_advance(parser) && parse_block(parser);
	}
	return parse_value(parser);
}

/** Parse a non-empty INDENT...DEDENT statement block. */
static bool parse_block(syntax_parser_t *parser) {
	const token_t *indent = syntax_parser_current(parser);
	if (indent != NULL) {
		syntax_semantic_action(SEMANTIC_ACTION_BLOCK_BEGIN, indent, 0);
	}
	if (!syntax_parser_expect(parser, TOKEN_INDENT)) {
		return false;
	}
	bool has_statement = false;
	for (;;) {
		const token_t *token = syntax_parser_current(parser);
		if (token == NULL) {
			parser->failed = true;
			return false;
		}
		if (token->type == TOKEN_DEDENT) {
			if (!has_statement) {
				parser->failed = true;
				return false;
			}
			syntax_semantic_action(SEMANTIC_ACTION_BLOCK, token, 0);
			if (!syntax_parser_advance(parser)) {
				return false;
			}
			return true;
		}
		if (!parse_statement(parser)) {
			return false;
		}
		has_statement = true;
	}
}

/** Parse the block, nested if, or inline value after ELSE. */
static bool parse_else_body(syntax_parser_t *parser) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		parser->failed = true;
		return false;
	}
	if (token->type == TOKEN_NEWLINE) {
		return syntax_parser_advance(parser) && parse_block(parser);
	}
	if (token->type == TOKEN_IF) {
		return parse_if(parser);
	}
	if (!syntax_parse_expression(parser) || !parse_stmt_end(parser)) {
		return false;
	}
	return true;
}

/** Parse an optional inline ELSE branch or the inline statement terminator. */
static bool parse_else_line(syntax_parser_t *parser) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		parser->failed = true;
		return false;
	}
	if (token->type == TOKEN_ELSE) {
		return syntax_parser_advance(parser) && parse_else_body(parser);
	}
	return parse_stmt_end(parser);
}

/** Parse the optional ELSE branch following a block-style THEN branch. */
static bool parse_else_block(syntax_parser_t *parser) {
	bool has_else = false;
	if (!syntax_parser_accept(parser, TOKEN_ELSE, &has_else)) {
		return false;
	}
	return !has_else || parse_else_body(parser);
}

/** Parse either block-style or inline content following THEN. */
static bool parse_then_tail(syntax_parser_t *parser) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		parser->failed = true;
		return false;
	}
	if (token->type == TOKEN_NEWLINE) {
		return syntax_parser_advance(parser) && parse_block(parser) && parse_else_block(parser);
	}
	if (!syntax_parse_expression(parser) || !parse_else_line(parser)) {
		return false;
	}
	return true;
}

/** Parse an IF condition and its THEN/optional ELSE content. */
static bool parse_if(syntax_parser_t *parser) {
	if (!syntax_parser_expect(parser, TOKEN_IF) || !syntax_parse_expression(parser) ||
	    !syntax_parser_expect(parser, TOKEN_THEN) || !parse_then_tail(parser)) {
		return false;
	}
	semantic_action_construct(SEMANTIC_ACTION_IF, NULL);
	return true;
}

/** Parse a value: an expression with terminator, or an IF expression. */
static bool parse_value(syntax_parser_t *parser) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		parser->failed = true;
		return false;
	}
	if (token->type == TOKEN_IF) {
		return parse_if(parser);
	}
	return syntax_parse_expression(parser) && parse_stmt_end(parser);
}

/** Parse a for-generator range and optional step. */
static bool parse_generator(syntax_parser_t *parser) {
	if (syntax_parser_current(parser) == NULL || syntax_parser_current(parser)->type != TOKEN_IDENTIFIER ||
	    !syntax_parser_advance(parser) || !syntax_parser_expect(parser, TOKEN_LEFT_ARROW) ||
	    !syntax_parse_expression(parser)) {
		parser->failed = true;
		return false;
	}
	if (!identifier_is(parser, "to") && !identifier_is(parser, "until")) {
		parser->failed = true;
		return false;
	}
	if (!syntax_parser_advance(parser) || !syntax_parse_expression(parser)) {
		return false;
	}
	if (identifier_is(parser, "by")) {
		if (!syntax_parser_advance(parser) || !syntax_parse_expression(parser)) {
			return false;
		}
	}
	return true;
}

/** Parse zero or more inline IF guards. */
static bool parse_guards(syntax_parser_t *parser) {
	for (;;) {
		bool is_if = false;
		if (!syntax_parser_accept(parser, TOKEN_IF, &is_if)) {
			return false;
		}
		if (!is_if) {
			return true;
		}
		if (!syntax_parse_expression(parser)) {
			return false;
		}
	}
}

/** Parse zero or more newline-terminated IF guards. */
static bool parse_guard_lines(syntax_parser_t *parser) {
	for (;;) {
		bool is_if = false;
		if (!syntax_parser_accept(parser, TOKEN_IF, &is_if)) {
			return false;
		}
		if (!is_if) {
			return true;
		}
		if (!syntax_parse_expression(parser) || !syntax_parser_expect(parser, TOKEN_NEWLINE)) {
			return false;
		}
	}
}

/** Parse either the inline or multiline form of a for-loop header. */
static bool parse_for_head(syntax_parser_t *parser) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		parser->failed = true;
		return false;
	}
	if (token->type == TOKEN_NEWLINE) {
		return syntax_parser_advance(parser) && syntax_parser_expect(parser, TOKEN_INDENT) &&
		       parse_generator(parser) && syntax_parser_expect(parser, TOKEN_NEWLINE) &&
		       parse_guard_lines(parser) && syntax_parser_expect(parser, TOKEN_DEDENT);
	}
	return parse_generator(parser) && parse_guards(parser);
}

/** Parse the block or inline expression following DO. */
static bool parse_do_body(syntax_parser_t *parser) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		parser->failed = true;
		return false;
	}
	if (token->type == TOKEN_NEWLINE) {
		return syntax_parser_advance(parser) && parse_block(parser);
	}
	return syntax_parse_expression(parser) && parse_stmt_end(parser);
}

/** Parse one declaration, assignment, control statement, or expression statement. */
static bool parse_statement(syntax_parser_t *parser) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		parser->failed = true;
		return false;
	}
	switch (token->type) {
	case TOKEN_VAL:
	case TOKEN_VAR: {
		const token_t *declaration = token;
		syntax_semantic_action(SEMANTIC_ACTION_DECLARATION_BEGIN, declaration, 0);
		if (!syntax_parser_advance(parser)) {
			parser->failed = true;
			return false;
		}
		const token_t *name = syntax_parser_current(parser);
		if (name == NULL || name->type != TOKEN_IDENTIFIER) {
			parser->failed = true;
			return false;
		}
		syntax_semantic_action(SEMANTIC_ACTION_DECLARATION_NAME, name, 0);
		if (!syntax_parser_advance(parser) || !parse_type_opt(parser) ||
		    !syntax_parser_expect(parser, TOKEN_ASSIGN) || !parse_value(parser)) {
			return false;
		}
		semantic_action_construct(SEMANTIC_ACTION_DECLARATION, NULL);
		return true;
	}
	case TOKEN_RETURN: {
		if (!syntax_parser_advance(parser)) {
			return false;
		}
		const token_t *next = syntax_parser_current(parser);
		if (next == NULL) {
			parser->failed = true;
			return false;
		}
		if (next->type == TOKEN_NEWLINE || next->type == TOKEN_SEMICOLON) {
			if (!parse_stmt_end(parser)) {
				return false;
			}
		} else if (!parse_value(parser)) {
			return false;
		}
		semantic_action_construct(SEMANTIC_ACTION_RETURN, NULL);
		return true;
	}
	case TOKEN_WHILE:
		if (!syntax_parser_advance(parser) || !syntax_parse_expression(parser) ||
		    !syntax_parser_expect(parser, TOKEN_DO) || !syntax_parser_expect(parser, TOKEN_NEWLINE) ||
		    !parse_block(parser)) {
			return false;
		}
		semantic_action_construct(SEMANTIC_ACTION_WHILE, NULL);
		return true;
	case TOKEN_FOR:
		if (!syntax_parser_advance(parser) || !parse_for_head(parser) || !syntax_parser_expect(parser, TOKEN_DO) ||
		    !parse_do_body(parser)) {
			return false;
		}
		semantic_action_construct(SEMANTIC_ACTION_FOR, NULL);
		return true;
	case TOKEN_IF:
		return parse_if(parser);
	case TOKEN_IDENTIFIER:
		if (syntax_parser_next(parser) != NULL && syntax_parser_next(parser)->type == TOKEN_ASSIGN) {
			syntax_semantic_action(SEMANTIC_ACTION_ASSIGNMENT_TARGET, token, 0);
			if (!syntax_parser_advance(parser) || !syntax_parser_expect(parser, TOKEN_ASSIGN) || !parse_value(parser)) {
				return false;
			}
			semantic_action_construct(SEMANTIC_ACTION_ASSIGNMENT, NULL);
			return true;
		}
		return syntax_parse_expression(parser) && parse_stmt_end(parser);
	default:
		if (!expression_start(token->type)) {
			parser->failed = true;
			return false;
		}
		return syntax_parse_expression(parser) && parse_stmt_end(parser);
	}
}

/** Parse one function definition, including optional @main and return type. */
static bool parse_definition(syntax_parser_t *parser) {
	bool is_main = false;
	if (!syntax_parser_accept(parser, TOKEN_MAIN_ANNOTATION, &is_main)) {
		return false;
	}
	if (is_main) {
		bool newline = false;
		if (!syntax_parser_accept(parser, TOKEN_NEWLINE, &newline)) {
			return false;
		}
	}
	const token_t *def_token = syntax_parser_current(parser);
	if (def_token == NULL || def_token->type != TOKEN_DEF) {
		parser->failed = true;
		return false;
	}
	syntax_semantic_action(SEMANTIC_ACTION_FUNCTION_BEGIN, def_token, 0);
	if (!syntax_parser_advance(parser)) {
		return false;
	}
	const token_t *function_name = syntax_parser_current(parser);
	if (function_name == NULL || function_name->type != TOKEN_IDENTIFIER) {
		parser->failed = true;
		return false;
	}
	syntax_semantic_action(SEMANTIC_ACTION_FUNCTION_NAME, function_name, 0);
	if (!syntax_parser_advance(parser) || !syntax_parser_expect(parser, TOKEN_LPAREN) || !parse_param_list(parser) ||
	    !syntax_parser_expect(parser, TOKEN_RPAREN)) {
		return false;
	}
	bool has_return_type = false;
	if (!syntax_parser_accept(parser, TOKEN_COLON, &has_return_type) || (has_return_type && !parse_type(parser)) ||
	    !syntax_parser_expect(parser, TOKEN_ASSIGN) || !parse_body(parser)) {
		return false;
	}
	semantic_action_construct(SEMANTIC_ACTION_FUNCTION, NULL);
	return true;
}

/** Parse the required import ifj26.* prolog. */
static bool parse_prolog(syntax_parser_t *parser) {
	if (!syntax_parser_expect(parser, TOKEN_IMPORT)) {
		return false;
	}
	const token_t *module = syntax_parser_current(parser);
	if (module == NULL || module->type != TOKEN_IDENTIFIER || module->lexeme == NULL || module->length != 5 ||
	    memcmp(module->lexeme, "ifj26", 5) != 0 || !syntax_parser_advance(parser)) {
		parser->failed = true;
		return false;
	}
	return syntax_parser_expect(parser, TOKEN_DOT) && syntax_parser_expect(parser, TOKEN_MULTIPLY) &&
	       syntax_parser_expect(parser, TOKEN_NEWLINE);
}

/** Parse a complete program: prolog, definitions, then EOF. */
bool syntax_parse_program(syntax_parser_t *parser) {
	if (parser == NULL || parser->failed || !parse_prolog(parser)) {
		return false;
	}
	for (;;) {
		const token_t *token = syntax_parser_current(parser);
		if (token == NULL) {
			parser->failed = true;
			return false;
		}
		if (token->type == TOKEN_EOF) {
			if (!syntax_parser_advance(parser)) {
				return false;
			}
			semantic_action_construct(SEMANTIC_ACTION_PROGRAM, NULL);
			return true;
		}
		if (token->type != TOKEN_DEF && token->type != TOKEN_MAIN_ANNOTATION) {
			parser->failed = true;
			return false;
		}
		if (!parse_definition(parser)) {
			return false;
		}
	}
}
