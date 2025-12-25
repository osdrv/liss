# Compiler and flags
CC = clang

# By default, compile without debug flags.
# To enable, run: make DEBUG=1
DEBUG_FLAGS =
ifeq ($(DEBUG),1)
	DEBUG_FLAGS = -DLISS_DEBUG_BUILD -g
endif

CFLAGS = -std=c17 -Wall -Wextra -Wpedantic $(DEBUG_FLAGS)
LDFLAGS =

# Project structure
SRCDIR = src
OBJDIR = obj
BINDIR = bin
TESTDIR = tests

# Source files and object files
SRCS = $(wildcard $(SRCDIR)/*.c) $(SRCDIR)/object.c
OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))
# Application objects excluding main, for linking into tests
OBJS_NO_MAIN = $(filter-out $(OBJDIR)/main.o, $(OBJS))

# Test source files and object files
TEST_SRCS = $(wildcard $(TESTDIR)/*.c)
TEST_OBJS = $(patsubst $(TESTDIR)/%.c, $(OBJDIR)/%.o, $(TEST_SRCS))

# Main executable
TARGET = $(BINDIR)/liss

# Test runner executable
TEST_RUNNER = $(BINDIR)/test_runner

.PHONY: all run test clean format lint

all: $(TARGET)

run: $(TARGET)
	@./$(TARGET)

test: $(TEST_RUNNER)
	@./$(TEST_RUNNER)

# Main executable target
$(TARGET): $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Test runner target
$(TEST_RUNNER): $(OBJS_NO_MAIN) $(TEST_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -I$(SRCDIR)

# Rule to compile source files into object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -c -o $@ $<

# Rule to compile test files into object files
$(OBJDIR)/%.o: $(TESTDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -c -o $@ $<

# Create directories if they don't exist
$(BINDIR) $(OBJDIR):
	mkdir -p $@

clean:
	rm -rf $(OBJDIR) $(BINDIR)

format:
	clang-format -i -style=file $(SRCDIR)/*.c $(SRCDIR)/*.h $(TESTDIR)/*.c

lint:
	clang-tidy $(SRCDIR)/*.c $(TESTDIR)/*.c -- -I$(SRCDIR)
