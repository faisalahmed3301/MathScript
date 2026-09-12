# MathScript

A small, self-defined compiler for a mathematical language, built with
**Flex** (lexer) + **Bison** (parser) + **GCC/Clang** (backend and
executable-code generation), in plain C. Written for CSE 303 --
Compiler Design (Presentation 3: implementation, executable
generation, testing, and demonstration).

MathScript source files use the extension **`.ms`** (see `examples/`).

```
> /calc
[CALC mode activated]
calc> 2 + 3 * 4
14

> /graph
[GRAPH mode activated]
graph> y = x^2 - 4
Graph generated.   (ASCII plot in the terminal)

> /eqn
[EQN mode activated]
eqn> x^2 - 4 = 0
Roots:
x = -2
x = 2
```

## 1. Build

You need three tools: **Flex**, **Bison**, and a **C compiler**. All
three are free and open-source.

### Windows (PowerShell)

```powershell
# One-time setup if you don't already have these:
#   - Flex + Bison: https://github.com/lexxmark/winflexbison (or MSYS2 / Chocolatey)
#   - A C compiler: MinGW-w64 gcc (via MSYS2, or the Chocolatey "mingw" package)
# Make sure flex.exe, bison.exe and gcc.exe are on PATH, then:

.\build.ps1
.\build\mathscript.exe
```

### macOS

```bash
xcode-select --install        # gives you a C compiler (clang, aliased as `cc`)
brew install flex bison       # macOS ships very old flex/bison otherwise

chmod +x build.sh
./build.sh
./build/mathscript
```

### Linux

```bash
sudo apt install flex bison gcc   # or your distro's equivalent
chmod +x build.sh
./build.sh
./build/mathscript
```

Either platform can also use the provided `Makefile` (`make` on
macOS/Linux) -- it runs the exact same three steps as `build.sh`.

## 2. Run

```bash
./build/mathscript                       # macOS/Linux: interactive REPL
.\build\mathscript.exe                   # Windows:     interactive REPL

./build/mathscript examples/calc_examples.ms    # macOS/Linux: run a script
.\build\mathscript.exe examples\calc_examples.ms  # Windows:   run a script
```

A MathScript session first picks a mode, then evaluates expressions
in that mode until you type `exit`:

```
> /calc
[CALC mode activated]
calc> (10 + 5)^2
225
calc> exit
[Leaving CALC mode]
> /exit
Goodbye.
```

Type `/help` at any top-level prompt for the full command list,
`/grammar` to print the exact BNF grammar being parsed, `/precedence`
to print the operator precedence table with worked examples, and
`/symbols` to inspect the symbol table.

## 3. Language quick reference

| Category      | Examples |
|----------------|----------|
| Arithmetic     | `+  -  *  /  ^` |
| Implicit multiply | `3x` or `3(x+1)` (same as `3*x`, `3*(x+1)`) |
| Comparisons    | `<  >  >=  <=  ==  !=` |
| Functions      | `sqrt(x) abs(x) pow(x,y) sin(x) cos(x) tan(x) log(x)` |
| Constants      | `pi  e` |
| Variables      | `x = 10` (only in `/calc`; any letter works, not just `x`/`y`) |
| Grouping       | `(2 + 3) * 4` |

Three modes, one grammar (see `docs/GRAMMAR.md`):

- **`/calc`** -- evaluate an expression, or assign a variable: `x = 10`
- **`/graph`** -- plots `y = <expression>` as an ASCII curve; also accepts
  a bare equation in one variable (`3*x = 1`, solved and shown on a number
  line) or in two variables (`x^2 + y^2 = 25`, plotted as an implicit
  curve like a line or circle)
- **`/eqn`** -- solve `<expr> = <expr>` for whichever single variable it
  uses (defaults to `x` if the equation is a constant); prints all real roots

`/graph`'s vertical range defaults to a fixed `[-50, 50]` (expanding only
if the data needs more) instead of auto-fitting to each function's own
min/max -- otherwise a shallow line and a steep one would each get
stretched to fill the frame and look equally steep. The curve itself is
drawn with `.`; axes are drawn with `|`, `-`, and `+` at the origin.

Inside `/calc`, prefix a line with `gen ` (e.g. `gen (5+3)*2`) to
additionally generate an equivalent C program, compile it with the
system C compiler, run it, and print its output -- a live
demonstration of executable code generation from the same IR.

## 4. Project structure

```
MathScript/
├── src/
│   ├── lexer.l          Flex lexical analyzer
│   ├── parser.y          Bison grammar (see docs/GRAMMAR.md)
│   ├── ast.h / ast.c      AST node types
│   ├── symtab.h / .c      Symbol table (variables, pi, e)
│   ├── errors.h / .c      Lexical / syntax / semantic / math error reporting
│   ├── eval.h / .c        AST evaluator + semantic checks
│   ├── ir.h / .c          AST -> Three-Address Code (IR)
│   ├── calc.h / .c        /calc backend
│   ├── graph.h / .c       /graph backend (ASCII plot: curves, 1- and 2-variable equations)
│   ├── solver.h / .c      /eqn backend
│   ├── rootfind.h / .c    shared root-finder (closed-form + bisection fallback)
│   ├── codegen.h / .c     TAC -> C source -> compile -> run
│   ├── util.h / .c        Shared number formatting
│   └── main.c             REPL / script driver, mode dispatch
├── examples/              Sample .ms scripts for all three modes
├── tests/                 Regression tests (run_tests.sh / run_tests.ps1)
├── docs/
│   ├── GRAMMAR.md              Full BNF + rationale
│   └── OPERATOR_PRECEDENCE.md  Worked precedence derivations
├── build.ps1 / build.sh / Makefile
└── output/                Generated IR/code artifacts (created at runtime)
```

## 5. Testing

```bash
./build.sh && ./tests/run_tests.sh          # macOS/Linux
.\build.ps1 ; .\tests\run_tests.ps1         # Windows
```

Both run the same set of checks: operator precedence, functions,
variables, all four error categories (lexical/syntax/semantic/math),
and every documented `/calc` `/graph` `/eqn` example, including every
row of the original test-case table (linear, quadratic, and cubic
equations).

## 6. Error handling

Every stage of the pipeline reports its own category of error and
stops before the next stage runs on bad input:

```
calc> 10 @ 5        ->  Lexical Error:  Invalid character '@'
calc> 10 + * 5       ->  Syntax Error:  ...
calc> foo(5)         ->  Semantic Error: Unknown function 'foo'
calc> 10 / 0         ->  Math Error:    Division by zero
```

## 7. Compiler pipeline

```
text --[Flex: lexer.l]--> tokens --[Bison: parser.y]--> AST
     --[eval.c: semantic checks]--
     --[ir.c: AST -> Three-Address Code]--
     --[calc.c | graph.c | solver.c: mode backend]--> result
                                         |
                                         +--[codegen.c]--> generated.c -> compiled & run
```
