package regexp

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestMatchString(t *testing.T) {
	tests := []struct {
		name    string
		input   string
		pattern string
		want    bool
	}{
		{
			name:    "empty string empty pattern",
			input:   "",
			pattern: "",
			want:    true,
		},
		{
			name:    "non-empty string empty pattern",
			input:   "abc",
			pattern: "",
			want:    false,
		},
		{
			name:    "match single rune",
			input:   "a",
			pattern: "a",
			want:    true,
		},
		{
			name:    "no match single rune",
			input:   "a",
			pattern: "b",
			want:    false,
		},
		{
			name:    "string exact match",
			input:   "hello",
			pattern: "hello",
			want:    true,
		},
		{
			name:    "string no match",
			input:   "hello",
			pattern: "world",
			want:    false,
		},
		{
			name:    "string partial match",
			input:   "hello",
			pattern: "hell",
			want:    false,
		},
		{
			name:    "string under match",
			input:   "hello",
			pattern: "hello world",
			want:    false,
		},
		{
			name:    "single char match against dot",
			input:   "a",
			pattern: ".",
			want:    true,
		},
		{
			name:    "multiple char match against dots",
			input:   "abc",
			pattern: "...",
			want:    true,
		},
		{
			name:    "wildcard dot match",
			input:   "abc",
			pattern: "a.c",
			want:    true,
		},
		{
			name:    "wildcard dot no match",
			input:   "abc",
			pattern: "a.d",
			want:    false,
		},
		{
			name:    "wildcard asterix match",
			input:   "abc",
			pattern: ".*",
			want:    true,
		},
		{
			name:    "wildcard asterix with a catch",
			input:   "abc",
			pattern: "a.*bc",
			want:    true,
		},
		{
			name:    "unicode match",
			input:   "привет",
			pattern: "при.*ет",
			want:    true,
		},
		{
			name:    "match digit class",
			input:   "123",
			pattern: "\\d+",
			want:    true,
		},
		{
			name:    "match multiple digit groups",
			input:   "123 456 789",
			pattern: "(\\d+) (\\d+) (\\d+)",
			want:    true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			re, err := Compile(tt.pattern)
			require.NoError(t, err, "Compile(%q) failed", tt.pattern)
			_, got := re.MatchString(tt.input)
			assert.Equal(t, tt.want, got, "MatchString(%q) with pattern %q", tt.input, tt.pattern)
		})
	}
}

func TestMatchWithCaptures(t *testing.T) {
	tests := []struct {
		name         string
		pattern      string
		input        string
		wantCaptures []string
		wantOk       bool
	}{
		{
			name:         "no captures match",
			pattern:      "hello",
			input:        "hello",
			wantCaptures: []string{"hello"},
			wantOk:       true,
		},
		{
			name:         "single capture match",
			pattern:      "(he)llo",
			input:        "hello",
			wantCaptures: []string{"hello", "he"},
			wantOk:       true,
		},
		{
			name:         "multiple captures match",
			pattern:      "(he)(ll)o",
			input:        "hello",
			wantCaptures: []string{"hello", "he", "ll"},
			wantOk:       true,
		},
		{
			name:    "test single rune capture",
			pattern: "(a)b",
			input:   "ab",
			wantCaptures: []string{
				"ab",
				"a",
			},
			wantOk: true,
		},
		{
			name:    "unicode capture",
			pattern: "(при)(вет)",
			input:   "привет",
			wantCaptures: []string{
				"привет",
				"при",
				"вет",
			},
			wantOk: true,
		},
		{
			name:    "capture from a multi-word sentence",
			pattern: `The (.*) jumps over the (.*)`,
			input:   "The quick brown fox jumps over the lazy dog",
			wantCaptures: []string{
				"The quick brown fox jumps over the lazy dog",
				"quick brown fox",
				"lazy dog",
			},
			wantOk: true,
		},
		{
			name:    "capture multiple digit groups",
			pattern: "(\\d+) (\\d+) (\\d+)",
			input:   "123 456 789",
			wantCaptures: []string{
				"123 456 789",
				"123",
				"456",
				"789",
			},
			wantOk: true,
		},
		{
			name:    "capture inside escaped parenthesis",
			pattern: "mul\\((\\d+),(\\d+)\\)",
			input:   "mul(12,34)",
			wantCaptures: []string{
				"mul(12,34)",
				"12",
				"34",
			},
			wantOk: true,
		},
		{
			name:    "captures first match",
			pattern: ".*(\\d+).*",
			input:   "abc 123 def 456",
			wantCaptures: []string{
				"abc 123 def 456",
				"123",
			},
			wantOk: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			re, err := Compile(tt.pattern)
			t.Log(re.Prog.String())
			require.NoError(t, err, "Compile(%q) failed", tt.pattern)
			captures, ok := re.MatchString(tt.input)
			assert.Equal(t, tt.wantOk, ok, "MatchString(%q) with pattern %q", tt.input, tt.pattern)
			if ok {
				assert.Equal(t, tt.wantCaptures, captures, "Captures for input %q with pattern %q", tt.input, tt.pattern)
			}
		})
	}
}

func TestMatchStringIx(t *testing.T) {
	tests := []struct {
		name        string
		pattern     string
		input       string
		wantIndices [][]int
		wantOk      bool
	}{
		{
			name:    "group match",
			pattern: "(he)(ll)o",
			input:   "hello",
			wantIndices: [][]int{
				{0, 5},
				{0, 2},
				{2, 4},
			},
			wantOk: true,
		},
		{
			name:    "capture first match",
			pattern: ".*(\\d+).*",
			input:   "abc 123 def 456",
			wantIndices: [][]int{
				{0, 15},
				{4, 7},
			},
			wantOk: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			re, err := Compile(tt.pattern)
			require.NoError(t, err, "Compile(%q) failed", tt.pattern)
			indices, ok := re.MatchStringIx(tt.input)
			assert.Equal(t, tt.wantOk, ok, "MatchStringIx(%q) with pattern %q", tt.input, tt.pattern)
			if ok {
				assert.Equal(t, tt.wantIndices, indices, "Capture indices for input %q with pattern %q", tt.input, tt.pattern)
			}
		})
	}
}
