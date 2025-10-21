package object

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestGetFuncNumArgs(t *testing.T) {
	tests := []struct {
		name     string
		fn       any
		wantArgc int
		wantErr  error
	}{
		{
			name:     "Function with no arguments",
			fn:       func() {},
			wantArgc: 0,
		},
		{
			name:     "Function with one argument",
			fn:       func(a int) {},
			wantArgc: 1,
		},
		{
			name:     "Function with multiple arguments",
			fn:       func(a int, b string, c float64) {},
			wantArgc: 3,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			argc, err := getFuncNumArgs(tt.fn)
			if err != tt.wantErr {
				t.Errorf("getFuncNumArgs() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			if argc != tt.wantArgc {
				t.Errorf("getFuncNumArgs() = %v, want %v", argc, tt.wantArgc)
			}
		})
	}
}

func TestBuiltinLen(t *testing.T) {
	tests := []struct {
		name    string
		input   Object
		want    int64
		wantErr bool
	}{
		{
			name:    "Length of a string",
			input:   &String{Value: "hello"},
			want:    5,
			wantErr: false,
		},
		{
			name:    "Length of an array",
			input:   &List{items: []Object{&Integer{Value: 1}, &Integer{Value: 2}, &Integer{Value: 3}}},
			want:    3,
			wantErr: false,
		},
		// {
		// 	name:    "Length of a dictionary",
		// 	input:   &Dictionary{items: map[any]Object{"a": &Integer{Value: 1}, "b": &Integer{Value: 2}}},
		// 	want:    2,
		// 	wantErr: false,
		// },
		{
			name:    "Length of a non-lenable object",
			input:   &Integer{Value: 42},
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := builtinLen(tt.input)
			if (err != nil) != tt.wantErr {
				t.Errorf("BuiltinLen() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			if !tt.wantErr {
				if gotInt, ok := got.(*Integer); ok {
					if gotInt.Value != tt.want {
						t.Errorf("BuiltinLen() = %v, want %v", gotInt.Value, tt.want)
					}
				} else {
					t.Errorf("BuiltinLen() returned non-integer type")
				}
			}
		})
	}
}

func TestBuiltinHead(t *testing.T) {
	tests := []struct {
		name    string
		input   Object
		want    Object
		wantErr bool
	}{
		{
			name:    "Head element of a non-empty array",
			input:   &List{items: []Object{&Integer{Value: 1}, &Integer{Value: 2}, &Integer{Value: 3}}},
			want:    &Integer{Value: 1},
			wantErr: false,
		},
		{
			name:    "Head element of an empty array",
			input:   &List{items: []Object{}},
			want:    &Null{},
			wantErr: false,
		},
		{
			name:    "Head character of a non-empty string",
			input:   &String{Value: "hello"},
			want:    &String{Value: "h"},
			wantErr: false,
		},
		{
			name:    "Head character of an empty string",
			input:   &String{Value: ""},
			want:    &Null{},
			wantErr: false,
		},
		{
			name:    "Head element of a non-lenable object",
			input:   &Integer{Value: 42},
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := builtinHead(tt.input)
			if (err != nil) != tt.wantErr {
				t.Errorf("builtinHead() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			if !tt.wantErr {
				if got.String() != tt.want.String() {
					t.Errorf("builtinHead() = %v, want %v", got.String(), tt.want.String())
				}
			}
		})
	}
}

func TestBuiltinLast(t *testing.T) {
	tests := []struct {
		name    string
		input   Object
		want    Object
		wantErr bool
	}{
		{
			name:    "Last element of a non-empty array",
			input:   &List{items: []Object{&Integer{Value: 1}, &Integer{Value: 2}, &Integer{Value: 3}}},
			want:    &Integer{Value: 3},
			wantErr: false,
		},
		{
			name:    "Last element of an empty array",
			input:   &List{items: []Object{}},
			want:    &Null{},
			wantErr: false,
		},
		{
			name:    "Last character of a non-empty string",
			input:   &String{Value: "hello"},
			want:    &String{Value: "o"},
			wantErr: false,
		},
		{
			name:    "Last character of an empty string",
			input:   &String{Value: ""},
			want:    &Null{},
			wantErr: false,
		},
		{
			name:    "Last element of a non-lenable object",
			input:   &Integer{Value: 42},
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := builtinLast(tt.input)
			if (err != nil) != tt.wantErr {
				t.Errorf("BuiltinLast() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			if !tt.wantErr {
				if got.String() != tt.want.String() {
					t.Errorf("BuiltinLast() = %v, want %v", got.String(), tt.want.String())
				}
			}
		})
	}
}

func TestBuiltinTail(t *testing.T) {
	tests := []struct {
		name    string
		input   Object
		want    Object
		wantErr bool
	}{
		{
			name:    "Tail of a non-empty array",
			input:   &List{items: []Object{&Integer{Value: 1}, &Integer{Value: 2}, &Integer{Value: 3}}},
			want:    &List{items: []Object{&Integer{Value: 2}, &Integer{Value: 3}}},
			wantErr: false,
		},
		{
			name:    "Tail of an array with one element",
			input:   &List{items: []Object{&Integer{Value: 1}}},
			want:    &Null{},
			wantErr: false,
		},
		{
			name:    "Tail of an empty array",
			input:   &List{items: []Object{}},
			want:    &Null{},
			wantErr: false,
		},
		{
			name:    "Tail of a non-empty string",
			input:   &String{Value: "hello"},
			want:    &String{Value: "ello"},
			wantErr: false,
		},
		{
			name:    "Tail of a string with one character",
			input:   &String{Value: "h"},
			want:    &Null{},
			wantErr: false,
		},
		{
			name:    "Tail of an empty string",
			input:   &String{Value: ""},
			want:    &Null{},
			wantErr: false,
		},
		{
			name:    "Tail of a non-lenable object",
			input:   &Integer{Value: 42},
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := builtinTail(tt.input)
			if (err != nil) != tt.wantErr {
				t.Errorf("builtinTail() error = %v, wantErr %v", err, tt.wantErr)
				return
			}

			if !tt.wantErr {
				if got.String() != tt.want.String() {
					t.Errorf("builtinTail() = %v, want %v", got.String(), tt.want.String())
				}
			}
		})
	}
}

func TestBuiltinList(t *testing.T) {
	tests := []struct {
		name    string
		input   []Object
		want    Object
		wantErr bool
	}{
		{
			name:  "empty list",
			input: []Object{},
			want:  &List{items: []Object{}},
		},
		{
			name: "list with one element",
			input: []Object{
				&Integer{Value: 1},
			},
			want: &List{items: []Object{&Integer{Value: 1}}},
		},
		{
			name: "list with multiple elements",
			input: []Object{
				&Integer{Value: 1},
				&String{Value: "two"},
				&Bool{Value: true},
			},
			want: &List{items: []Object{
				&Integer{Value: 1},
				&String{Value: "two"},
				&Bool{Value: true},
			}},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := builtinList(tt.input...)
			if (err != nil) != tt.wantErr {
				t.Errorf("BuiltinList() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			if !tt.wantErr {
				if got.String() != tt.want.String() {
					t.Errorf("BuiltinList() = %v, want %v", got.String(), tt.want.String())
				}
			}
		})
	}
}

func TestBuiltinGet(t *testing.T) {
	tests := []struct {
		name    string
		input   Object
		key     Object
		want    Object
		wantErr bool
	}{
		{
			name:  "Get element from array by index",
			input: &List{items: []Object{&Integer{Value: 10}, &Integer{Value: 20}, &Integer{Value: 30}}},
			key:   &Integer{Value: 1},
			want:  &Integer{Value: 20},
		},
		{
			name:  "Get character from string by index",
			input: &String{Value: "hello"},
			key:   &Integer{Value: 4},
			want:  &String{Value: "o"},
		},
		{
			name:  "Get element with invalid index",
			input: &List{items: []Object{&Integer{Value: 10}, &Integer{Value: 20}}},
			key:   &Integer{Value: 5},
			want:  &Null{},
		},
		{
			name:  "Get character with invalid index",
			input: &String{Value: "hi"},
			key:   &Integer{Value: -1},
			want:  &Null{},
		},
		{
			name:    "Get from non-indexable object",
			input:   &Integer{Value: 42},
			key:     &Integer{Value: 0},
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := builtinGet(tt.input, tt.key)
			if (err != nil) != tt.wantErr {
				t.Errorf("BuiltinGet() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			if !tt.wantErr {
				assert.Equal(t, tt.want.String(), got.String(), "BuiltinGet() = %v, want %v", got.String(), tt.want.String())
			}
		})
	}
}
