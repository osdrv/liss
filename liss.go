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
	"path/filepath"
	"slices"
	"strings"
)

type Result interface {
	String() string
}

func Run(mod *compiler.Module, opts repl.Options) (Result, error) {
	var vmopts vm.VMOptions
	if opts.Debug {
		vmopts.Debug = 1
		if opts.Verbose {
			vmopts.Debug++
		}
	}
	vminst := vm.New(mod.Bytecode, mod).WithOptions(vmopts)

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

func Compile(src string, opts repl.Options) (*compiler.Module, error) {
	lex := lexer.NewLexer(src)
	par := parser.NewParser(lex)
	prog, err := par.Parse()
	if err != nil {
		return nil, fmt.Errorf("failed to parse program: %w", err)
	}

	if opts.Debug {
		fmt.Printf("AST:\n%s\n", prog.String())
	}

	c := compiler.New(compiler.CompilerOptions{
		Debug: opts.Debug,
	})
	if err := c.Compile(prog); err != nil {
		return nil, fmt.Errorf("failed to compile program: %w", err)
	}
	mod := &compiler.Module{
		Name:     "main",
		Bytecode: c.Bytecode(),
		Symbols:  c.Symbols(),
		Env: &object.Environment{
			Consts: c.Bytecode().Consts,
		},
	}
	return mod, nil
}

func Execute(src string, opts repl.Options) (Result, error) {
	mod, err := Compile(src, opts)
	if err != nil {
		return nil, err
	}
	if opts.Verbose {
		printBytecode(mod.Bytecode)
	}
	return Run(mod, opts)
}

func InstantiateModule(mod *compiler.Module) (*object.Environment, error) {
	vm := vm.New(mod.Bytecode, mod)
	err := vm.Run()
	if err != nil {
		return nil, err
	}
	return vm.Env(), nil
}

func CompileModule(dotPath string, nameOrPath string, opts repl.Options,
	cache map[string]*compiler.Module, chain []string) (*compiler.Module, error) {
	resolved, err := resolveModulePath(dotPath, nameOrPath)
	if err != nil {
		return nil, fmt.Errorf("failed to resolve module path for %s: %w", nameOrPath, err)
	}
	fullPath, err := filepath.Abs(resolved)
	if err != nil {
		return nil, fmt.Errorf("failed to get absolute path for module %s: %w", nameOrPath, err)
	}
	if err != nil {
		return nil, fmt.Errorf("failed to resolve module path: %w", err)
	}
	name := getModuleNameFromPath(nameOrPath)

	if mod, ok := cache[resolved]; ok {
		return mod, nil
	}

	if slices.Contains(chain, name) {
		return nil, fmt.Errorf("circular module dependency detected: %s -> %s",
			strings.Join(append(chain, name), " -> "), name)
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

	mod := &compiler.Module{
		Name:    name,
		Path:    fullPath,
		DotPath: path.Dir(fullPath),
		Symbols: object.NewSymbolTable(),
	}

	imports := ast.NewTreeWalker(prog).CollectNodes(func(node *ast.Node) bool {
		_, ok := (*node).(*ast.ImportExpression)
		return ok
	})
	c := compiler.NewWithState(compiler.CompilerOptions{
		Debug: opts.Debug,
	}, mod.Symbols, []object.Object{})
	for _, imp := range imports {
		impExpr := imp.(*ast.ImportExpression)
		var wantSymbols []string
		if impExpr.Symbols != nil {
			for _, sym := range impExpr.Symbols.Items {
				wantSymbols = append(wantSymbols, sym.(*ast.StringLiteral).Value)
			}
		}
		ref := impExpr.Ref.(*ast.StringLiteral).Value
		if opts.Debug {
			fmt.Printf("Compiling imported module %s for module %s\n", ref, name)
		}
		impmod, err := CompileModule(mod.DotPath, ref, opts, cache, append(chain, name))
		if err != nil {
			return nil, fmt.Errorf("failed to compile imported module %s: %w",
				impExpr.Ref.(*ast.StringLiteral).Value, err)
		}
		env, err := InstantiateModule(impmod)
		if err != nil {
			return nil, fmt.Errorf("failed to instantiate imported module %s: %w",
				impmod.Name, err)
		}
		impmod.Env = env
		if _, err := c.ImportModule(ref, impmod, wantSymbols); err != nil {
			return nil, fmt.Errorf("failed to import module %s: %w", impmod.Name, err)
		}
	}

	if err := c.Compile(prog); err != nil {
		return nil, fmt.Errorf("failed to compile module %s: %w", name, err)
	}

	mod.Bytecode = c.Bytecode()
	mod.Symbols = c.Symbols()

	cache[resolved] = mod

	return mod, nil
}

func ExecutePath(srcPath string, opts repl.Options) (Result, error) {
	cache := make(map[string]*compiler.Module)
	dotPath := path.Clean(".")
	mod, err := CompileModule(dotPath, srcPath, opts, cache, nil)
	if err != nil {
		return nil, err
	}
	if opts.Verbose {
		printBytecode(mod.Bytecode)
	}
	return Run(mod, opts)
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

func resolveModulePath(dotPath string, p string) (string, error) {
	var resolved string
	// If the path looks like a standard module name, search in ./std/{import_path}.liss
	if looksLikeStdModule(p) {
		resolved = path.Join(".", "std", p+".liss")
	} else if path.IsAbs(p) {
		return p, nil
	} else {
		// Otherwise, treat it as a file path relative to the dotPath
		resolved = path.Join(dotPath, p)
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
