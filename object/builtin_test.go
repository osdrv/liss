package object

import "testing"

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
			input:   &Array{items: []Object{&Integer{Value: 1}, &Integer{Value: 2}, &Integer{Value: 3}}},
			want:    3,
			wantErr: false,
		},
		{
			name:    "Length of a map",
			input:   &Dictionary{items: map[string]Object{"a": &Integer{Value: 1}, "b": &Integer{Value: 2}}},
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
			got, err := BuiltinLen(tt.input)
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

func TestBuiltinFirst(t *testing.T) {
	tests := []struct {
		name    string
		input   Object
		want    Object
		wantErr bool
	}{
		{
			name:    "First element of a non-empty array",
			input:   &Array{items: []Object{&Integer{Value: 1}, &Integer{Value: 2}, &Integer{Value: 3}}},
			want:    &Integer{Value: 1},
			wantErr: false,
		},
		{
			name:    "First element of an empty array",
			input:   &Array{items: []Object{}},
			want:    &Null{},
			wantErr: false,
		},
		{
			name:    "First character of a non-empty string",
			input:   &String{Value: "hello"},
			want:    &String{Value: "h"},
			wantErr: false,
		},
		{
			name:    "First character of an empty string",
			input:   &String{Value: ""},
			want:    &Null{},
			wantErr: false,
		},
		{
			name:    "First element of a non-lenable object",
			input:   &Integer{Value: 42},
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := BuiltinFirst(tt.input)
			if (err != nil) != tt.wantErr {
				t.Errorf("BuiltinFirst() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			if !tt.wantErr {
				if got.String() != tt.want.String() {
					t.Errorf("BuiltinFirst() = %v, want %v", got.String(), tt.want.String())
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
			input:   &Array{items: []Object{&Integer{Value: 1}, &Integer{Value: 2}, &Integer{Value: 3}}},
			want:    &Integer{Value: 3},
			wantErr: false,
		},
		{
			name:    "Last element of an empty array",
			input:   &Array{items: []Object{}},
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
			got, err := BuiltinLast(tt.input)
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

func TestBuiltinRest(t *testing.T) {
	tests := []struct {
		name    string
		input   Object
		want    Object
		wantErr bool
	}{
		{
			name:    "Rest of a non-empty array",
			input:   &Array{items: []Object{&Integer{Value: 1}, &Integer{Value: 2}, &Integer{Value: 3}}},
			want:    &Array{items: []Object{&Integer{Value: 2}, &Integer{Value: 3}}},
			wantErr: false,
		},
		{
			name:    "Rest of an array with one element",
			input:   &Array{items: []Object{&Integer{Value: 1}}},
			want:    &Null{},
			wantErr: false,
		},
		{
			name:    "Rest of an empty array",
			input:   &Array{items: []Object{}},
			want:    &Null{},
			wantErr: false,
		},
		{
			name:    "Rest of a non-empty string",
			input:   &String{Value: "hello"},
			want:    &String{Value: "ello"},
			wantErr: false,
		},
		{
			name:    "Rest of a string with one character",
			input:   &String{Value: "h"},
			want:    &Null{},
			wantErr: false,
		},
		{
			name:    "Rest of an empty string",
			input:   &String{Value: ""},
			want:    &Null{},
			wantErr: false,
		},
		{
			name:    "Rest of a non-lenable object",
			input:   &Integer{Value: 42},
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := BuiltinRest(tt.input)
			if (err != nil) != tt.wantErr {
				t.Errorf("BuiltinRest() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			if !tt.wantErr {
				if got.String() != tt.want.String() {
					t.Errorf("BuiltinRest() = %v, want %v", got.String(), tt.want.String())
				}
			}
		})
	}
}
