# LISS Programming Language

![LISS Logo](https://github.com/osdrv/liss-doc/blob/main/img/liss-1.png?raw=true)

Liss is an educational functional programming language with a Lisp-inspired syntax. It is a compiled language that runs on its own virtual machine.

The current VM implementation is based on Thorsten Ball's book "Writing a Compiler in Go" [[link]](https://compilerbook.com).

## Features

*   **Lisp-like Syntax:** Uses S-expressions for a simple and consistent syntax.
*   **Functional:** Emphasizes a functional programming style.
*   **Compiler and VM:** Compiles to bytecode that runs on a custom virtual machine.
*   **Tail Call Optimisation**: A proper handling for deep recursive calls.
*   **REPL:** Includes an interactive Read-Eval-Print Loop for experimentation.
*   **Regexp Support:** Built-in support for regular expressions (Thompson's construction).
*   **Closures:** Supports first-class functions and closures.
*   **Modules**: Std lib and local LISS code imports.

## Example

Here is an example of a LISS program that computes the factorial of a number:

```liss
(fn fact [n]
  (cond (= n 0)
        1
        (* n (fact (- n 1)))
  )
)
( println "the result is: " (fact 30))
```

## References

* "Writing a Compiler in Go" by Thorsten Ball, 2020 (ISBN: 978-3982016108)
* "Crafting Interpreters" by Robert Nystrom, 2021 (ISBN: 978-0990582939)
* "Compilers: Principles, Techniques, and Tools" by Aho, Lam, Sethi and Ullman, 2013 (ISBN: 978-1292024349)
* "Structure and Interpretation of Computer Programs" by Abelson and Sussman, 1996 (ISBN: 978-0262510875)
* "Advanced Data Structures" by Peter Brass, 2019 (ISBN: 978-1108735513)
* "Regular Expression Matching Can Be Simple And Fast" by Russ Cox, 2007 [[link]](https://swtch.com/~rsc/regexp/regexp1.html)
* "Regular Expression Matching: the Virtual Machine Approach" by Russ Cox, 2009 [[link]](https://swtch.com/~rsc/regexp/regexp2.html)
* "Regular Expression Matching in the Wild" by Russ Cox, 2010 [[link]](https://swtch.com/~rsc/regexp/regexp3.html)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Copyright

Copyright (c) 2025 Oleg Sidorov
