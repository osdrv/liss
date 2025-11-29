package object

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestBuiltinLen(t *testing.T) {
	tests := []struct {
		name    string
		input   Object
		want    int64
		wantErr bool
	}{
		{
			name:    "Length of a string",
			input:   &String{Value: []rune("hello")},
			want:    5,
			wantErr: false,
		},
		{
			name:    "Length of an array",
			input:   &List{items: []Object{&Integer{Value: 1}, &Integer{Value: 2}, &Integer{Value: 3}}},
			want:    3,
			wantErr: false,
		},
		{
			name:    "Length of a dictionary",
			input:   &Dictionary{len: 2},
			want:    2,
			wantErr: false,
		},
		{
			name:    "Length of a non-lenable object",
			input:   &Integer{Value: 42},
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := builtinLen([]Object{tt.input})
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
			input:   &String{Value: []rune("hello")},
			want:    &String{Value: []rune("h")},
			wantErr: false,
		},
		{
			name:    "Head character of an empty string",
			input:   &String{Value: []rune{}},
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
			got, err := builtinHead([]Object{tt.input})
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
			input:   &String{Value: []rune("hello")},
			want:    &String{Value: []rune{'o'}},
			wantErr: false,
		},
		{
			name:    "Last character of an empty string",
			input:   &String{Value: []rune{}},
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
			got, err := builtinLast([]Object{tt.input})
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
			want:    &List{},
			wantErr: false,
		},
		{
			name:    "Tail of an empty array",
			input:   &List{items: []Object{}},
			want:    &List{},
			wantErr: false,
		},
		{
			name:    "Tail of a non-empty string",
			input:   &String{Value: []rune("hello")},
			want:    &String{Value: []rune("ello")},
			wantErr: false,
		},
		{
			name:    "Tail of a string with one character",
			input:   &String{Value: []rune{'h'}},
			want:    &String{},
			wantErr: false,
		},
		{
			name:    "Tail of an empty string",
			input:   &String{Value: []rune{}},
			want:    &String{},
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
			got, err := builtinTail([]Object{tt.input})
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
				&String{Value: []rune("two")},
				&Bool{Value: true},
			},
			want: &List{items: []Object{
				&Integer{Value: 1},
				&String{Value: []rune("two")},
				&Bool{Value: true},
			}},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := builtinList(tt.input)
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
			input: &String{Value: []rune("hello")},
			key:   &Integer{Value: 4},
			want:  &String{Value: []rune{'o'}},
		},
		{
			name:  "Get element with invalid index",
			input: &List{items: []Object{&Integer{Value: 10}, &Integer{Value: 20}}},
			key:   &Integer{Value: 5},
			want:  &Null{},
		},
		{
			name:  "Get character with invalid index",
			input: &String{Value: []rune("hi")},
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
			got, err := builtinGet([]Object{tt.input, tt.key})
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

func TestBuiltinMatchIx(t *testing.T) {
	tests := []struct {
		name    string
		input   Object
		pattern Object
		want    [][]int64
		wantErr error
	}{
		{
			name:    "Simple match with captures",
			input:   &String{Value: []rune("hello world")},
			pattern: &String{Value: []rune("(hello) (world)")},
			want: [][]int64{
				{0, 11},
				{0, 5},
				{6, 11},
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := builtinReCaptureIx([]Object{tt.pattern, tt.input})
			if tt.wantErr != nil {
				assert.ErrorContains(t, err, tt.wantErr.Error(), "BuiltinReCaptureIx() unexpected error")
				return
			}
			require.Nil(t, err, "BuiltinReCaptureIx() unexpected error: %v", err)
			assertListEqual(t, got, tt.want, "BuiltinReCaptureIx() = %v, want %v", got, tt.want)
		})
	}
}

func assertListEqual(t *testing.T, got Object, want [][]int64, msgAndArgs ...any) {
	gotList, ok := got.(*List)
	require.True(t, ok, "Expected List object, got %T", got)
	plainList := make([][]int64, len(gotList.items))
	for i, item := range gotList.items {
		intList, ok := item.(*List)
		require.True(t, ok, "Expected List object at index %d, got %T", i, item)
		require.Len(t, intList.items, 2, "Expected list of length 2 at index %d, got length %d", i, len(intList.items))
		fromObj, ok := intList.items[0].(*Integer)
		require.True(t, ok, "Expected Integer object at index %d, got %T", i, intList.items[0])
		toObj, ok := intList.items[1].(*Integer)
		require.True(t, ok, "Expected Integer object at index %d, got %T", i, intList.items[1])
		plainList[i] = []int64{fromObj.Value, toObj.Value}
	}
	assert.Equal(t, want, plainList, msgAndArgs...)
}
