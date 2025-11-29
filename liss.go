package main

import (
	"flag"
	"fmt"
	"io"
	"os"
	"osdrv/liss/ast"
	"osdrv/liss/code"
	"osdrv/liss/compiler"
	"osdrv/liss/lexer"
	"osdrv/liss/module_loader"
	"osdrv/liss/object"
	"osdrv/liss/parser"
	"osdrv/liss/repl"
	"osdrv/liss/vm"
	"path"
	"path/filepath"
	"runtime/pprof"
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
	vminst := vm.New(mod).WithOptions(vmopts)

	defer vminst.Shutdown()
	if err := vminst.Run(); err != nil {
		return nil, fmt.Errorf("Runtime error: %w", err)
	}
	top := vminst.LastPopped()
	if opts.Debug {
		fmt.Printf("VM stack: %s\n", vminst.PrintStack())
	}
	return top, nil
}

// CompileAll compiles the given source code along with its imports using the provided module loader.
func CompileAll(src string, loader module_loader.Loader, opts repl.Options) (*compiler.Module, error) {
	mod := compiler.NewModule(src, loader.DotPath())

	lex := lexer.NewLexer(mod.Src)
	par := parser.NewParser(lex)
	prog, err := par.Parse()
	if err != nil {
		return nil, fmt.Errorf("failed to parse program: %w", err)
	}

	if opts.Debug && opts.Verbose {
		fmt.Printf("AST:\n%s\n", prog.String())
	}

	symbols := object.NewSymbolTable()
	consts := []object.Object{}

	c := compiler.NewWithState(compiler.CompilerOptions{
		Debug: opts.Debug,
	}, symbols, consts)

	imports, err := findImports(prog)
	if err != nil {
		return nil, fmt.Errorf("failed to resolve module imports: %w", err)
	}

	for alias, ie := range imports {
		if opts.Debug {
			fmt.Printf("Compiling imported module %s(%s)\n", ie.ref, alias)
		}
		// TODO: implement circular import detection
		path, err := loader.Resolve(ie.ref)
		if err != nil {
			return nil, fmt.Errorf("failed to resolve imported module %s: %w", ie.ref, err)
		}
		src, err := loader.Load(path)
		if err != nil {
			return nil, fmt.Errorf("failed to load imported module %s: %w", ie.ref, err)
		}

		loader.PushDotPath(path)
		impmod, err := CompileAll(string(src), loader, opts)
		impmod.Path = path
		if err != nil {
			return nil, fmt.Errorf("failed to compile imported module %s: %w", ie.ref, err)
		}
		loader.PopDotPath()

		if err := InstantiateModule(impmod); err != nil {
			return nil, fmt.Errorf("failed to instantiate imported module %s: %w", impmod.Name, err)
		}
		if _, err := c.ImportModule(alias, impmod, ie.symbols); err != nil {
			return nil, fmt.Errorf("failed to import module %s: %w", impmod.Name, err)
		}
	}

	if err := c.Compile(prog); err != nil {
		return nil, fmt.Errorf("failed to compile program: %w", err)
	}

	mod.Bytecode = c.Bytecode()
	mod.Symbols = c.Symbols()
	mod.Env.Consts = c.Bytecode().Consts
	// TODO: reconsider this flag: I am not using it anywhere yet
	mod.MarkInitialized()

	return mod, nil
}

type importEntry struct {
	ref     string
	symbols []string
}

func findImports(prog ast.Node) (map[string]importEntry, error) {
	imps := make(map[string]importEntry)
	impExprs := ast.NewTreeWalker(prog).CollectNodes(func(node *ast.Node) bool {
		_, ok := (*node).(*ast.ImportExpression)
		return ok
	})
	for _, imp := range impExprs {
		impExpr := imp.(*ast.ImportExpression)
		refLit, ok := impExpr.Ref.(*ast.StringLiteral)
		if !ok {
			return nil, fmt.Errorf("import reference is not a string literal: %s", impExpr.Ref.String())
		}
		ref := refLit.Value
		var alias string
		if impExpr.Alias != nil {
			alias = impExpr.Alias.Name
		} else {
			alias = getModuleNameFromRef(ref)
		}
		if _, ok := imps[alias]; ok {
			return nil, fmt.Errorf("duplicate import alias: %s", alias)
		}
		var symbols []string
		if impExpr.Symbols != nil {
			for _, sym := range impExpr.Symbols.Items {
				symLit, ok := sym.(*ast.StringLiteral)
				if !ok {
					return nil, fmt.Errorf("import symbol is not a string literal: %s", sym.String())
				}
				symbols = append(symbols, symLit.Value)
			}
		}
		imps[alias] = importEntry{
			ref:     ref,
			symbols: symbols,
		}
	}
	return imps, nil
}

func InstantiateModule(mod *compiler.Module) error {
	vm := vm.New(mod)
	err := vm.Run()
	if err != nil {
		return err
	}
	mod.Env = vm.Env()
	return nil
}

func Execute(src string, loader module_loader.Loader, opts repl.Options) (Result, error) {
	mod, err := CompileAll(src, loader, opts)
	if err != nil {
		return nil, err
	}
	if opts.Verbose {
		printBytecode(mod.Bytecode)
	}
	return Run(mod, opts)
}

func ExecutePath(srcPath string, loader module_loader.Loader, opts repl.Options) (Result, error) {
	dotPath, err := filepath.Abs(path.Dir(srcPath))
	if err != nil {
		return nil, fmt.Errorf("failed to determine current path: %w", err)
	}
	loader.PushDotPath(dotPath)
	defer loader.PopDotPath()

	f, err := os.Open(srcPath)
	defer f.Close()
	if err != nil {
		return nil, fmt.Errorf("failed to open source file %s: %w", srcPath, err)
	}

	src, err := io.ReadAll(f)
	if err != nil {
		return nil, fmt.Errorf("failed to read source file %s: %w", srcPath, err)
	}

	return Execute(string(src), loader, opts)
}

func getModuleNameFromRef(ref string) string {
	base := path.Base(ref)
	ext := path.Ext(base)
	return strings.TrimSuffix(base, ext)
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

	exp, err := os.Executable()
	if err != nil {
		fmt.Fprintf(er, "Error getting executable path: %v\n", err)
		os.Exit(1)
	}
	// Drop the executable name to get the directory
	exp = filepath.Dir(exp)

	debug := flag.Bool("debug", false, "Enables debug mode with stack dumps and traces")
	verbose := flag.Bool("v", false, "Enables verbose output")
	prof := flag.Bool("pprof", false, "Enables CPU profiling to cpu_profile.prof")
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

	f, err := os.Create("cpu_profile.prof")
	if err != nil {
		fmt.Fprintf(er, "Error creating CPU profile file: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	loader := module_loader.New(exp, module_loader.Options{})

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

	if *prof {
		if err := pprof.StartCPUProfile(f); err != nil {
			fmt.Fprintf(er, "Error starting CPU profile: %v\n", err)
			os.Exit(1)
		}
	}
	if _, err := ExecutePath(*src, loader, opts); err != nil {
		fmt.Fprintf(er, "Error executing file %s: %v\n", *src, err)
		os.Exit(1)
	}
	if *prof {
		pprof.StopCPUProfile()
	}
	os.Exit(0)
}
