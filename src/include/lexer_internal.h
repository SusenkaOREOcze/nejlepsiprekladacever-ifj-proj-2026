#ifndef IFJ_LEXER_INTERNAL_H
#define IFJ_LEXER_INTERNAL_H

/**
 * @file lexer_internal.h
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-10-10
 * @brief IFJ26 lexer implementation header.
 *
 * This header contains internal declarations for the lexer implementation.
 * It is not intended for public use and should only be included in the lexer implementation files.
 */

#include "lexer.h"

/**
 * @brief Destroys a token and frees its resources.
 * @param token Pointer to the token to destroy. If NULL, the function does nothing.
 */
void lexer_token_destroy(token_t *token);

/**
 * @brief Sets an error in the lexer with the specified code and message.
 * @param lexer Pointer to the lexer. If NULL or if an error has already been set, the function does nothing.
 * @param code The error code to set.
 * @param message The error message to set.
 */
void lexer_set_error(lexer_t *lexer, lexer_result_t code, const char *message);

/**
 * @brief Scans the next token from the lexer and stores it in the provided output token.
 * @param lexer Pointer to the lexer from which to scan the token.
 * @param out_token Pointer to the token structure where the scanned token will be stored.
 * @return Returns a lexer_result_t indicating the result of the scanning operation.
 */
lexer_result_t lexer_scan_token(lexer_t *lexer, token_t *out_token);

/**
 * @brief Determines the token type for a given keyword text.
 * @param text The keyword text to check.
 * @param length The length of the keyword text.
 * @return Returns the corresponding token_type_t for the keyword, or TOKEN_IDENTIFIER if the text does not
 * match any known keyword.
 */
token_type_t lexer_keyword_type(const char *text, size_t length);

#endif /* IFJ_LEXER_INTERNAL_H */
