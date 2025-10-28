package object

import "os"

type File struct {
	defaultObject
	fd   *os.File
	path string
}

var _ Object = (*File)(nil)

func NewFile(fd *os.File, path string) *File {
	return &File{
		fd:   fd,
		path: path,
	}
}

func FOpen(path string) (*File, error) {
	fd, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	return &File{
		fd:   fd,
		path: path,
	}, nil
}

func (f *File) IsFile() bool {
	return true
}

func (f *File) Close() error {
	return f.fd.Close()
}

func (f *File) String() string {
	return "file(" + f.path + ")"
}

func (f *File) Clone() Object {
	return f
}

func (f *File) Type() ObjectType {
	return FileType
}

func (f *File) Raw() any {
	return f.fd
}

func (f *File) Path() string {
	return f.path
}
