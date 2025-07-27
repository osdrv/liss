package vm

import "fmt"

type TypeMismatchError struct {
	err string
}

var _ error = (*TypeMismatchError)(nil)

func (e *TypeMismatchError) Error() string {
	return fmt.Sprintf("Type mismatch: %s", e.err)
}

func NewTypeMismatchError(err string) *TypeMismatchError {
	return &TypeMismatchError{err: err}
}

type UnsupportedOpTypeError struct {
	err string
}

var _ error = (*UnsupportedOpTypeError)(nil)

func (e *UnsupportedOpTypeError) Error() string {
	return fmt.Sprintf("Unsupported operation for type: %s", e.err)
}

func NewUnsupportedOpTypeError(err string) *UnsupportedOpTypeError {
	return &UnsupportedOpTypeError{err: err}
}
