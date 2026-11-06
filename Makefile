CC = gcc

CPPFLAGS = -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g
LDFLAGS =

SRC = $(wildcard src/*.c)
TARGET = mini-shell
PARSER_TEST = tests/parser_tests

.PHONY: all clean strict sanitize analyze gcc-analyze cppcheck test integration-test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) $(LDFLAGS) -o $@

strict:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -Werror -Wconversion -Wshadow -Wformat=2 -Wstrict-prototypes" $(TARGET)

sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -O1 -fsanitize=address,undefined -fno-omit-frame-pointer" LDFLAGS="$(LDFLAGS) -fsanitize=address,undefined" $(TARGET)

$(PARSER_TEST): tests/parser_tests.c src/parser.c include/parser.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/parser_tests.c src/parser.c -o $@

test: $(PARSER_TEST)
	./$(PARSER_TEST)

analyze:
	clang --analyze $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic $(SRC)

gcc-analyze:
	@set -e; temp_dir=$$(mktemp -d); trap 'rm -rf "$$temp_dir"' EXIT; \
	for file in $(SRC); do \
		gcc $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -fanalyzer -c "$$file" \
		-o "$$temp_dir/$$(basename "$$file" .c).o"; \
	done; \
	echo "GCC -fanalyzer completed without diagnostics"

cppcheck:
	cppcheck --enable=warning,style,performance,portability --error-exitcode=1 --std=c11 $(CPPFLAGS) src

integration-test: $(TARGET)
	python3 tests/integration_test.py ./$(TARGET)

clean:
	rm -f $(TARGET) mini-shell-asan $(PARSER_TEST) *.plist
