CC       ?= cc
PREFIX   ?= /usr/local
BINDIR   := $(PREFIX)/bin

SRC_DIR  := src
BUILD_DIR := build

CFLAGS   ?= -O2 -g
CFLAGS   += -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
            -Wno-unused-parameter -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
LDFLAGS  ?=

SOURCES := $(shell find $(SRC_DIR) -name '*.c' ! -path '$(SRC_DIR)/tests/*')
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS    := $(OBJECTS:.o=.d)

TARGET  := calm

.PHONY: all clean test install uninstall

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET) tests/run_tests

# Test sources link against everything except main.c, plus the test
# runner's own main().
TEST_SOURCES := $(filter-out $(SRC_DIR)/main.c,$(SOURCES))
TEST_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(TEST_SOURCES))

tests/run_tests: tests/test_main.c $(TEST_OBJECTS)
	$(CC) $(CFLAGS) -I$(SRC_DIR) tests/test_main.c $(TEST_OBJECTS) -o $@ $(LDFLAGS)

test: tests/run_tests
	./tests/run_tests

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
