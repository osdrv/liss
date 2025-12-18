# Liss Programming Language

![Liss Logo](https://github.com/osdrv/liss-doc/blob/main/img/liss-motto.png?raw=true)

## About

Liss is an educational functional programming language with a Lisp-inspired syntax. It is a compiled language that runs on its own virtual machine.

The current VM implementation is based on Thorsten Ball's book "Writing a Compiler in Go" [[link]](https://compilerbook.com).

## Features

*   **Lisp-inspired Syntax:** Uses S-expressions for a simple and consistent syntax.
*   **Functional:** Emphasizes a functional programming style.
*   **Compiler and VM:** Compiles to bytecode that runs on a custom virtual machine.
*   **Tail Call Optimization**: A proper handling for deep recursive calls.
*   **REPL:** Includes an interactive Read-Eval-Print Loop for experimentation.
*   **Regexp Support:** Built-in support for regular expressions (Thompson's construction).
*   **Closures:** Supports first-class functions and closures.
*   **Modules**: Std lib and local LISS code imports.

## Examples

Here are some examples demonstrating key features of the Liss programming language:

### Example 1: Palindrome Checker (String Manipulation)

This example showcases basic string manipulation (`strings:reverse`) and conditional logic to check if a word is a palindrome.

<pre>
(<B>import</B> <I>"strings"</I> [<I>"reverse"</I>])

(<B>fn</B> is_palindrome? [word]
    (<B>cond</B> (<B>or</B> (<B>is_null?</B> word) (<B>is_empty?</B> word))
        <B>false</B>
        (= word (<B>strings:reverse</B> word))
    )
)

(<B>println</B> <I>"'hello' is a palindrome? "</I> (is_palindrome? <I>"hello"</I>))
(<B>println</B> <I>"'racecar' is a palindrome? "</I> (is_palindrome? <I>"racecar"</I>))
</pre>

### Example 2: Data Processing Pipeline (List & String stdlib)

This example demonstrates a common data processing task: splitting a sentence, filtering words by length and rejoining. It highlights the functional style and the use of the `list` and `strings` standard library modules.

<pre>
(<B>import</B> <I>"list"</I> [<I>"filter"</I>])
(<B>import</B> <I>"strings"</I> [<I>"split"</I> <I>"join"</I>])

(<B>let</B> sentence <I>"Liss is a fun and powerful language"</I>)

(<B>let</B> long_words (<B>list:filter</B> (<B>strings:split</B> sentence <I>" "</I>) (<B>fn</B> [word]
    (> (<B>len</B> word) 2)
)))

(<B>let</B> result (<B>strings:join</B> long_words <I>", "</I>))

(<B>println</B> <I>"Original: "</I> sentence)
(<B>println</B> <I>"Processed: "</I> result)
; Expected Output: "Processed: Liss, fun, and, powerful, language"
</pre>

### Example 3: Summing a Long List (Tail Recursion)

This example demonstrates a tail-recursive function to sum a list of numbers. This implementation is optimized for memory efficiency, preventing stack overflows even with very large lists, showcasing a key feature of Liss's VM.

<pre>
(<B>import</B> <I>"list"</I> [<I>"seq"</I>])

; A tail-recursive function to sum a list
(<B>fn</B> sum_list [lst]
    (<B>fn</B> _sum [acc l]
        (<B>cond</B> (<B>is_empty?</B> l)
            acc
            (_sum (+ acc (<B>head</B> l)) (<B>tail</B> l))
        )
    )
    (_sum 0 lst)
)

; Create a list with 10,000 numbers from 1 to 10,000
(<B>let</B> numbers (<B>list:seq</B> 1 10001))

(<B>println</B> <I>"Summing 10,000 numbers..."</I>)
(<B>let</B> total (sum_list numbers))
(<B>println</B> <I>"Total: "</I> total)
; Expected Output: "Total: 50005000"
</pre>



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
