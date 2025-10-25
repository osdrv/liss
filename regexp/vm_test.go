package regexp

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestVMMatch(t *testing.T) {
	tests := []struct {
		name       string
		program    []Inst
		input      string
		numCaps    int
		wantOk     bool
		wantGroups []int
	}{
		{
			name: "single rune match",
			program: []Inst{
				{Op: ReOpRune, Rune: 'a', Out: 1},
				{Op: ReOpMatch},
			},
			input:  "a",
			wantOk: true,
		},
		{
			name: "single rune mismatch",
			program: []Inst{
				{Op: ReOpRune, Rune: 'a', Out: 1},
				{Op: ReOpMatch},
			},
			input:  "b",
			wantOk: false,
		},
		{
			name: "a(b|c)*d on abcd:",
			program: []Inst{
				{Op: ReOpRune, Rune: 'a', Out: 1}, // 0: 'a' -> 1
				{Op: ReOpSplit, Out: 2, Out1: 7},  // 1: loop start (epsilon)
				{Op: ReOpSplit, Out: 3, Out1: 5},  // 2: alternation start (epsilon)
				{Op: ReOpRune, Rune: 'b', Out: 4}, // 3: 'b' -> 4
				{Op: ReOpJump, Out: 6},            // 4: Jump to loop end
				{Op: ReOpRune, Rune: 'c', Out: 6}, // 5: 'c' -> 6
				{Op: ReOpJump, Out: 1},            // 6: Jump back to loop start
				{Op: ReOpRune, Rune: 'd', Out: 8}, // 7: 'd' -> 8
				{Op: ReOpMatch},                   // 8: Success
			},
			input:  "abcd",
			wantOk: true,
		},
		{
			name: "a(b|c)*d on ad:",
			program: []Inst{
				{Op: ReOpRune, Rune: 'a', Out: 1}, // 0: 'a' -> 1
				{Op: ReOpSplit, Out: 2, Out1: 7},  // 1: loop start (epsilon)
				{Op: ReOpSplit, Out: 3, Out1: 5},  // 2: alternation start (epsilon)
				{Op: ReOpRune, Rune: 'b', Out: 4}, // 3: 'b' -> 4
				{Op: ReOpJump, Out: 6},            // 4: Jump to loop end
				{Op: ReOpRune, Rune: 'c', Out: 6}, // 5: 'c' -> 6
				{Op: ReOpJump, Out: 1},            // 6: Jump back to loop start
				{Op: ReOpRune, Rune: 'd', Out: 8}, // 7: 'd' -> 8
				{Op: ReOpMatch},                   // 8: Success
			},
			input:  "ad",
			wantOk: true,
		},
		{
			name: "a(b|c)*d on axd:",
			program: []Inst{
				{Op: ReOpRune, Rune: 'a', Out: 1}, // 0: 'a' -> 1
				{Op: ReOpSplit, Out: 2, Out1: 7},  // 1: loop start (epsilon)
				{Op: ReOpSplit, Out: 3, Out1: 5},  // 2: alternation start (epsilon)
				{Op: ReOpRune, Rune: 'b', Out: 4}, // 3: 'b' -> 4
				{Op: ReOpJump, Out: 6},            // 4: Jump to loop end
				{Op: ReOpRune, Rune: 'c', Out: 6}, // 5: 'c' -> 6
				{Op: ReOpJump, Out: 1},            // 6: Jump back to loop start
				{Op: ReOpRune, Rune: 'd', Out: 8}, // 7: 'd' -> 8
				{Op: ReOpMatch},                   // 8: Success
			},
			input:  "axd",
			wantOk: false,
		},
		{
			name: "capture single rune",
			program: []Inst{
				{Op: ReOpCaptureStart, Arg: 1, Out: 1}, // 0: Capture start for group 1
				{Op: ReOpRune, Rune: 'a', Out: 2},      // 1: 'a' -> 2
				{Op: ReOpCaptureEnd, Arg: 1, Out: 3},   // 2: Capture end for group 1
				{Op: ReOpRune, Rune: 'b', Out: 4},      // 3: 'b' -> 4
				{Op: ReOpMatch},                        // 4: Match
			},
			numCaps:    1,
			input:      "ab",
			wantOk:     true,
			wantGroups: []int{0, 2, 0, 1}, // group 0: [0,2), group 1: [0,1)
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			vm := NewVM(tt.program, 0)
			gotGroups, gotOk := vm.Match(tt.input, tt.numCaps)
			assert.Equal(t, tt.wantOk, gotOk, "VM.Match(%q) = %v; want %v", tt.input, gotOk, tt.wantOk)
			if tt.wantGroups != nil {
				assert.Equal(t, tt.wantGroups, gotGroups, "VM.Match(%q) groups = %v; want %v", tt.input, gotGroups, tt.wantGroups)
			}
		})
	}
}
