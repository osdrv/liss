package regexp

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestVMMatch(t *testing.T) {
	tests := []struct {
		name       string
		input      string
		program    *Prog
		wantOk     bool
		wantGroups []int
	}{
		{
			name: "single rune match",
			program: &Prog{[]Inst{
				{Op: ReOpRune, Rune: 'a', Out: 1},
				{Op: ReOpMatch},
			}, 0, 0},
			input:  "a",
			wantOk: true,
		},
		{
			name: "single rune mismatch",
			program: &Prog{[]Inst{
				{Op: ReOpRune, Rune: 'a', Out: 1},
				{Op: ReOpMatch},
			}, 0, 0},
			input:  "b",
			wantOk: false,
		},
		{
			name: "a(b|c)*d on abcd:",
			program: &Prog{[]Inst{
				{Op: ReOpRune, Rune: 'a', Out: 1}, // 0: 'a' -> 1
				{Op: ReOpSplit, Out: 2, Out1: 7},  // 1: loop start (epsilon)
				{Op: ReOpSplit, Out: 3, Out1: 5},  // 2: alternation start (epsilon)
				{Op: ReOpRune, Rune: 'b', Out: 4}, // 3: 'b' -> 4
				{Op: ReOpJump, Out: 6},            // 4: Jump to loop end
				{Op: ReOpRune, Rune: 'c', Out: 6}, // 5: 'c' -> 6
				{Op: ReOpJump, Out: 1},            // 6: Jump back to loop start
				{Op: ReOpRune, Rune: 'd', Out: 8}, // 7: 'd' -> 8
				{Op: ReOpMatch},                   // 8: Success
			}, 0, 0},
			input:  "abcd",
			wantOk: true,
		},
		{
			name: "a(b|c)*d on ad:",
			program: &Prog{[]Inst{
				{Op: ReOpRune, Rune: 'a', Out: 1}, // 0: 'a' -> 1
				{Op: ReOpSplit, Out: 2, Out1: 7},  // 1: loop start (epsilon)
				{Op: ReOpSplit, Out: 3, Out1: 5},  // 2: alternation start (epsilon)
				{Op: ReOpRune, Rune: 'b', Out: 4}, // 3: 'b' -> 4
				{Op: ReOpJump, Out: 6},            // 4: Jump to loop end
				{Op: ReOpRune, Rune: 'c', Out: 6}, // 5: 'c' -> 6
				{Op: ReOpJump, Out: 1},            // 6: Jump back to loop start
				{Op: ReOpRune, Rune: 'd', Out: 8}, // 7: 'd' -> 8
				{Op: ReOpMatch},                   // 8: Success
			}, 0, 0},
			input:  "ad",
			wantOk: true,
		},
		{
			name: "a(b|c)*d on axd:",
			program: &Prog{[]Inst{
				{Op: ReOpRune, Rune: 'a', Out: 1}, // 0: 'a' -> 1
				{Op: ReOpSplit, Out: 2, Out1: 7},  // 1: loop start (epsilon)
				{Op: ReOpSplit, Out: 3, Out1: 5},  // 2: alternation start (epsilon)
				{Op: ReOpRune, Rune: 'b', Out: 4}, // 3: 'b' -> 4
				{Op: ReOpJump, Out: 6},            // 4: Jump to loop end
				{Op: ReOpRune, Rune: 'c', Out: 6}, // 5: 'c' -> 6
				{Op: ReOpJump, Out: 1},            // 6: Jump back to loop start
				{Op: ReOpRune, Rune: 'd', Out: 8}, // 7: 'd' -> 8
				{Op: ReOpMatch},                   // 8: Success
			}, 0, 0},
			input:  "axd",
			wantOk: false,
		},
		{
			name: "capture single rune",
			program: &Prog{[]Inst{
				{Op: ReOpCaptureStart, Arg: 1, Out: 1}, // 0: Capture start for group 1
				{Op: ReOpRune, Rune: 'a', Out: 2},      // 1: 'a' -> 2
				{Op: ReOpCaptureEnd, Arg: 1, Out: 3},   // 2: Capture end for group 1
				{Op: ReOpRune, Rune: 'b', Out: 4},      // 3: 'b' -> 4
				{Op: ReOpMatch},                        // 4: Match
			}, 0, 1},
			input:      "ab",
			wantOk:     true,
			wantGroups: []int{0, 2, 0, 1}, // group 0: [0,2), group 1: [0,1)
		},
		{
			name: "capture with * matcher",
			program: &Prog{[]Inst{
				{Op: ReOpCaptureStart, Arg: 1, Out: 1}, // 0: Capture start for group 1
				{Op: ReOpSplit, Out: 2, Out1: 4},       // 1: loop start (epsilon)
				{Op: ReOpRune, Rune: 'a', Out: 3},      // 2: 'a' -> 3
				{Op: ReOpJump, Out: 1},                 // 3: Jump back to loop start
				{Op: ReOpCaptureEnd, Arg: 1, Out: 5},   // 4: Capture end for group 1
				{Op: ReOpRune, Rune: 'b', Out: 6},      // 5: 'b' -> 6
				{Op: ReOpMatch},                        // 6: Match
			}, 0, 1},
			input:      "aaaab",
			wantOk:     true,
			wantGroups: []int{0, 5, 0, 4}, // group 0: [0,5), group 1: [0,4)
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			gotGroups, gotOk := tt.program.Match(tt.input)
			assert.Equal(t, tt.wantOk, gotOk, "VM.Match(%q) = %v; want %v", tt.input, gotOk, tt.wantOk)
			if tt.wantGroups != nil {
				assert.Equal(t, tt.wantGroups, gotGroups, "VM.Match(%q) groups = %v; want %v", tt.input, gotGroups, tt.wantGroups)
			}
		})
	}
}
