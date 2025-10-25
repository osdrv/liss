package regexp

type Regexp struct{}

func Compile(pattern string) (*Regexp, error) {
	panic("unimplemented")
}

func (r *Regexp) MatchString(s string) ([]string, bool) {
	panic("unimplemented")
}
