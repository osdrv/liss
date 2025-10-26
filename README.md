# LISS Programming Language

![LISS Logo](https://github.com/osdrv/liss-doc/blob/main/img/liss-1.png?raw=true)

Liss is an educational functional programming language with a Lisp-inspired syntax. It is a compiled language that runs on its own virtual machine.

## Features

*   **Lisp-like Syntax:** Uses S-expressions for a simple and consistent syntax.
*   **Functional:** Emphasizes a functional programming style.
*   **Compiler and VM:** Compiles to bytecode that runs on a custom virtual machine.
*   **REPL:** Includes an interactive Read-Eval-Print Loop for experimentation.
*   **Regexp Support:** Built-in support for regular expressions (Thompson's construction).
*  **Closures:** Supports first-class functions and closures.

## Example

Here is an example of a LISS program that computes the factorial of a number:

```liss
(fn fact [n]
  (cond (= n 0)
        1
        (* n (fact (- n 1)))
  )
)
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Copyright

Copyright (c) 2025 Oleg Sidorov
