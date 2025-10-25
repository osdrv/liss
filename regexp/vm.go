package regexp

type ReOp uint8

const (
	ReOpMatch ReOp = iota
	ReOpRune
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

type VM struct {
	Insts []Inst
	Start int
}

func NewVM(insts []Inst, start int) *VM {
	return &VM{
		Insts: insts,
		Start: start,
	}
}

func (vm *VM) addThread(list []Thread, t Thread, ix int) []Thread {
	// worklist is a stack of threads to process
	workList := []Thread{t}

	for len(workList) > 0 {
		t := workList[len(workList)-1]
		workList = workList[:len(workList)-1]

		inst := vm.Insts[t.PC]
		switch inst.Op {
		case ReOpRune, ReOpAny, ReOpMatch:
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
			newCaps[inst.Arg*2] = ix
			workList = append(workList, Thread{inst.Out, newCaps})
		case ReOpCaptureEnd:
			newCaps := make([]int, len(t.Caps))
			copy(newCaps, t.Caps)
			newCaps[inst.Arg*2+1] = ix
			workList = append(workList, Thread{inst.Out, newCaps})
		}
	}

	return list
}

func (vm *VM) Match(s string, numCaps int) ([]int, bool) {
	capsLen := (numCaps + 1) * 2 // +1 for group 0
	var clist []Thread
	var nlist []Thread

	initialCaps := make([]int, capsLen)
	initialCaps[0] = 0 // Group 0 starts at position 0
	clist = vm.addThread(nil, Thread{vm.Start, initialCaps}, 0)
	var ix int

	for i, r := range s {
		ix = i
		nlist = nil

		for _, t := range clist {
			inst := vm.Insts[t.PC]
			switch inst.Op {
			case ReOpRune:
				if inst.Rune == r {
					nlist = vm.addThread(nlist, Thread{inst.Out, t.Caps}, i+1)
				}
			case ReOpAny:
				nlist = vm.addThread(nlist, Thread{inst.Out, t.Caps}, i+1)
			}
		}
		clist = nlist

		if len(clist) == 0 {
			// No match
			return nil, false
		}
	}
	ix = len(s)

	for _, t := range clist {
		inst := vm.Insts[t.PC]
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
