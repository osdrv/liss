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
				{Op: ReOpMatch},
				{Op: ReOpRune, Rune: 'a', Out: 0},
			}, 1, 0},
			input:      "a",
			wantOk:     true,
			wantGroups: []int{0, 1},
		},
		{
			name: "single rune mismatch",
			program: &Prog{[]Inst{
				{Op: ReOpMatch},
				{Op: ReOpRune, Rune: 'a', Out: 0},
			}, 1, 0},
			input:  "b",
			wantOk: false,
		},
		{
			name: "a(b|c)*d on abcd:",
			program: &Prog{[]Inst{
				{Op: ReOpMatch},                   // 0: Success
				{Op: ReOpRune, Rune: 'd', Out: 0}, // 1: 'd' -> 0
				{Op: ReOpJump, Out: 7},            // 2: Jump back to loop start
				{Op: ReOpRune, Rune: 'c', Out: 2}, // 3: 'c' -> 2
				{Op: ReOpJump, Out: 2},            // 4: Jump to loop end
				{Op: ReOpRune, Rune: 'b', Out: 4}, // 5: 'b' -> 4
				{Op: ReOpSplit, Out: 5, Out1: 3},  // 6: alternation start (epsilon)
				{Op: ReOpSplit, Out: 6, Out1: 1},  // 7: loop start (epsilon)
				{Op: ReOpRune, Rune: 'a', Out: 7}, // 8: 'a' -> 7
			}, 8, 0},
			input:  "abcd",
			wantOk: true,
		},
		{
			name: "a(b|c)*d on ad:",
			program: &Prog{[]Inst{
				{Op: ReOpMatch},                   // 0: Success
				{Op: ReOpRune, Rune: 'd', Out: 0}, // 1: 'd' -> 0
				{Op: ReOpJump, Out: 7},            // 2: Jump back to loop start
				{Op: ReOpRune, Rune: 'c', Out: 2}, // 3: 'c' -> 2
				{Op: ReOpJump, Out: 2},            // 4: Jump to loop end
				{Op: ReOpRune, Rune: 'b', Out: 4}, // 5: 'b' -> 4
				{Op: ReOpSplit, Out: 5, Out1: 3},  // 6: alternation start (epsilon)
				{Op: ReOpSplit, Out: 6, Out1: 1},  // 7: loop start (epsilon)
				{Op: ReOpRune, Rune: 'a', Out: 7}, // 8: 'a' -> 7
			}, 8, 0},
			input:  "ad",
			wantOk: true,
		},
		{
			name: "a(b|c)*d on axd:",
			program: &Prog{[]Inst{
				{Op: ReOpMatch},                   // 0: Success
				{Op: ReOpRune, Rune: 'd', Out: 0}, // 1: 'd' -> 0
				{Op: ReOpJump, Out: 7},            // 2: Jump back to loop start
				{Op: ReOpRune, Rune: 'c', Out: 2}, // 3: 'c' -> 2
				{Op: ReOpJump, Out: 2},            // 4: Jump to loop end
				{Op: ReOpRune, Rune: 'b', Out: 4}, // 5: 'b' -> 4
				{Op: ReOpSplit, Out: 5, Out1: 3},  // 6: alternation start (epsilon)
				{Op: ReOpSplit, Out: 6, Out1: 1},  // 7: loop start (epsilon)
				{Op: ReOpRune, Rune: 'a', Out: 7}, // 8: 'a' -> 7
			}, 8, 0},
			input:  "axd",
			wantOk: false,
		},
		{
			name: "capture single rune",
			program: &Prog{[]Inst{
				{Op: ReOpMatch},                        // 0: Match
				{Op: ReOpRune, Rune: 'b', Out: 0},      // 1: 'b' -> 4
				{Op: ReOpCaptureEnd, Arg: 0, Out: 1},   // 2: Capture end for group 1
				{Op: ReOpRune, Rune: 'a', Out: 2},      // 3: 'a' -> 2
				{Op: ReOpCaptureStart, Arg: 0, Out: 3}, // 4: Capture start for group 1
			}, 4, 1},
			input:      "ab",
			wantOk:     true,
			wantGroups: []int{0, 2, 0, 1}, // group 0: [0,2), group 1: [0,1)
		},
		{
			name: "capture with * matcher",
			program: &Prog{[]Inst{
				{Op: ReOpMatch},                        // 6 0: Match
				{Op: ReOpRune, Rune: 'b', Out: 0},      // 5 1: 'b' -> 0
				{Op: ReOpCaptureEnd, Arg: 0, Out: 1},   // 4 2: Capture end for group 1
				{Op: ReOpJump, Out: 5},                 // 3 3: Jump back to loop start
				{Op: ReOpRune, Rune: 'a', Out: 3},      // 2 4: 'a' -> 3
				{Op: ReOpSplit, Out: 4, Out1: 2},       // 1 5: loop start (epsilon)
				{Op: ReOpCaptureStart, Arg: 0, Out: 5}, // 0 6: Capture start for group 1
			}, 6, 1},
			input:      "aaaab",
			wantOk:     true,
			wantGroups: []int{0, 5, 0, 4}, // group 0: [0,5), group 1: [0,4)
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			t.Log(tt.program.String())
			gotGroups, gotOk := tt.program.Match(tt.input)
			assert.Equal(t, tt.wantOk, gotOk, "VM.Match(%q) = %v; want %v", tt.input, gotOk, tt.wantOk)
			if tt.wantGroups != nil {
				assert.Equal(t, tt.wantGroups, gotGroups, "VM.Match(%q) groups = %v; want %v", tt.input, gotGroups, tt.wantGroups)
			}
		})
	}
}
