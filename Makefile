.PHONY: all build run test bench clean

BINARY_NAME=liss

all: build

# Build the application binary
build:
	@echo "Building $(BINARY_NAME)..."
	@go build -o $(BINARY_NAME) .

# Run the application
run:
	@go run .

# Run all tests
test:
	@echo "Running tests..."
	@go test ./...

# Run all benchmarks
bench:
	@echo "Running benchmarks..."
	@go test -bench=. ./...

# Remove the application binary
clean:
	@echo "Cleaning up..."
	@rm -f $(BINARY_NAME)

test-std:
	@echo "Running Liss standard library tests..."
	./${BINARY_NAME} -src ./std/list_test.liss
	./${BINARY_NAME} -src ./std/strings_test.liss
