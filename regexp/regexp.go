package regexp

type Regexp struct {
	Prog *Prog
}

func Compile(pattern string) (*Regexp, error) {
	p := &parser{input: []rune(pattern)}
	node, err := p.parseAlternate()
	if err != nil {
		return nil, err
	}

	c := &compiler{prog: &Prog{}}
	match := c.addInst(Inst{Op: ReOpMatch})
	start := c.compile(node, match)
	c.prog.Start = start
	c.prog.NumCaps = c.nCaps

	return &Regexp{
		Prog: c.prog,
	}, nil
}

func (r *Regexp) MatchStringIx(s string) ([][]int, bool) {
	rr := []rune(s)
	caps, ok := r.Prog.Match(s)
	if !ok {
		return nil, false
	}
	numGroups := len(caps) / 2
	matches := make([][]int, numGroups)
	for i := range numGroups {
		start := caps[2*i]
		end := caps[2*i+1]
		if start >= 0 && end >= 0 && start <= end && end <= len(rr) {
			matches[i] = []int{start, end}
		} else {
			matches[i] = []int{-1, -1}
		}
	}
	return matches, true
}

func (r *Regexp) MatchString(s string) ([]string, bool) {
	idxs, ok := r.MatchStringIx(s)
	if !ok {
		return nil, false
	}
	rr := []rune(s)
	matches := make([]string, len(idxs))
	for i, pair := range idxs {
		start := pair[0]
		end := pair[1]
		if start >= 0 && end >= 0 && start <= end && end <= len(rr) {
			matches[i] = string(rr[start:end])
		} else {
			matches[i] = ""
		}
	}
	return matches, true
}
