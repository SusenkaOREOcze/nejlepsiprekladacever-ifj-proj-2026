#ifndef IFJ_TOKEN_H
#define IFJ_TOKEN_H

/**
 * @file token.h
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-09-30
 * @brief Token definitions for the IFJ26 language.
 */

#include <stddef.h>

/**
 * @brief Terminal symbols in the IFJ26 language.
 */
typedef enum TokenType {
	// SPECIAL TOKENS
	TOKEN_EOF,
	TOKEN_NEWLINE,
	TOKEN_INDENT,
	TOKEN_DEDENT,

	// IDENTIFIERS AND LITERALS
	TOKEN_IDENTIFIER,       // Identifier, eg. `total`
	TOKEN_WILDCARD,         // Wildcard pattern `_`
	TOKEN_INTEGER,          // Integer, eg. `42`
	TOKEN_DOUBLE,           // Double value, eg. `3.14`
	TOKEN_STRING,           // String, eg. `"hello"`
	TOKEN_MULTILINE_STRING, // Multiline string, eg. `"""hello"""`
	TOKEN_TRUE,             // Boolean value `true`
	TOKEN_FALSE,            // Boolean value `false`

	// KEYWORDS :3
	TOKEN_DEF,             // `def`
	TOKEN_DO,              // `do`
	TOKEN_ELSE,            // `else`
	TOKEN_IF,              // `if`
	TOKEN_IMPORT,          // `import`
	TOKEN_RETURN,          // `return`
	TOKEN_THEN,            // `then`
	TOKEN_VAL,             // `val`
	TOKEN_VAR,             // `var`
	TOKEN_WHILE,           // `while`
	TOKEN_TYPE_INT,        // `Int`
	TOKEN_TYPE_DOUBLE,     // `Double`
	TOKEN_TYPE_STRING,     // `String`
	TOKEN_TYPE_UNIT,       // `Unit`
	TOKEN_MAIN_ANNOTATION, // `@main`

	// EXTEMSION KEYWORDS
	TOKEN_TYPE_BOOLEAN, // `Boolean`
	TOKEN_TYPE_LIST,    // `List`
	TOKEN_CASE,         // `case`
	TOKEN_CLASS,        // `class`
	TOKEN_EXTENSION,    // `extension`
	TOKEN_FOR,          // `for`
	TOKEN_MATCH,        // `match`
	TOKEN_TYPE,         // `type`

	// SPECIAL SYMBOLS
	TOKEN_LPAREN,    // `(`
	TOKEN_RPAREN,    // `)`
	TOKEN_LBRACKET,  // `[`
	TOKEN_RBRACKET,  // `]`
	TOKEN_COMMA,     // `,`
	TOKEN_SEMICOLON, // `;`
	TOKEN_DOT,       // `.`
	TOKEN_COLON,     // `:`

	// OPERATORS
	TOKEN_ASSIGN,        // `=`
	TOKEN_FAT_ARROW,     // `=>`
	TOKEN_LEFT_ARROW,    // `<-`
	TOKEN_PIPE,          // `|`
	TOKEN_PLUS,          // `+`
	TOKEN_CONCAT,        // `++`
	TOKEN_MINUS,         // `-`
	TOKEN_MULTIPLY,      // `*`
	TOKEN_DIVIDE,        // `/`
	TOKEN_LESS,          // `<`
	TOKEN_LESS_EQUAL,    // `<=`
	TOKEN_GREATER,       // `>`
	TOKEN_GREATER_EQUAL, // `>=`
	TOKEN_EQUAL,         // `==`
	TOKEN_NOT_EQUAL,     // `!=`
	TOKEN_NOT,           // `!`
	TOKEN_AND,           // `&&`
	TOKEN_OR             // `||`
} token_type_t;

/**
 * @brief Structure representing the location of a token in the source code.
 */
typedef struct TokenLocation {
	size_t line;
	size_t column;
} token_location_t;

/**
 * @brief Structure representing a token in the source code.
 *
 * lexeme is an exact, NULL-terminated string.
 * It may be NULL for some symbols (EOF, INDENT, and DEDENT).
 * length excludes the NULL terminator.
 */
typedef struct Token {
	token_type_t type;
	token_location_t location;
	char *lexeme;
	size_t length;
} token_t;

#endif /* IFJ_TOKEN_H */
