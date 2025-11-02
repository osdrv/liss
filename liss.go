package main

import (
	"flag"
	"fmt"
	"os"
	"osdrv/liss/ast"
	"osdrv/liss/code"
	"osdrv/liss/compiler"
	"osdrv/liss/lexer"
	"osdrv/liss/object"
	"osdrv/liss/parser"
	"osdrv/liss/repl"
	"osdrv/liss/vm"
	"path"
	"strings"
)

type Result interface {
	String() string
}

func Run(bc *compiler.Bytecode, opts repl.Options) (Result, error) {
	var vmopts vm.VMOptions
	if opts.Debug {
		vmopts.Debug = 1
		if opts.Verbose {
			vmopts.Debug++
		}
	}
	vminst := vm.New(bc).WithOptions(vmopts)

	defer vminst.Shutdown()
	if err := vminst.Run(); err != nil {
		return nil, fmt.Errorf("failed to run bytecode: %w", err)
	}
	top := vminst.LastPopped()
	if opts.Debug {
		fmt.Printf("VM stack: %s\n", vminst.PrintStack())
	}
	return top, nil
}

func Compile(src string, opts repl.Options) (*compiler.Bytecode, error) {
	lex := lexer.NewLexer(src)
	par := parser.NewParser(lex)
	prog, err := par.Parse()
	if err != nil {
		return nil, fmt.Errorf("failed to parse program: %w", err)
	}

	if opts.Debug {
		fmt.Printf("AST:\n%s\n", prog.String())
	}

	c := compiler.New()
	if err := c.Compile(prog); err != nil {
		return nil, fmt.Errorf("failed to compile program: %w", err)
	}
	return c.Bytecode(), nil
}

func Execute(src string, opts repl.Options) (Result, error) {
	bytecode, err := Compile(src, opts)
	if err != nil {
		return nil, err
	}
	if opts.Verbose {
		printBytecode(bytecode)
	}
	return Run(bytecode, opts)
}

func CompileModule(nameOrPath string, opts repl.Options, cache map[string]*compiler.Module) (*compiler.Module, error) {
	resolved, err := resolveModulePath(nameOrPath)
	if err != nil {
		return nil, fmt.Errorf("failed to resolve module path: %w", err)
	}
	name := path.Base(resolved)

	if mod, ok := cache[resolved]; ok {
		return mod, nil
	}

	// try to read the resolved path
	data, err := os.ReadFile(resolved)
	if err != nil {
		return nil, fmt.Errorf("failed to read module file %s: %w", resolved, err)
	}

	lex := lexer.NewLexer(string(data))
	par := parser.NewParser(lex)
	prog, err := par.Parse()
	if err != nil {
		return nil, fmt.Errorf("failed to parse module %s: %w", name, err)
	}

	imports := ast.NewTreeWalker(prog).CollectNodes(func(node *ast.Node) bool {
		_, ok := (*node).(*ast.ImportExpression)
		return ok
	})
	if opts.Debug {
		fmt.Printf("Module %s imports: %+v\n", name, imports)
	}

	c := compiler.New()
	if err := c.Compile(prog); err != nil {
		return nil, fmt.Errorf("failed to compile module %s: %w", name, err)
	}

	mod := &compiler.Module{
		Name:     name,
		Path:     resolved,
		Bytecode: c.Bytecode(),
		Symbols:  c.Symbols(),
	}
	return mod, nil
}

func ExecutePath(srcPath string, opts repl.Options) (Result, error) {
	cache := make(map[string]*compiler.Module)
	mod, err := CompileModule(srcPath, opts, cache)
	if err != nil {
		return nil, err
	}
	if opts.Verbose {
		printBytecode(mod.Bytecode)
	}
	return Run(mod.Bytecode, opts)
}

func printBytecode(bc *compiler.Bytecode) {
	shift := func(s string, pref string) string {
		var b strings.Builder
		for line := range strings.SplitSeq(s, "\n") {
			if line != "" {
				b.WriteString(pref)
			}
			b.WriteString(line)
			b.WriteString("\n")
		}
		return b.String()
	}

	fmt.Printf("Constants:\n")
	for i, cnst := range bc.Consts {
		switch cnst := cnst.(type) {
		case *object.Function:
			fmt.Printf("%04d: %s\n", i, cnst.String())
			fmt.Printf("  NumLocals: %d\n", cnst.NumLocals)
			fmt.Printf("  Instructions:\n%s\n", shift(code.PrintInstr(cnst.Instrs), "    "))
		default:
			fmt.Printf("%04d: %s\n", i, cnst.String())
		}
	}
	fmt.Print("\n")
	fmt.Printf("Instructions:\n%s\n", shift(code.PrintInstr(bc.Instrs), "  "))
}

func main() {
	out := os.Stdout
	er := os.Stderr
	defer out.Close()
	defer er.Close()

	debug := flag.Bool("debug", false, "Enables debug mode with stack dumps and traces")
	verbose := flag.Bool("v", false, "Enables verbose output")
	src := flag.String("src", "", "Source file to execute")
	exec := flag.String("exec", "", "Execute source code and exit")
	flag.Parse()
	opts := repl.Options{
		Debug:       *debug,
		Interactive: *exec == "",
		Verbose:     *verbose,
	}
	if opts.Debug {
		fmt.Printf("Runtime flags: %+v\n", opts)
	}

	if *exec != "" {
		opts.Interactive = false
		in := strings.NewReader(strings.TrimSpace(*exec) + "\n")
		repl.Run(in, out, opts)
		os.Exit(0)
	}

	if *src == "" {
		in := os.Stdin
		defer in.Close()
		repl.Run(in, out, opts)
		os.Exit(0)
	}

	if _, err := ExecutePath(*src, opts); err != nil {
		fmt.Fprintf(er, "Error executing file %s: %v\n", *src, err)
		os.Exit(1)
	}

	os.Exit(0)
}

func ImportModule(base, imp *compiler.Module, modName string, wantSymbols []string) error {
	st := base.Symbols
	for _, exp := range imp.Symbols.Export(wantSymbols) {
		fname := fmt.Sprintf("%s:%s", modName, exp.Name)
		if _, err := st.Define(fname); err != nil {
			return fmt.Errorf("failed to define imported symbol %s: %w", fname, err)
		}
	}
	return nil
}

func resolveModulePath(p string) (string, error) {
	var resolved string
	// If the path looks like a standard module name, search in ./std/{import_path}.liss
	if looksLikeStdModule(p) {
		resolved = path.Join(".", "std", p+".liss")
	} else {
		// Otherwise, treat it as a file path
		resolved = p
	}
	return resolved, nil
}

func looksLikeStdModule(path string) bool {
	return !strings.Contains(path, "/") && !strings.Contains(path, "\\") && !strings.HasPrefix(path, ".")
}
