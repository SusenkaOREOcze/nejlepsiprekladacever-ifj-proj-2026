#include "include/syntax_anal.h"

#include <stdio.h>
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

/**
 * @brief sends one event to the semantic analyser
 *
 * an error from the semantic analyser stops parsing like a syntax error would,
 * its exit code is kept in semantic_result
 *
 * @param parser parser that stops on error
 * @param action what was recognized
 * @param token token of it or NULL
 * @param item_count see syntax_semantic_action_t
 * @return true when the semantic analyser accepted the event
 */
static bool syntax_report(syntax_parser_t *parser, syntax_semantic_action_t action, const token_t *token,
                          size_t item_count) {
	if (parser == NULL || parser->failed) {
		return false;
	}
	const sem_result_t code = syntax_semantic_action(action, token, item_count);
	if (code != SEM_OK) {
		parser->failed = true;
		parser->semantic_result = code;
		return false;
	}
	return true;
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
	PREC_RANGE,  // to until and by after an operand inside parens, LISTS
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
 * @brief parses the rest of an expression whose first identifier was already consumed
 *
 * this is expr_tail from the grammar, the statement parser eats the identifier first
 * to see if an assign follows, if not the expression goes on from here
 *
 * @param parser parser sitting right after the identifier
 * @return true if the expression was valid
 */
static bool prec_parse_tail(syntax_parser_t *parser);

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
	fprintf(stderr, "internal error: out of memory.\n");
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
	/* LISTS disabled for now, PREC_LIST also ended an operand:
	   terminal == PREC_LIST || */
	return terminal == PREC_ID || terminal == PREC_UNIT || terminal == PREC_RPAREN || terminal == PREC_DOT ||
	       terminal == PREC_CALL;
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
		/* identifier after an operand ends the expression and the caller decides if thats an error,
		   a for loop header parses its to until and by itself */
		if (!after_operand) {
			input.terminal = PREC_ID;
		}
		/* LISTS
		else if (depth > 0 && (token_is_identifier(token, "to") || token_is_identifier(token, "until") ||
		                         token_is_identifier(token, "by"))) {
		    input.terminal = PREC_RANGE;
		}
		*/
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
	/* LISTS
	case TOKEN_TYPE_LIST:
		if (!after_operand) {
		    input.terminal = PREC_LIST;
		}
		break;
	*/
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
	/* LISTS
	case TOKEN_CONCAT:
		input.terminal = PREC_ADD;
		break;
	*/
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
	// condition, then value and else value are done
	return syntax_report(parser, SEMANTIC_ACTION_IF, NULL, 3);
}

/**
 * @brief consumes the input symbol and pushes it as a terminal
 *
 * operands members and calls go to the semantic analyser here since their tokens
 * die on the next advance, operators go later when they get reduced
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
		/* LISTS
		   case PREC_LIST: */
		{
			// identifier or literal, reported now because the lexer frees the token text on next advance
			if (!syntax_report(parser, SEMANTIC_ACTION_PRIMARY, token, 0) || !syntax_parser_advance(parser)) {
				return false;
			}
			break;
		}
	case PREC_UNIT: {
		// unit literal is one operand but two tokens
		if (!syntax_report(parser, SEMANTIC_ACTION_UNIT, token, 0) ||
		    !syntax_parser_expect(parser, TOKEN_LPAREN) || !syntax_parser_expect(parser, TOKEN_RPAREN)) {
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
		// member name is reported while the token is still valid
		if (!syntax_report(parser, SEMANTIC_ACTION_MEMBER, member, 0) || !syntax_parser_advance(parser)) {
			return false;
		}
		break;
	}
	case PREC_CALL: {
		size_t argument_count = 0;
		if (!prec_parse_call_arguments(parser, &argument_count)) {
			return false;
		}
		// callee was reported before the call and each argument while it was parsed
		if (!syntax_report(parser, SEMANTIC_ACTION_CALL, NULL, argument_count)) {
			return false;
		}
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

	// operands are reported when shifted, only unary and binary operators are reported here
	if (length == 1) {
		/* LISTS
		   prec_item_is(&handle[0], PREC_LIST) || */
		valid = prec_item_is(&handle[0], PREC_ID) || prec_item_is(&handle[0], PREC_UNIT);
	} else if (length == 2) {
		if (handle[0].kind == PREC_ITEM_EXPRESSION &&
		    (prec_item_is(&handle[1], PREC_DOT) || prec_item_is(&handle[1], PREC_CALL))) {
			valid = true;
		} else if (prec_item_is(&handle[0], PREC_PREFIX) && handle[1].kind == PREC_ITEM_EXPRESSION) {
			// unary operator is the operator_token of the first item
			if (!syntax_report(parser, SEMANTIC_ACTION_UNARY, &handle[0].operator_token, 0)) {
				return false;
			}
			valid = true;
		}
	} else if (length == 3) {
		if (prec_item_is(&handle[0], PREC_LPAREN) && handle[1].kind == PREC_ITEM_EXPRESSION &&
		    prec_item_is(&handle[2], PREC_RPAREN)) {
			valid = true;
		} else if (handle[0].kind == PREC_ITEM_EXPRESSION && handle[1].kind == PREC_ITEM_TERMINAL &&
		           prec_is_binary(handle[1].terminal) && handle[2].kind == PREC_ITEM_EXPRESSION) {
			// binary operator is the operator_token of the middle item
			if (!syntax_report(parser, SEMANTIC_ACTION_BINARY, &handle[1].operator_token, 0)) {
				return false;
			}
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

/**
 * @brief the precedence parser itself, prec_parse and prec_parse_tail both end up here
 *
 * @param parser parser to use
 * @param first_operand_consumed true when the caller already ate the first identifier
 * @return true if the expression was valid
 */
static bool prec_parse_from(syntax_parser_t *parser, bool first_operand_consumed) {
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

	// the eaten identifier is shifted like any operand would be, end symbol vs id is a less than
	if (first_operand_consumed) {
		if (!prec_stack_insert(parser, &stack, 1, (prec_item_t){.kind = PREC_ITEM_HANDLE}) ||
		    !prec_stack_insert(parser, &stack, 2,
		                       (prec_item_t){.kind = PREC_ITEM_TERMINAL, .terminal = PREC_ID})) {
			goto cleanup;
		}
		last = PREC_ID;
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

static bool prec_parse(syntax_parser_t *parser) { return prec_parse_from(parser, false); }

static bool prec_parse_tail(syntax_parser_t *parser) { return prec_parse_from(parser, true); }

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
	/* LISTS
	   case TOKEN_TYPE_LIST: */
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
 * List type for LISTS is commented out for now
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
		return syntax_report(parser, SEMANTIC_ACTION_TYPE, token, 0) && syntax_parser_advance(parser);
	/* LISTS
	case TOKEN_TYPE_LIST:
		// semantic analyser: report List here, element type comes next
		// syntax_semantic_action(SEMANTIC_ACTION_TYPE, token, 0);
		return syntax_parser_advance(parser) && syntax_parser_expect(parser, TOKEN_LBRACKET) &&
		       parse_type(parser) && syntax_parser_expect(parser, TOKEN_RBRACKET);
	*/
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
	if (!syntax_report(parser, SEMANTIC_ACTION_PARAMETER, token, 0) || !syntax_parser_advance(parser) ||
	    !syntax_parser_expect(parser, TOKEN_COLON) || !parse_type(parser)) {
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
	if (!syntax_report(parser, SEMANTIC_ACTION_PARAMETER, token, 0) || !syntax_parser_advance(parser) ||
	    !syntax_parser_expect(parser, TOKEN_COLON) || !parse_type(parser)) {
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
	// new block and scope starts here
	if (!syntax_report(parser, SEMANTIC_ACTION_BLOCK_BEGIN, NULL, 0) ||
	    !syntax_parser_expect(parser, TOKEN_INDENT)) {
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
			// block and scope ends here
			if (!syntax_report(parser, SEMANTIC_ACTION_BLOCK, NULL, 0) || !syntax_parser_advance(parser)) {
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
		const bool has_value = next->type != TOKEN_NEWLINE && next->type != TOKEN_SEMICOLON &&
		                       next->type != TOKEN_ELSE && next->type != TOKEN_DEDENT &&
		                       next->type != TOKEN_EOF;
		if (has_value && !syntax_parse_expression(parser)) {
			return false;
		}
		return syntax_report(parser, SEMANTIC_ACTION_RETURN, NULL, has_value ? 1 : 0);
	}

	// ID <id_stmt> like in parse_statement, eat the name first and then decide
	if (token->type == TOKEN_IDENTIFIER) {
		/*
		 * the name is the assignment target or the first operand, the semantic analyser
		 * finds out from the events that follow
		 */
		if (!syntax_report(parser, SEMANTIC_ACTION_ASSIGNMENT_TARGET, token, 0) ||
		    !syntax_parser_advance(parser)) {
			return false;
		}
		bool assign = false;
		if (!syntax_parser_accept(parser, TOKEN_ASSIGN, &assign)) {
			return false;
		}
		if (assign) {
			return syntax_parse_expression(parser) &&
			       syntax_report(parser, SEMANTIC_ACTION_ASSIGNMENT, NULL, 0);
		}
		return prec_parse_tail(parser);
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

/** Parse an optional inline ELSE branch or the inline statement terminator, has_else says which. */
static bool parse_else_line(syntax_parser_t *parser, bool *has_else) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		parser->failed = true;
		return false;
	}
	if (token->type == TOKEN_ELSE) {
		*has_else = true;
		return syntax_parser_advance(parser) && parse_else_body(parser);
	}

	// one line then with else on the next line, skip the newline and go on with else
	const token_t *next = syntax_parser_next(parser);
	if (token->type == TOKEN_NEWLINE && next != NULL && next->type == TOKEN_ELSE) {
		*has_else = true;
		return syntax_parser_advance(parser) && syntax_parser_advance(parser) && parse_else_body(parser);
	}
	*has_else = false;
	return parse_stmt_end(parser);
}

/** Parse the optional ELSE branch following a block-style THEN branch, has_else says if there was one. */
static bool parse_else_block(syntax_parser_t *parser, bool *has_else) {
	if (!syntax_parser_accept(parser, TOKEN_ELSE, has_else)) {
		return false;
	}
	return !*has_else || parse_else_body(parser);
}

/** Parse either block-style or inline content following THEN, has_else says if an ELSE branch was parsed. */
static bool parse_then_tail(syntax_parser_t *parser, bool *has_else) {
	const token_t *token = syntax_parser_current(parser);
	if (token == NULL) {
		parser->failed = true;
		return false;
	}
	if (token->type == TOKEN_NEWLINE) {
		return syntax_parser_advance(parser) && parse_block(parser) && parse_else_block(parser, has_else);
	}
	return parse_inline_body(parser) && parse_else_line(parser, has_else);
}

/** Parse an IF condition and its THEN/optional ELSE content. */
static bool parse_if(syntax_parser_t *parser) {
	bool has_else = false;
	if (!syntax_parser_expect(parser, TOKEN_IF) || !syntax_parse_expression(parser) ||
	    !syntax_parser_expect(parser, TOKEN_THEN) || !parse_then_tail(parser, &has_else)) {
		return false;
	}
	// condition, then branch and maybe else branch are done
	return syntax_report(parser, SEMANTIC_ACTION_IF, NULL, has_else ? 3 : 2);
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
 * grammar, <generator> -> ID LEFT_ARROW expr <gen_tail>
 * <gen_tail> -> <range> expr <step>, eps when the source is a List is commented out until LISTS is back
 * <range> -> to | until, <step> -> by expr | eps
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
	if (!syntax_report(parser, SEMANTIC_ACTION_FOR_VARIABLE, token, 0) || !syntax_parser_advance(parser) ||
	    !syntax_parser_expect(parser, TOKEN_LEFT_ARROW) || !syntax_parse_expression(parser)) {
		return false;
	}

	// expression parser stops at to and until outside parens, so the range is parsed here
	token = syntax_parser_current(parser);
	if (!token_is_identifier(token, "to") && !token_is_identifier(token, "until")) {
		// without LISTS the source has to be a range, so to or until must follow
		parser->failed = true;
		return false;
		/* LISTS
		// semantic analyser: the source is a List
		return true;
		*/
	}
	// range start is done, the token says if its to or until
	if (!syntax_report(parser, SEMANTIC_ACTION_RANGE_BEGIN, token, 0) || !syntax_parser_advance(parser) ||
	    !syntax_parse_expression(parser)) {
		return false;
	}
	const bool has_step = token_is_identifier(syntax_parser_current(parser), "by");
	if (has_step && (!syntax_parser_advance(parser) || !syntax_parse_expression(parser))) {
		return false;
	}
	// range end and maybe the step are done
	return syntax_report(parser, SEMANTIC_ACTION_RANGE, NULL, has_step ? 2 : 1);
}

/** Parse zero or more inline IF guards, guard_count gets increased for each. */
static bool parse_guards(syntax_parser_t *parser, size_t *guard_count) {
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
		++*guard_count;
	}
}

/** Parse zero or more newline-terminated IF guards, guard_count gets increased for each. */
static bool parse_guard_lines(syntax_parser_t *parser, size_t *guard_count) {
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
		++*guard_count;
	}
}

/** Parse either the inline or multiline form of a for-loop header, guard_count gets the number of guards. */
static bool parse_for_head(syntax_parser_t *parser, size_t *guard_count) {
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
		       parse_guard_lines(parser, guard_count) && syntax_parser_expect(parser, TOKEN_DEDENT);
	}
	return parse_generator(parser) && parse_guards(parser, guard_count);
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
		// val or var definition starts here
		if (!syntax_report(parser, SEMANTIC_ACTION_DECLARATION_BEGIN, token, 0)) {
			return false;
		}
		if (!syntax_parser_advance(parser)) {
			parser->failed = true;
			return false;
		}
		const token_t *name = syntax_parser_current(parser);
		if (name == NULL || name->type != TOKEN_IDENTIFIER) {
			parser->failed = true;
			return false;
		}
		if (!syntax_report(parser, SEMANTIC_ACTION_DECLARATION_NAME, name, 0) ||
		    !syntax_parser_advance(parser) || !parse_type_opt(parser) ||
		    !syntax_parser_expect(parser, TOKEN_ASSIGN) || !parse_value(parser)) {
			return false;
		}
		return syntax_report(parser, SEMANTIC_ACTION_DECLARATION, NULL, 0);
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
		const bool has_value = next->type != TOKEN_NEWLINE && next->type != TOKEN_SEMICOLON &&
		                       next->type != TOKEN_DEDENT && next->type != TOKEN_EOF;
		if (has_value ? !parse_value(parser) : !parse_stmt_end(parser)) {
			return false;
		}
		return syntax_report(parser, SEMANTIC_ACTION_RETURN, NULL, has_value ? 1 : 0);
	}
	case TOKEN_WHILE:
		if (!syntax_parser_advance(parser) || !syntax_parse_expression(parser) ||
		    !syntax_parser_expect(parser, TOKEN_DO) || !parse_do_body(parser)) {
			return false;
		}
		return syntax_report(parser, SEMANTIC_ACTION_WHILE, NULL, 0);
	case TOKEN_FOR: {
		size_t guard_count = 0;
		if (!syntax_parser_advance(parser) || !parse_for_head(parser, &guard_count) ||
		    !syntax_parser_expect(parser, TOKEN_DO) || !parse_do_body(parser)) {
			return false;
		}
		return syntax_report(parser, SEMANTIC_ACTION_FOR, NULL, guard_count);
	}
	case TOKEN_IF:
		return parse_if(parser);
	case TOKEN_IDENTIFIER: {
		// grammar rules 32 to 35, ID <id_stmt>, the name is eaten first
		// then assign means assignment, anything else is the rest of an expression statement (expr_tail)
		/*
		 * the name is the assignment target or the first operand, the semantic analyser
		 * finds out from the events that follow
		 */
		if (!syntax_report(parser, SEMANTIC_ACTION_ASSIGNMENT_TARGET, token, 0) ||
		    !syntax_parser_advance(parser)) {
			return false;
		}
		bool assign = false;
		if (!syntax_parser_accept(parser, TOKEN_ASSIGN, &assign)) {
			return false;
		}
		if (assign) {
			return parse_value(parser) && syntax_report(parser, SEMANTIC_ACTION_ASSIGNMENT, NULL, 0);
		}
		return prec_parse_tail(parser) && parse_stmt_end(parser);
	}
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
	// function definition starts here, item count says if it has @main
	if (!syntax_report(parser, SEMANTIC_ACTION_FUNCTION_BEGIN, def_token, is_main ? 1 : 0) ||
	    !syntax_parser_advance(parser)) {
		return false;
	}
	const token_t *function_name = syntax_parser_current(parser);
	if (function_name == NULL || function_name->type != TOKEN_IDENTIFIER) {
		parser->failed = true;
		return false;
	}
	if (!syntax_report(parser, SEMANTIC_ACTION_FUNCTION_NAME, function_name, 0) ||
	    !syntax_parser_advance(parser) || !syntax_parser_expect(parser, TOKEN_LPAREN) ||
	    !parse_param_list(parser) || !syntax_parser_expect(parser, TOKEN_RPAREN)) {
		return false;
	}
	bool has_return_type = false;
	if (!syntax_parser_accept(parser, TOKEN_COLON, &has_return_type) ||
	    (has_return_type && !parse_type(parser)) || !syntax_parser_expect(parser, TOKEN_ASSIGN) ||
	    !parse_body(parser)) {
		return false;
	}
	return syntax_report(parser, SEMANTIC_ACTION_FUNCTION, NULL, 0);
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
static bool parse_program(syntax_parser_t *parser) {
	if (parser->failed || !syntax_report(parser, SEMANTIC_ACTION_PROGRAM_BEGIN, NULL, 0) ||
	    !parse_prolog(parser)) {
		return false;
	}
	for (;;) {
		const token_t *token = syntax_parser_current(parser);
		if (token == NULL) {
			parser->failed = true;
			return false;
		}
		if (token->type == TOKEN_EOF) {
			// whole program is parsed, the semantic analyser checks it now
			return syntax_parser_advance(parser) && syntax_report(parser, SEMANTIC_ACTION_PROGRAM, NULL, 0);
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

/**
 * @brief prints a syntax error for the token where parsing stopped
 *
 * an internal error and a semantic error were printed where they happened,
 * a lexer error is printed by main from lexer_error, so only a real syntax error is printed here
 *
 * @param parser parser that failed
 */
static void syntax_print_error(const syntax_parser_t *parser) {
	if (parser->internal_error || parser->semantic_result != SEM_OK || lexer_error(parser->lexer) != NULL) {
		return;
	}
	// syntax_parser_current gives NULL after a failure, the lexer still has the token
	const token_t *token = lexer_current(parser->lexer);
	if (token == NULL) {
		fprintf(stderr, "syntax error.\n");
		return;
	}
	switch (token->type) {
	case TOKEN_EOF:
		fprintf(stderr, "%zu: syntax error: unexpected end of file.\n", token->location.line);
		break;
	case TOKEN_NEWLINE:
		fprintf(stderr, "%zu: syntax error: unexpected end of line.\n", token->location.line);
		break;
	case TOKEN_INDENT:
		fprintf(stderr, "%zu: syntax error: unexpected indentation.\n", token->location.line);
		break;
	case TOKEN_DEDENT:
		fprintf(stderr, "%zu: syntax error: unexpected end of block.\n", token->location.line);
		break;
	default:
		if (token->lexeme != NULL) {
			fprintf(stderr, "%zu: syntax error: unexpected '%s'.\n", token->location.line, token->lexeme);
		} else {
			fprintf(stderr, "%zu: syntax error: unexpected token.\n", token->location.line);
		}
		break;
	}
}

bool syntax_parse_program(syntax_parser_t *parser) {
	if (parser == NULL) {
		return false;
	}
	if (!parse_program(parser)) {
		syntax_print_error(parser);
		return false;
	}
	return true;
}
