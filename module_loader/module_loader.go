package module_loader

import (
	"fmt"
	"os"
	"path"
	"path/filepath"
	"strings"
)

const (
	LissModuleExt = ".liss"
)

type Loader interface {
	DotPath() string
	PushDotPath(dotPath string)
	PopDotPath()
	Load(path string) ([]byte, error)
	Resolve(pathOrName string) (string, error)
}

type Options struct{}

type LoaderImp struct {
	opts  Options
	paths []string
}

var _ Loader = (*LoaderImp)(nil)

func New(rootPath string, opts Options) *LoaderImp {
	return &LoaderImp{
		opts:  opts,
		paths: []string{rootPath},
	}
}

func (l *LoaderImp) RootPath() string {
	return l.paths[0]
}

func (l *LoaderImp) DotPath() string {
	return l.paths[len(l.paths)-1]
}

func (l *LoaderImp) PushDotPath(path string) {
	if strings.HasSuffix(path, LissModuleExt) {
		path = filepath.Dir(path)
	}
	l.paths = append(l.paths, path)
}

func (l *LoaderImp) PopDotPath() {
	if len(l.paths) <= 1 {
		panic("cannot pop dot path: reached the root")
	}
	l.paths = l.paths[:len(l.paths)-1]
}

func (l *LoaderImp) Load(path string) ([]byte, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("failed to read module file %s: %w", path, err)
	}
	return data, nil
}

func (l *LoaderImp) Resolve(pathOrName string) (string, error) {
	var resolved string
	// If the path looks like a standard module name, search in ./std/{import_path}.liss
	if looksLikeStdModule(pathOrName) {
		resolved = path.Join(l.RootPath(), "std", pathOrName+".liss")
	} else if path.IsAbs(pathOrName) {
		return pathOrName, nil
	} else {
		// Otherwise, treat it as a file path relative to the dotPath
		resolved = path.Join(l.DotPath(), pathOrName)
		return filepath.Clean(resolved), nil
	}
	return resolved, nil
}

func looksLikeStdModule(path string) bool {
	return !strings.Contains(path, "/") &&
		!strings.Contains(path, "\\") &&
		!strings.HasPrefix(path, ".") &&
		!strings.HasSuffix(path, ".liss")
}

func getModuleNameFromPath(p string) string {
	bp := path.Base(p)
	return strings.TrimSuffix(bp, path.Ext(bp))
}
