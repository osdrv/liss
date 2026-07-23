# Liss Programming Language

![Liss Logo](https://github.com/osdrv/liss-doc/raw/main/img/liss-motto.png?raw=true)

## About

Liss is an experimental functional programming language with a Lisp-style S-expression syntax. It is a one-pass compiled language that runs on its own direct-threaded virtual machine, written in C23.

This is the C implementation of Liss, built as a systems programming exercise. The earlier prototype lives at [osdrv/liss-go](https://github.com/osdrv/liss-go).

## Features

- **Lisp-style Syntax:** S-expressions with prefix notation.
- **Functional:** Single-time assignment, closures, first-class functions.
- **Tail Call Optimization:** Proper tail calls — deep recursion without stack overflow.
- **Persistent Data Structures:** Dicts backed by a Hash Array Mapped Trie (HAMT); lists via persistent cons cells.
- **Direct-threaded VM:** One-pass compiler emitting bytecode, executed by a direct-threaded interpreter.
- **Pattern Matching:** `switch` with structural destructuring.
- **Pipe Operator:** `->` threads a value left-to-right, short-circuiting on `err`.
- **Error Handling:** Value-level errors (`err` / `is_err?`) and stack-unwinding exceptions (`raise!` / `try`).
- **Regexp Support:** Built-in `re` module with a custom NFA-based regex engine.
- **Modules:** Native modules (`core`, `list`, `math`, `io`, `str`, `re`) and local Liss file imports.
- **REPL:** Interactive Read-Eval-Print Loop.
- **Mark-and-Sweep GC:** Incremental garbage collector with configurable heap growth.

## Building and Running

```sh
make              # build bin/liss
make run          # build and start REPL
make test         # build and run test suite
make clean        # remove build artifacts
```

Run a `.liss` file:

```sh
./bin/liss examples/fib.liss
```

Debug builds with AddressSanitizer:

```sh
make DEBUG=1 SANITIZE=1
```

## Examples

### Fibonacci

```lisp
(import io ["println"])

(fn fib [n]
    (cond (lt n 2) n (+ (fib (- n 1)) (fib (- n 2)))))

(println (fib 10))
```

### List Processing

```lisp
(import io ["println"])

(fn sum [lst]
    (fn loop [acc l]
        (cond (is_empty? l) acc
              (loop (+ acc (head l)) (tail l))))
    (loop 0 lst))

(println (sum [1 2 3 4 5]))
```

### Pattern Matching

```lisp
(import io ["println"])

(fn describe [n]
    (switch n
        [1  "one"]
        [2  "two"]
        [*  "many"]))

(println (describe 1))
(println (describe 42))
```

### Pipe Operator and Error Handling

```lisp
(import io ["println"])
(import str ["split" "parse_int"])

(switch (-> "42:hello:world" (str:split ":") (get 0) str:parse_int)
    [(err msg) (println "parse error:" msg)]
    [n         (println "got:" n)])
```

### Persistent Dict

```lisp
(import io ["println"])

(let d  (dict ("a" . 1) ("b" . 2)))
(let d2 (put d "c" 3))

(println (len d))   ; 2 — original unchanged
(println (len d2))  ; 3
(println (get d2 "c"))
```

### Regular Expressions

```lisp
(import io ["println"])
(import re ["match"])

(switch (re:match "^[0-9]+" "42abc")
    [(err msg) (println "no match:" msg)]
    [m         (println "matched:" m)])
```

## Language Reference

### Keywords

`fn` `let` `cond` `switch` `import` `try` `and` `or` `not`
`true` `false` `null` `eq` `ne` `lt` `lte` `gt` `gte`
`div` `mul` `mod` `band` `bor` `bxor` `bnot` `bsl` `bsr`
`as` `->`

### Core Functions

| Function | Description |
|---|---|
| `err msg` | Construct an error value |
| `is_err? v` | Test whether a value is an error |
| `raise! e` | Throw an error, unwind to nearest `try` |
| `len v` | Length of string, list, or dict |
| `is_empty? v` | True if string, list, or dict is empty |
| `get coll key` | Index into list, dict, or string |
| `pair a b` | Construct a dotted pair |
| `fst p` | First element of a pair |
| `snd p` | Second element of a pair |
| `dict pairs...` | Construct a persistent dict from pairs |
| `put d k v` | Return new dict with key added/updated |
| `del d k` | Return new dict with key removed |
| `has? d k` | True if key exists in dict |
| `keys d` | List of dict keys |
| `values d` | List of dict values |
| `head lst` | First element of list |
| `tail lst` | Rest of list as a new list |
| `cons lst elem` | Prepend element, return new list |
| `push lst elem` | Append element, return new list |
| `append lst1 lst2` | Concatenate two lists |
| `sort lst` | Sort a list of ints, reals, or strings in natural ascending order |
| `sort_by lst cmp` | Sort with a custom comparator — `cmp` returns true if its first arg comes before its second |
| `str v` | Convert any value to its string representation |
| `to_int v` | Convert int or real to int (truncates toward zero) |
| `to_real v` | Convert int or real to real |
| `str:parse_int s` | Parse a string as an integer — returns `err` on failure |
| `str:parse_real s` | Parse a string as a real — returns `err` on failure |
| `inspect v` | Return a string describing the type and value — useful for debugging |

## References

- "Compilers: Principles, Techniques, and Tools" by Aho, Lam, Sethi and Ullman, 2006 (ISBN: 978-0321486813)
- "Ideal Hash Trees" by Phil Bagwell, 2001
- "Regular Expression Matching Can Be Simple And Fast" by Russ Cox, 2007
- "Regular Expression Matching: the Virtual Machine Approach" by Russ Cox, 2009
- "Regular Expression Matching in the Wild" by Russ Cox, 2010
- "Advanced Data Structures" by Peter Brass, 2019 (ISBN: 978-1108735513)
- "Structure and Interpretation of Computer Programs" by Abelson and Sussman, 1996

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.

## Copyright

Copyright (c) 2026 Oleg Sidorov
