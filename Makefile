.PHONY: all build run test bench clean watch-test

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
	./${BINARY_NAME} -src ./std/math_test.liss
	./${BINARY_NAME} -src ./std/json_test.liss

# Example: make watch-test FILE=vm/vm.go
watch-test:
	@echo "Watching $(FILE) for changes and running tests..."
	fswatch -o $(FILE) | xargs -n1 -I{} make test
