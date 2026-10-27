#include "include/syntax_anal.h"

#include <stdlib.h>
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

//
//	OPERATOR PRECEDENCE EXPRESSION PARSER
//

/**
 * @brief terminal groups of the precedence table, same order as rows and columns in the table
 */
typedef enum PrecTerminal {
	PREC_ID,     // identifier or literal, true and false too
	PREC_LIST,   // List as an operand, LISTS
	PREC_UNIT,   // unit literal, empty parens
	PREC_PREFIX, // unary minus plus and not
	PREC_DOT,    // dot and member name read as one symbol, VALMEM and LISTS
	PREC_CALL,   // whole argument list after an operand as one symbol, FUNEXP and LISTS
	PREC_MUL,    // multiply and divide
	PREC_ADD,    // binary plus minus and list concat
	PREC_REL,    // relational operators
	PREC_EQ,     // equal and not equal
	PREC_AND,    // logical and
	PREC_OR,     // logical or
	PREC_RANGE,  // to until and by after an operand, CONTROL and LISTS
	PREC_LPAREN, // grouping open paren
	PREC_RPAREN, // grouping close paren
	PREC_END,    // end symbol, any token that cant continue the expression
	PREC_TERMINAL_COUNT
} prec_terminal_t;

/**
 * @brief the precedence table, same as the Precedence matrix sheet in the revised table
 *
 * row is the topmost terminal on the stack, column is the incoming one
 * less than sign means shift and put a handle marker right after the topmost terminal
 * greater than sign means reduce the handle
 * equals sign means shift without a marker, only for matching parentheses
 * X is a syntax error
 */
static const char PRECEDENCE_TABLE[PREC_TERMINAL_COUNT][PREC_TERMINAL_COUNT + 1] = {
    /*               id LIST UNIT P DOT CALL M A R E AND OR RNG LPAR RPAR END */
    /* ID        */ "XXXX>>>>>>>>>X>>",
    /* LIST_CTOR */ "XXXX>>XXXXXXXXXX",
    /* UNIT      */ "XXXX>>>>>>>>>X>>",
    /* P         */ "<<<<<<>>>>>>><>>",
    /* DOT       */ "XXXX>>>>>>>>>X>>",
    /* CALL      */ "XXXX>>>>>>>>>X>>",
    /* M         */ "<<<<<<>>>>>>><>>",
    /* A         */ "<<<<<<<>>>>>><>>",
    /* R         */ "<<<<<<<<X>>>><>>",
    /* E         */ "<<<<<<<<<X>>><>>",
    /* AND       */ "<<<<<<<<<<>>><>>",
    /* OR        */ "<<<<<<<<<<<>><>>",
    /* RNG       */ "<<<<<<<<<<<<><>>",
    /* LPAR      */ "<<<<<<<<<<<<<<=X",
    /* RPAR      */ "XXXX>>>>>>>>>X>>",
    /* END       */ "<<<<<<<<<<<<<<X=",
};

/**
 * @brief one classified input symbol
 */
typedef struct PrecInput {
	prec_terminal_t terminal;
	bool if_operand; // PREC_ID thats actually a nested if expression, CONTROL
} prec_input_t;

/**
 * @brief what kind of item sits on the precedence stack
 */
typedef enum PrecItemKind {
	PREC_ITEM_TERMINAL,   // shifted terminal
	PREC_ITEM_EXPRESSION, // nonterminal E
	PREC_ITEM_HANDLE      // handle start marker
} prec_item_kind_t;

/**
 * @brief one item of the precedence stack
 *
 * operator_token keeps type and location of a prefix or binary operator
 * lexeme is only set for ranges, see prec_shift
 */
typedef struct PrecItem {
	prec_item_kind_t kind;
	prec_terminal_t terminal;
	token_t operator_token;
} prec_item_t;

/**
 * @brief precedence stack that grows as needed, one per expression or subexpression
 */
typedef struct PrecStack {
	prec_item_t *items;
	size_t count;
	size_t capacity;
} prec_stack_t;

/**
 * @brief parses one expression with the precedence table
 *
 * token that ends the expression is not consumed
 *
 * @param parser parser to use
 * @return true if the expression was valid
 */
static bool prec_parse(syntax_parser_t *parser);

/**
 * @brief marks a syntax error
 *
 * @param parser parser to mark as failed
 * @return always false so it can be returned right away
 */
static bool prec_fail(syntax_parser_t *parser) {
	parser->failed = true;
	return false;
}

/**
 * @brief marks an internal error, ie malloc failed
 *
 * @param parser parser to mark as failed
 * @return always false so it can be returned right away
 */
static bool prec_out_of_memory(syntax_parser_t *parser) {
	parser->failed = true;
	parser->internal_error = true;
	return false;
}

/**
 * @brief inserts an item into the stack, grows it when full
 *
 * @param parser parser that gets the allocation error
 * @param stack stack to insert into
 * @param index where the item goes, index equal to count means append
 * @param item item to insert
 * @return true on success, false when out of memory
 */
static bool prec_stack_insert(syntax_parser_t *parser, prec_stack_t *stack, size_t index, prec_item_t item) {
	if (stack->count == stack->capacity) {
		size_t capacity = stack->capacity == 0 ? 16 : stack->capacity * 2;
		prec_item_t *items = realloc(stack->items, capacity * sizeof(*items));
		if (items == NULL) {
			return prec_out_of_memory(parser);
		}
		stack->items = items;
		stack->capacity = capacity;
	}

	// shift everything above index up by one so the handle marker fits after the topmost terminal
	for (size_t position = stack->count; position > index; --position) {
		stack->items[position] = stack->items[position - 1];
	}
	stack->items[index] = item;
	stack->count++;
	return true;
}

/**
 * @brief finds the topmost terminal, skips E and handle markers
 *
 * @param stack stack to search
 * @return index of the topmost terminal, the bottom end symbol always counts
 */
static size_t prec_top_terminal(const prec_stack_t *stack) {
	size_t index = stack->count;
	while (index > 0) {
		--index;
		if (stack->items[index].kind == PREC_ITEM_TERMINAL) {
			return index;
		}
	}
	return 0;
}

/**
 * @brief checks if a symbol ends an operand
 *
 * after an operand an open paren starts a call, a dot is a member, minus and plus are binary and to is a
 * range
 *
 * @param terminal symbol to check
 * @return true if the symbol ends an operand
 */
static bool prec_is_operand_end(prec_terminal_t terminal) {
	return terminal == PREC_ID || terminal == PREC_LIST || terminal == PREC_UNIT || terminal == PREC_RPAREN ||
	       terminal == PREC_DOT || terminal == PREC_CALL;
}

/**
 * @brief checks if a token is an identifier with exactly this spelling
 *
 * @param token token to check
 * @param text spelling we want
 * @return true if it matches
 */
static bool token_is_identifier(const token_t *token, const char *text) {
	size_t length = strlen(text);
	return token != NULL && token->type == TOKEN_IDENTIFIER && token->lexeme != NULL &&
	       token->length == length && memcmp(token->lexeme, text, length) == 0;
}

/**
 * @brief maps the current token to a terminal of the precedence table
 *
 * same rules as the Input classification sheet in the table workbook
 *
 * @param parser parser whose current token we classify
 * @param last terminal shifted before this one, PREC_END if nothing was shifted yet
 * @param depth how many grouping parens this expression opened
 * @return the classified symbol
 */
static prec_input_t prec_classify(const syntax_parser_t *parser, prec_terminal_t last, size_t depth) {
	const token_t *token = syntax_parser_current(parser);
	const bool after_operand = prec_is_operand_end(last);
	prec_input_t input = {.terminal = PREC_END, .if_operand = false};
	if (token == NULL) {
		return input;
	}

	switch (token->type) {
	case TOKEN_IDENTIFIER:
		// after an operand only to until and by continue the expression, they are range operators
		// any other identifier ends it and the caller decides if thats an error
		if (!after_operand) {
			input.terminal = PREC_ID;
		} else if (token_is_identifier(token, "to") || token_is_identifier(token, "until") ||
		           token_is_identifier(token, "by")) {
			input.terminal = PREC_RANGE;
		}
		break;
	case TOKEN_INTEGER:
	case TOKEN_DOUBLE:
	case TOKEN_STRING:
	case TOKEN_MULTILINE_STRING:
	case TOKEN_TRUE:
	case TOKEN_FALSE:
		// literal right after an operand like x 5 ends the expression, caller rejects it
		if (!after_operand) {
			input.terminal = PREC_ID;
		}
		break;
	case TOKEN_TYPE_LIST:
		if (!after_operand) {
			input.terminal = PREC_LIST;
		}
		break;
	case TOKEN_LPAREN:
		// after an operand its a call or list index, otherwise unit literal or grouping
		if (after_operand) {
			input.terminal = PREC_CALL;
		} else {
			const token_t *next = syntax_parser_next(parser);
			input.terminal = next != NULL && next->type == TOKEN_RPAREN ? PREC_UNIT : PREC_LPAREN;
		}
		break;
	case TOKEN_RPAREN:
		// close paren we didnt open belongs to the caller, eg it closes an argument list
		if (depth > 0) {
			input.terminal = PREC_RPAREN;
		}
		break;
	case TOKEN_DOT:
		// dot with no operand before it ends the expression so caller reports it
		if (after_operand) {
			input.terminal = PREC_DOT;
		}
		break;
	case TOKEN_PLUS:
	case TOKEN_MINUS:
		// binary after an operand like a minus b, otherwise unary like minus a
		input.terminal = after_operand ? PREC_ADD : PREC_PREFIX;
		break;
	case TOKEN_NOT:
		input.terminal = PREC_PREFIX;
		break;
	case TOKEN_MULTIPLY:
	case TOKEN_DIVIDE:
		input.terminal = PREC_MUL;
		break;
	case TOKEN_CONCAT:
		input.terminal = PREC_ADD;
		break;
	case TOKEN_LESS:
	case TOKEN_LESS_EQUAL:
	case TOKEN_GREATER:
	case TOKEN_GREATER_EQUAL:
		input.terminal = PREC_REL;
		break;
	case TOKEN_EQUAL:
	case TOKEN_NOT_EQUAL:
		input.terminal = PREC_EQ;
		break;
	case TOKEN_AND:
		input.terminal = PREC_AND;
		break;
	case TOKEN_OR:
		input.terminal = PREC_OR;
		break;
	case TOKEN_IF:
		// nested if expression has to be a whole subexpression, so at the start or right after open paren
		// after an operand its a for loop guard and ends the expression
		if (last == PREC_END || last == PREC_LPAREN) {
			input.terminal = PREC_ID;
			input.if_operand = true;
		}
		break;
	default:
		// newline semicolon comma dedent eof then do else assign colon left arrow etc all end the expression
		break;
	}
	return input;
}

/**
 * @brief parses the argument list of a call or list index
 *
 * every argument is a full expression, trailing comma only ok when there is a line break
 * between it and the close paren, see section 1.2 of the spec
 *
 * @param parser parser sitting on the open paren
 * @param count gets the number of arguments
 * @return true on success
 */
static bool prec_parse_call_arguments(syntax_parser_t *parser, size_t *count) {
	*count = 0;
	if (!syntax_parser_expect(parser, TOKEN_LPAREN)) {
		return false;
	}
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		return prec_fail(parser);
	}
	if (token->type == TOKEN_RPAREN) {
		return syntax_parser_advance(parser);
	}
	for (;;) {
		if (!prec_parse(parser)) {
			return false;
		}
		(*count)++;
		token = syntax_parser_current(parser);
		if (token == NULL) {
			return prec_fail(parser);
		}
		if (token->type != TOKEN_COMMA) {
			break;
		}
		size_t comma_line = token->location.line;
		if (!syntax_parser_advance(parser)) {
			return false;
		}
		token = syntax_parser_current(parser);
		if (token == NULL) {
			return prec_fail(parser);
		}
		if (token->type == TOKEN_RPAREN) {
			// trailing comma, lexer drops newlines inside parens so just compare line numbers
			if (token->location.line <= comma_line) {
				return prec_fail(parser);
			}
			break;
		}
	}
	return syntax_parser_expect(parser, TOKEN_RPAREN);
}

/**
 * @brief parses a nested if expression with then and else, CONTROL extension
 *
 * else is required here since the expression needs a value
 *
 * @param parser parser sitting on if
 * @return true on success
 */
static bool prec_parse_if_operand(syntax_parser_t *parser) {
	if (!syntax_parser_expect(parser, TOKEN_IF) || !prec_parse(parser) ||
	    !syntax_parser_expect(parser, TOKEN_THEN) || !prec_parse(parser) ||
	    !syntax_parser_expect(parser, TOKEN_ELSE) || !prec_parse(parser)) {
		return false;
	}
	// semantic analyser: if expression done, condition then value and else value
	return true;
}

/**
 * @brief consumes the input symbol and pushes it as a terminal
 *
 * operands members and calls would go to the semantic analyser here since their tokens
 * die on the next advance, operators would go later when they get reduced
 *
 * @param parser parser to use
 * @param stack stack to push onto
 * @param input classified input symbol
 * @param depth open grouping parens, updated for open and close paren
 * @return true on success
 */
static bool prec_shift(syntax_parser_t *parser, prec_stack_t *stack, prec_input_t input, size_t *depth) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		return prec_fail(parser);
	}
	prec_item_t item = {.kind = PREC_ITEM_TERMINAL, .terminal = input.terminal};

	switch (input.terminal) {
	case PREC_ID:
		if (input.if_operand) {
			// whole if expression is just one operand
			if (!prec_parse_if_operand(parser)) {
				return false;
			}
			break;
		}
		/* fallthrough */
	case PREC_LIST: {
		// semantic analyser: report the operand here, identifier literal or List,
		// lexer frees the token text on next advance
		if (!syntax_parser_advance(parser)) {
			return false;
		}
		break;
	}
	case PREC_UNIT: {
		// semantic analyser: report unit literal here
		// unit literal is one operand but two tokens
		if (!syntax_parser_expect(parser, TOKEN_LPAREN) || !syntax_parser_expect(parser, TOKEN_RPAREN)) {
			return false;
		}
		break;
	}
	case PREC_DOT: {
		if (!syntax_parser_expect(parser, TOKEN_DOT)) {
			return false;
		}
		const token_t *member = syntax_parser_current(parser);
		if (member == NULL || member->type != TOKEN_IDENTIFIER) {
			return prec_fail(parser);
		}
		// semantic analyser: report member name here while the token is still valid
		if (!syntax_parser_advance(parser)) {
			return false;
		}
		break;
	}
	case PREC_CALL: {
		size_t argument_count = 0;
		if (!prec_parse_call_arguments(parser, &argument_count)) {
			return false;
		}
		// semantic analyser: report the call with argument_count args here
		// callee would go before the call, each argument while its parsed
		break;
	}
	case PREC_LPAREN:
		// depth is so prec_classify can tell our close paren from the one closing a call
		++*depth;
		if (!syntax_parser_advance(parser)) {
			return false;
		}
		break;
	case PREC_RPAREN:
		--*depth;
		if (!syntax_parser_advance(parser)) {
			return false;
		}
		break;
	case PREC_RANGE: {
		// point to a string literal, the tokens own lexeme gets freed on next advance
		const char *spelling = token_is_identifier(token, "to")      ? "to"
		                       : token_is_identifier(token, "until") ? "until"
		                                                             : "by";
		item.operator_token = (token_t){.type = TOKEN_IDENTIFIER,
		                                .location = token->location,
		                                .lexeme = (char *)spelling,
		                                .length = strlen(spelling)};
		if (!syntax_parser_advance(parser)) {
			return false;
		}
		break;
	}
	default:
		// operators only keep type and location for the semantic analyser, see prec_reduce
		item.operator_token = (token_t){.type = token->type, .location = token->location};
		if (!syntax_parser_advance(parser)) {
			return false;
		}
		break;
	}
	return prec_stack_insert(parser, stack, stack->count, item);
}

/**
 * @brief checks if a terminal is a binary operator group
 *
 * @param terminal terminal to check
 * @return true for binary operators
 */
static bool prec_is_binary(prec_terminal_t terminal) {
	return terminal == PREC_MUL || terminal == PREC_ADD || terminal == PREC_REL || terminal == PREC_EQ ||
	       terminal == PREC_AND || terminal == PREC_OR || terminal == PREC_RANGE;
}

/**
 * @brief checks if a stack item is this terminal
 *
 * @param item item to check
 * @param terminal terminal we want
 * @return true if it is
 */
static bool prec_item_is(const prec_item_t *item, prec_terminal_t terminal) {
	return item->kind == PREC_ITEM_TERMINAL && item->terminal == terminal;
}

/**
 * @brief reduces the handle above the topmost marker to E
 *
 * rules are from the Reduction rules sheet, E can be made from an id, List, unit literal,
 * prefix operator and E, E with dot or call, E operator E, or E in parens
 *
 * @param parser parser that gets the syntax error
 * @param stack stack to reduce
 * @return true if the handle matched a rule
 */
static bool prec_reduce(syntax_parser_t *parser, prec_stack_t *stack) {
	// handle is everything above the topmost marker
	size_t start = stack->count;
	while (start > 0 && stack->items[start - 1].kind != PREC_ITEM_HANDLE) {
		--start;
	}
	if (start == 0) {
		return prec_fail(parser);
	}
	const size_t length = stack->count - start;
	const prec_item_t *handle = &stack->items[start];
	bool valid = false;

	// operands are done when shifted, only unary and binary operators would be reported here
	if (length == 1) {
		valid = prec_item_is(&handle[0], PREC_ID) || prec_item_is(&handle[0], PREC_LIST) ||
		        prec_item_is(&handle[0], PREC_UNIT);
	} else if (length == 2) {
		if (handle[0].kind == PREC_ITEM_EXPRESSION &&
		    (prec_item_is(&handle[1], PREC_DOT) || prec_item_is(&handle[1], PREC_CALL))) {
			valid = true;
		} else if (prec_item_is(&handle[0], PREC_PREFIX) && handle[1].kind == PREC_ITEM_EXPRESSION) {
			// semantic analyser: report the unary operator here, its the operator_token of the first item
			valid = true;
		}
	} else if (length == 3) {
		if (prec_item_is(&handle[0], PREC_LPAREN) && handle[1].kind == PREC_ITEM_EXPRESSION &&
		    prec_item_is(&handle[2], PREC_RPAREN)) {
			valid = true;
		} else if (handle[0].kind == PREC_ITEM_EXPRESSION && handle[1].kind == PREC_ITEM_TERMINAL &&
		           prec_is_binary(handle[1].terminal) && handle[2].kind == PREC_ITEM_EXPRESSION) {
			// semantic analyser: report the binary operator here, its the operator_token of the middle item
			valid = true;
		}
	}
	if (!valid) {
		return prec_fail(parser);
	}

	// swap the marker and handle for E, stack only gets smaller so no alloc
	stack->count = start - 1;
	stack->items[stack->count++] = (prec_item_t){.kind = PREC_ITEM_EXPRESSION, .terminal = PREC_END};
	return true;
}

static bool prec_parse(syntax_parser_t *parser) {
	if (parser == NULL || parser->failed) {
		return false;
	}
	prec_stack_t stack = {0};
	bool result = false;
	size_t depth = 0;
	prec_terminal_t last = PREC_END;

	if (!prec_stack_insert(parser, &stack, 0,
	                       (prec_item_t){.kind = PREC_ITEM_TERMINAL, .terminal = PREC_END})) {
		goto cleanup;
	}

	// compare topmost terminal with incoming symbol in the table, then shift reduce or fail
	for (;;) {
		if (syntax_parser_current(parser) == NULL) {
			prec_fail(parser);
			goto cleanup;
		}
		const prec_input_t input = prec_classify(parser, last, depth);
		const size_t top = prec_top_terminal(&stack);
		const prec_terminal_t top_terminal = stack.items[top].terminal;

		if (top_terminal == PREC_END && input.terminal == PREC_END) {
			// accept only end symbol and E, empty expression is a syntax error
			result = stack.count == 2 && stack.items[1].kind == PREC_ITEM_EXPRESSION;
			if (!result) {
				prec_fail(parser);
			}
			goto cleanup;
		}

		const char relation = PRECEDENCE_TABLE[top_terminal][input.terminal];
		if (relation == '>') {
			if (!prec_reduce(parser, &stack)) {
				goto cleanup;
			}
			continue;
		}
		if (relation == 'X') {
			prec_fail(parser);
			goto cleanup;
		}

		// less than puts a marker after the topmost terminal, equals for matching parens shifts without one
		if (relation == '<' &&
		    !prec_stack_insert(parser, &stack, top + 1, (prec_item_t){.kind = PREC_ITEM_HANDLE})) {
			goto cleanup;
		}
		if (!prec_shift(parser, &stack, input, &depth)) {
			goto cleanup;
		}

		// remember it so we can classify the next token, eg unary or binary minus
		last = input.terminal;
	}

cleanup:
	free(stack.items);
	return result;
}

/** Public expression entry point; delimiters are left for the caller to consume. */
bool syntax_parse_expression(syntax_parser_t *parser) {
	if (parser == NULL || parser->failed) {
		return false;
	}
	return prec_parse(parser);
}

//
//	RECURSIVE DESCENT PARSER FOR DEFINITIONS AND STATEMENTS
//

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
	case TOKEN_TYPE_LIST:
	case TOKEN_LPAREN:
	case TOKEN_PLUS:
	case TOKEN_MINUS:
	case TOKEN_NOT:
		return true;
	default:
		return false;
	}
}

/**
 * @brief parses a type
 *
 * List type for LISTS is List then the element type in square brackets
 *
 * @param parser parser sitting on the type
 * @return true on success
 */
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
		// semantic analyser: report the type here
		return syntax_parser_advance(parser);
	case TOKEN_TYPE_LIST:
		// semantic analyser: report List here, element type comes next
		return syntax_parser_advance(parser) && syntax_parser_expect(parser, TOKEN_LBRACKET) &&
		       parse_type(parser) && syntax_parser_expect(parser, TOKEN_RBRACKET);
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
	// semantic analyser: report parameter name here
	if (!syntax_parser_advance(parser) || !syntax_parser_expect(parser, TOKEN_COLON) || !parse_type(parser)) {
		return false;
	}
	return parse_params_tail(parser);
}

static bool parse_params_tail(syntax_parser_t *parser) {
	const token_t *comma_token = syntax_parser_current(parser);
	size_t comma_line = comma_token != NULL ? comma_token->location.line : 0;
	bool comma = false;
	if (!syntax_parser_accept(parser, TOKEN_COMMA, &comma)) {
		return false;
	}
	if (!comma) {
		return true;
	}
	const token_t *token = syntax_parser_current(parser);
	if (token != NULL && token->type == TOKEN_RPAREN) {
		// trailing comma only ok before a line break, section 1.2 of the spec
		// lexer drops newlines inside parens so compare line numbers
		if (token->location.line <= comma_line) {
			parser->failed = true;
			return false;
		}
		return true;
	}
	if (token == NULL || token->type != TOKEN_IDENTIFIER) {
		parser->failed = true;
		return false;
	}
	// semantic analyser: report parameter name here
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
		// semicolon at end of line, the newline after it belongs to the same terminator
		if (!syntax_parser_advance(parser)) {
			return false;
		}
		const token_t *after = syntax_parser_current(parser);
		if (after == NULL) {
			parser->failed = true;
			return false;
		}
		if (after->type == TOKEN_NEWLINE) {
			return syntax_parser_advance(parser);
		}
		return true;
	}
	if (token->type == TOKEN_DEDENT || token->type == TOKEN_EOF) {
		// only at end of file, leave the token for the block or program around it
		return true;
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
	// semantic analyser: new block and scope starts here
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
			// semantic analyser: block and scope ends here
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

/**
 * @brief parses a one line body of a branch or loop, CONTROL extension
 *
 * body is an assignment, return or expression, caller handles the terminator
 *
 * @param parser parser sitting on the body
 * @return true on success
 */
static bool parse_inline_body(syntax_parser_t *parser) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		parser->failed = true;
		return false;
	}
	if (token->type == TOKEN_RETURN) {
		if (!syntax_parser_advance(parser)) {
			return false;
		}
		const token_t *next = syntax_parser_current(parser);
		if (next == NULL) {
			parser->failed = true;
			return false;
		}
		// return without a value ends at the terminator or at else
		if (next->type != TOKEN_NEWLINE && next->type != TOKEN_SEMICOLON && next->type != TOKEN_ELSE &&
		    next->type != TOKEN_DEDENT && next->type != TOKEN_EOF && !syntax_parse_expression(parser)) {
			return false;
		}
		// semantic analyser: return done
		return true;
	}

	// second token tells assignment apart from an expression that starts with the same name
	const token_t *next = syntax_parser_next(parser);
	if (token->type == TOKEN_IDENTIFIER && next != NULL && next->type == TOKEN_ASSIGN) {
		// semantic analyser: report assignment target here
		if (!syntax_parser_advance(parser) || !syntax_parser_expect(parser, TOKEN_ASSIGN) ||
		    !syntax_parse_expression(parser)) {
			return false;
		}
		// semantic analyser: assignment done
		return true;
	}
	return syntax_parse_expression(parser);
}

/**
 * @brief parses the branch after else, block or nested if or one line body
 *
 * @param parser parser sitting after else
 * @return true on success
 */
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
	return parse_inline_body(parser) && parse_stmt_end(parser);
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

	// one line then with else on the next line, skip the newline and go on with else
	const token_t *next = syntax_parser_next(parser);
	if (token->type == TOKEN_NEWLINE && next != NULL && next->type == TOKEN_ELSE) {
		return syntax_parser_advance(parser) && syntax_parser_advance(parser) && parse_else_body(parser);
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
	return parse_inline_body(parser) && parse_else_line(parser);
}

/** Parse an IF condition and its THEN/optional ELSE content. */
static bool parse_if(syntax_parser_t *parser) {
	if (!syntax_parser_expect(parser, TOKEN_IF) || !syntax_parse_expression(parser) ||
	    !syntax_parser_expect(parser, TOKEN_THEN) || !parse_then_tail(parser)) {
		return false;
	}
	// semantic analyser: if statement done
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

/**
 * @brief parses for loop generator, loop variable then left arrow then source
 *
 * source is one expression, ranges with to until and by are range operators in the table
 * so a List value works as source too for LISTS
 *
 * @param parser parser sitting on the loop variable
 * @return true on success
 */
static bool parse_generator(syntax_parser_t *parser) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL || token->type != TOKEN_IDENTIFIER) {
		parser->failed = true;
		return false;
	}
	return syntax_parser_advance(parser) && syntax_parser_expect(parser, TOKEN_LEFT_ARROW) &&
	       syntax_parse_expression(parser);
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

	// multi line header, generator and guards each on own indented line,
	// do goes back to the same indent as for
	if (token->type == TOKEN_NEWLINE) {
		return syntax_parser_advance(parser) && syntax_parser_expect(parser, TOKEN_INDENT) &&
		       parse_generator(parser) && syntax_parser_expect(parser, TOKEN_NEWLINE) &&
		       parse_guard_lines(parser) && syntax_parser_expect(parser, TOKEN_DEDENT);
	}
	return parse_generator(parser) && parse_guards(parser);
}

/**
 * @brief parses loop body after do, block or one line body
 *
 * @param parser parser sitting after do
 * @return true on success
 */
static bool parse_do_body(syntax_parser_t *parser) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		parser->failed = true;
		return false;
	}
	if (token->type == TOKEN_NEWLINE) {
		return syntax_parser_advance(parser) && parse_block(parser);
	}
	return parse_inline_body(parser) && parse_stmt_end(parser);
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
		// semantic analyser: val or var definition starts here
		if (!syntax_parser_advance(parser)) {
			parser->failed = true;
			return false;
		}
		const token_t *name = syntax_parser_current(parser);
		if (name == NULL || name->type != TOKEN_IDENTIFIER) {
			parser->failed = true;
			return false;
		}
		// semantic analyser: report variable name here
		if (!syntax_parser_advance(parser) || !parse_type_opt(parser) ||
		    !syntax_parser_expect(parser, TOKEN_ASSIGN) || !parse_value(parser)) {
			return false;
		}
		// semantic analyser: variable definition done
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
		// no value if the line or file ends right after return
		if (next->type == TOKEN_NEWLINE || next->type == TOKEN_SEMICOLON || next->type == TOKEN_DEDENT ||
		    next->type == TOKEN_EOF) {
			if (!parse_stmt_end(parser)) {
				return false;
			}
		} else if (!parse_value(parser)) {
			return false;
		}
		// semantic analyser: return done
		return true;
	}
	case TOKEN_WHILE:
		if (!syntax_parser_advance(parser) || !syntax_parse_expression(parser) ||
		    !syntax_parser_expect(parser, TOKEN_DO) || !parse_do_body(parser)) {
			return false;
		}
		// semantic analyser: while loop done
		return true;
	case TOKEN_FOR:
		if (!syntax_parser_advance(parser) || !parse_for_head(parser) ||
		    !syntax_parser_expect(parser, TOKEN_DO) || !parse_do_body(parser)) {
			return false;
		}
		// semantic analyser: for loop done
		return true;
	case TOKEN_IF:
		return parse_if(parser);
	case TOKEN_IDENTIFIER:
		// name then assign is an assignment, anything else starting with a name is an expression statement
		if (syntax_parser_next(parser) != NULL && syntax_parser_next(parser)->type == TOKEN_ASSIGN) {
			// semantic analyser: report assignment target here
			if (!syntax_parser_advance(parser) || !syntax_parser_expect(parser, TOKEN_ASSIGN) ||
			    !parse_value(parser)) {
				return false;
			}
			// semantic analyser: assignment done
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

	// main annotation can be on its own line above def
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
	// semantic analyser: function definition starts here
	if (!syntax_parser_advance(parser)) {
		return false;
	}
	const token_t *function_name = syntax_parser_current(parser);
	if (function_name == NULL || function_name->type != TOKEN_IDENTIFIER) {
		parser->failed = true;
		return false;
	}
	// semantic analyser: report function name here
	if (!syntax_parser_advance(parser) || !syntax_parser_expect(parser, TOKEN_LPAREN) ||
	    !parse_param_list(parser) || !syntax_parser_expect(parser, TOKEN_RPAREN)) {
		return false;
	}
	bool has_return_type = false;
	if (!syntax_parser_accept(parser, TOKEN_COLON, &has_return_type) ||
	    (has_return_type && !parse_type(parser)) || !syntax_parser_expect(parser, TOKEN_ASSIGN) ||
	    !parse_body(parser)) {
		return false;
	}
	// semantic analyser: function definition done
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
	if (!syntax_parser_expect(parser, TOKEN_DOT) || !syntax_parser_expect(parser, TOKEN_MULTIPLY)) {
		return false;
	}
	const token_t *end = syntax_parser_current(parser);
	if (end != NULL && end->type == TOKEN_EOF) {
		return true;
	}
	return syntax_parser_expect(parser, TOKEN_NEWLINE);
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
			// semantic analyser: program done
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
