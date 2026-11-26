/**
 * @file lexer_keywords.c
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-10-10
 * @brief IFJ26 keyword classification.
 */

#include <string.h>

#include "include/lexer_internal.h"

typedef struct Keyword {
	const char *text;
	token_type_t type;
} keyword_t;

/**
 * @brief Classifies a complete identifier using the IFJ26 keyword table.
 *
 * The FSM first produces the identifier.
 * When an indentifier is complete, this function is called to determine whether it is a keyword or a regular
 * identifier.
 *
 * @param text NULL terminated identifier text.
 * @param length Length of text without its terminating NUL byte.
 * @return The matching keyword token type, or TOKEN_IDENTIFIER.
 */
token_type_t lexer_keyword_type(const char *text, size_t length) {
	static const keyword_t KEYWORDS[] = {
	    {"def", TOKEN_DEF},
	    {"do", TOKEN_DO},
	    {"else", TOKEN_ELSE},
	    {"if", TOKEN_IF},
	    {"import", TOKEN_IMPORT},
	    {"return", TOKEN_RETURN},
	    {"then", TOKEN_THEN},
	    {"val", TOKEN_VAL},
	    {"var", TOKEN_VAR},
	    {"while", TOKEN_WHILE},
	    {"Int", TOKEN_TYPE_INT},
	    {"Double", TOKEN_TYPE_DOUBLE},
	    {"String", TOKEN_TYPE_STRING},
	    {"Unit", TOKEN_TYPE_UNIT},
	    {"Boolean", TOKEN_TYPE_BOOLEAN},
	    {"List", TOKEN_TYPE_LIST},
	    {"case", TOKEN_CASE},
	    {"class", TOKEN_CLASS},
	    {"extension", TOKEN_EXTENSION},
	    {"for", TOKEN_FOR},
	    {"match", TOKEN_MATCH},
	    {"type", TOKEN_TYPE},
	    {"true", TOKEN_TRUE},
	    {"false", TOKEN_FALSE},
	};

	// Search the keyword table for a match
	for (size_t index = 0; index < sizeof(KEYWORDS) / sizeof(KEYWORDS[0]); ++index) {
		const keyword_t *keyword = &KEYWORDS[index];

		if (strlen(keyword->text) == length && memcmp(keyword->text, text, length) == 0) {
			return keyword->type;
		}
	}

	return TOKEN_IDENTIFIER;
}
