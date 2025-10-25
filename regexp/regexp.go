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

func (r *Regexp) MatchString(s string) ([]string, bool) {
	caps, ok := r.Prog.Match(s)
	if !ok {
		return nil, false
	}
	matches := make([]string, len(caps)/2)
	for i := 0; i < len(caps); i += 2 {
		start := caps[i]
		end := caps[i+1]
		if start >= 0 && end >= 0 && start <= end && end <= len(s) {
			matches[i/2] = s[start:end]
		} else {
			matches[i/2] = ""
		}
	}
	return matches, true
}
