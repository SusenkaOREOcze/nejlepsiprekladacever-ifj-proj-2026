CC := gcc
CFLAGS := -std=gnu23 -Wall -Wextra -Wpedantic
CPPFLAGS := -Isrc/include

# Lets make it fancy, we sort and remove duplicates from the lists of source files and headers.
SRC := $(sort $(wildcard src/*.c))
HEADERS := $(sort $(wildcard src/include/*.h))

YELLOW = \033[33m
GREEN = \033[32m
RESET = \033[0m

.PHONY: tidy tidy-fix tidy-all format format-check check fix

tidy:
	clang-tidy --use-color $(SRC) -- $(CFLAGS) $(CPPFLAGS)

tidy-fix:
	clang-tidy --use-color --fix $(SRC) -- $(CFLAGS) $(CPPFLAGS)

tidy-all:
	clang-tidy --use-color --checks=* $(SRC) -- $(CFLAGS) $(CPPFLAGS)

format:
	clang-format -i $(SRC) $(HEADERS)

format-check:
	clang-format --dry-run --Werror $(SRC) $(HEADERS)

check:
	@printf "$(YELLOW)Running clang-format check...$(RESET)\n"
	$(MAKE) format-check
	@printf "$(YELLOW)Running clang-tidy...$(RESET)\n"
	$(MAKE) tidy
	@printf "$(GREEN)All checks finished.$(RESET)\n"

fix:
	@printf "$(YELLOW)Applying clang-tidy fixes...$(RESET)\n"
	$(MAKE) tidy-fix
	@printf "$(YELLOW)Running clang-format...$(RESET)\n"
	$(MAKE) format
	@printf "$(GREEN)Fixes finished.$(RESET)\n"
# format is run after tidy-fix, so the "FIXED" code is formatted as well. 