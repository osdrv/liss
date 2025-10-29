package regexp

import (
	"fmt"
	"strings"
)

type RuneClass = int

const (
	RuneClassDigit    RuneClass = 1 << iota // \d
	RuneClassWord                           // \w
	RuneClassSpace                          // \s
	RuneClassNotDigit                       // \D
	RuneClassNotWord                        // \W
	RuneClassNotSpace                       // \S
)

var (
	RuneClassMap = map[string]RuneClass{
		"d": RuneClassDigit,
		"w": RuneClassWord,
		"s": RuneClassSpace,
		"D": RuneClassNotDigit,
		"W": RuneClassNotWord,
		"S": RuneClassNotSpace,
	}
)

type ReOp uint8

const (
	ReOpMatch ReOp = iota
	ReOpRune
	ReOpRuneClass
	ReOpAny
	ReOpSplit
	ReOpCaptureStart
	ReOpCaptureEnd
	ReOpJump
)

type Inst struct {
	Op   ReOp
	Rune rune
	Out  int
	Out1 int
	Arg  int
}

type Thread struct {
	PC   int
	Caps []int
}

type Prog struct {
	Insts   []Inst
	Start   int
	NumCaps int
}

func NewProg(insts []Inst, start int) *Prog {
	return &Prog{
		Insts: insts,
		Start: start,
	}
}

func (p *Prog) addThread(list []Thread, t Thread, ix int) []Thread {
	// worklist is a stack of threads to process
	workList := []Thread{t}

	for len(workList) > 0 {
		t := workList[len(workList)-1]
		workList = workList[:len(workList)-1]

		inst := p.Insts[t.PC]
		switch inst.Op {
		case ReOpRune, ReOpRuneClass, ReOpAny, ReOpMatch:
			list = append(list, t)
		case ReOpSplit:
			// Push least preferred path first
			workList = append(workList, Thread{inst.Out1, t.Caps})
			// Push most preferred path second
			workList = append(workList, Thread{inst.Out, t.Caps})
		case ReOpJump:
			workList = append(workList, Thread{inst.Out, t.Caps})
		case ReOpCaptureStart:
			newCaps := make([]int, len(t.Caps))
			copy(newCaps, t.Caps)
			gid := p.NumCaps - inst.Arg
			newCaps[gid*2] = ix
			workList = append(workList, Thread{inst.Out, newCaps})
		case ReOpCaptureEnd:
			newCaps := make([]int, len(t.Caps))
			copy(newCaps, t.Caps)
			gid := p.NumCaps - inst.Arg
			newCaps[gid*2+1] = ix
			workList = append(workList, Thread{inst.Out, newCaps})
		}
	}

	return list
}

func (p *Prog) Match(s string) ([]int, bool) {
	capsLen := (p.NumCaps + 1) * 2 // +1 for group 0
	var clist []Thread
	var nlist []Thread

	initialCaps := make([]int, capsLen)
	initialCaps[0] = 0 // Group 0 starts at position 0
	clist = p.addThread(nil, Thread{p.Start, initialCaps}, 0)
	var ix int

	rr := []rune(s)
	for i, r := range rr {
		ix = i
		nlist = nil

		for _, t := range clist {
			inst := p.Insts[t.PC]
			switch inst.Op {
			case ReOpRune, ReOpRuneClass:
				if matchRune(r, inst) {
					nlist = p.addThread(nlist, Thread{inst.Out, t.Caps}, i+1)
				}
			case ReOpAny:
				nlist = p.addThread(nlist, Thread{inst.Out, t.Caps}, i+1)
			}
		}
		clist = nlist

		if len(clist) == 0 {
			// No match
			return nil, false
		}
	}
	ix = len(rr)

	for _, t := range clist {
		inst := p.Insts[t.PC]
		if inst.Op == ReOpMatch {
			// Complete the group 0 capture end
			finalCaps := make([]int, capsLen)
			copy(finalCaps, t.Caps)
			finalCaps[1] = ix
			return finalCaps, true
		}
	}

	return nil, false
}

func matchRune(r rune, inst Inst) bool {
	switch inst.Op {
	case ReOpRune:
		return r == inst.Rune
	case ReOpRuneClass:
		switch inst.Arg {
		case RuneClassDigit:
			return r >= '0' && r <= '9'
		case RuneClassWord:
			return (r >= 'a' && r <= 'z') || (r >= 'A' && r <= 'Z') || (r >= '0' && r <= '9') || r == '_'
		case RuneClassSpace:
			return r == ' ' || r == '\t' || r == '\n' || r == '\r' || r == '\f' || r == '\v'
		case RuneClassNotDigit:
			return !(r >= '0' && r <= '9')
		case RuneClassNotWord:
			return !((r >= 'a' && r <= 'z') || (r >= 'A' && r <= 'Z') || (r >= '0' && r <= '9') || r == '_')
		case RuneClassNotSpace:
			return !(r == ' ' || r == '\t' || r == '\n' || r == '\r' || r == '\f' || r == '\v')
		}
	}
	return false
}

func (p *Prog) String() string {
	var b strings.Builder
	for i, inst := range p.Insts {
		b.WriteString(fmt.Sprintf("%04d: ", i))
		switch inst.Op {
		case ReOpMatch:
			b.WriteString("MATCH\n")
		case ReOpRune:
			b.WriteString(fmt.Sprintf("RUNE '%c' -> %d\n", inst.Rune, inst.Out))
		case ReOpAny:
			b.WriteString(fmt.Sprintf("ANY -> %d\n", inst.Out))
		case ReOpSplit:
			b.WriteString(fmt.Sprintf("SPLIT -> %d, %d\n", inst.Out, inst.Out1))
		case ReOpCaptureStart:
			// Groups are numerated from the end
			gid := p.NumCaps - inst.Arg
			b.WriteString(fmt.Sprintf("CAPTURE_START (group %d) -> %d\n", gid, inst.Out))
		case ReOpCaptureEnd:
			// Groups are numerated from the end
			gid := p.NumCaps - inst.Arg
			b.WriteString(fmt.Sprintf("CAPTURE_END (group %d) -> %d\n", gid, inst.Out))
		case ReOpJump:
			b.WriteString(fmt.Sprintf("JUMP -> %d\n", inst.Out))
		}
	}
	return b.String()
}
