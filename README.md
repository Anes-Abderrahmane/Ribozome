# Ribozome

Ribozome is a custom multi-stage compiler written in portable C. It translates a biologically-themed high-level language (`.dna`) through an intermediate RNA-inspired assembly (`.rna`) and finally into standard x86_64 NASM assembly (`.asm`) before producing a Linux ELF64 executable (`.protein`).

## Architecture

Ribozome is built as a minimal compiler pipeline with clear, separate phases:

1. **Lexer** (`lexer.c`, `lexer.h`)
   - Reads `.dna` source text
   - Produces tokens for keywords, identifiers, literals, operators, and punctuation

2. **Parser** (`parser.c`, `parser.h`)
   - Builds an Abstract Syntax Tree (AST) from the token stream
   - Supports function definitions, the `NUCLEUS` entry point, variable declarations, expressions, conditionals, loops, `EXPRESS`, and `SECRETE`

3. **Code Generation** (`codegen.c`, `codegen.h`)
   - Traverses the AST to emit `output.rna`
   - Emits `CHROMOSOME_CODING` for static data and variables
   - Emits `CHROMOSOME_EXPRESSION` for logic using RNA keywords

4. **Translation** (`translator.c`)
   - Reads `output.rna`
   - Replaces biological instruction names with NASM equivalents
   - Writes `output.asm`

5. **Build / Catalysis** (`main.c`)
   - Drives the full pipeline
   - Runs `nasm` and `ld` using `system()` to produce `output.protein`

## Language Overview

The source language is intentionally small and biology-inspired. It maps directly to primitive assembly operations.

### High-level keywords

- `NUCLEUS` — program entry point (main)
- `GENE` — function definition
- `NUCLEOTIDE` — numeric variable type
- `SEQUENCE` — string literal type
- `EXPRESS` — print to stdout
- `SECRETE` — return / exit
- `IF_HOMOLOGOUS` / `ELSE_MUTATE` — conditional branching
- `REPLICATE_WHILE` / `REPLICATE` — while loop

### RNA keywords

The intermediate `.rna` format uses exact RNA-style instructions:

- `PHOSPHORYLATE` → `mov`
- `POLYMERIZE` → `add`
- `HYDROLYZE` → `sub`
- `ANNEAL` → `cmp`
- `TRANSLOCATE` → `jmp`
- `BIND_MATCH` → `je`
- `EXOCYTOSE` → `syscall`

Registers are also mapped:

- `KINASE_A` → `rax`
- `RECEPTOR_D` → `rdi`
- `RECEPTOR_S` → `rsi`
- `RECEPTOR_X` → `rdx`

## Build

To compile the compiler and translator:

    make

This produces two executables:

- `main` — the compiler driver
- `translator` — converts `output.rna` into `output.asm`

## Usage

Compile a `.dna` source file and build the final executable in one step:

    ./main source.dna

This runs the full pipeline and writes:

- `output.rna`
- `output.asm`
- `output.o`
- `output.protein`

Options:

- `-v`, `--verbose` — print the token stream and AST details during compilation
- `-h`, `--help` — show the usage message

You can also separately translate RNA to ASM:

    ./translator output.rna output.asm

A `NUCLEUS` entry point that does not end with `SECRETE` automatically gets a
default exit (`exit 0`) appended by the code generator, so simple programs do
not need an explicit return to avoid crashing.

## Known Limitations

- `EXPRESS` only prints string literals (and string variables) correctly.
  Printing a numeric expression currently emits raw register bytes rather than
  a formatted integer.
- String escape sequences such as `\n` and `\t` are decoded into the output
  data; everything printed via `EXPRESS` is also terminated with a newline.

## Example

A minimal `.dna` program:

```dna
NUCLEUS {
    NUCLEOTIDE x = 42;
    EXPRESS "Hello";
}
```

The generated `output.rna` contains a data section for the string and a text section for the entry point. `translator` then maps the RNA-style instructions to NASM.

## File Layout

- `lexer.h`, `lexer.c` — tokenization phase
- `parser.h`, `parser.c` — parser and AST builder
- `codegen.h`, `codegen.c` — RNA code generation
- `translator.c` — RNA-to-ASM translator
- `main.c` — front-end driver and build orchestration
- `Makefile` — build rules

## Notes

Ribozome is designed for readability and easy extension. The current implementation is a prototype that demonstrates the full compilation path from a custom high-level syntax to a Linux executable.
