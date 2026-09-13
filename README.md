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

> /graph2d
[GRAPH2D mode activated]
graph2d> y = x^2 - 4
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

Four modes, one grammar (see `docs/GRAMMAR.md`):

- **`/calc`** -- evaluate an expression, or assign a variable: `x = 10`
- **`/graph2d`** -- plots `y = <expression>` as an ASCII curve; also accepts
  a bare equation in one variable (`3*x = 1`, solved and shown on a number
  line) or in two variables (`x^2 + y^2 = 25`, plotted as an implicit
  curve like a line or circle)
- **`/graph3d`** -- plots explicit or implicit equations in up to three variables,
  with an ASCII preview and an offline HTML view you can rotate and zoom
- **`/eqn`** -- solve `<expr> = <expr>` for whichever single variable it
  uses (defaults to `x` if the equation is a constant); prints all real and complex roots for degree 1, 2, and 3 polynomials
  (other expressions use a numerical real-root search over `[-25, 25]`)

In `/graph2d`, a bare expression in `y` is shorthand for `x = <expression>`:
`y^2` plots both branches of `x = y^2`, and `y^3` plots `x = y^3`,
including negative coordinates. You can equivalently write `pow(y,2)` or
`pow(y,3)`. Both bare expressions and equations such as `x = pow(y,2)`
or `pow(y,3) = x` are supported. Explicit equations such as `y^2 = x`
and `y^3 = x` also work. Implicit curves print sample `(x, y)` points
on a labeled **200-column by 75-row** canvas. Horizontal coordinates span
`[-10, 10]`. The vertical span is calculated from the canvas dimensions and
terminal cell proportions, approximately `[-7.437, 7.437]` in this frame.
With typical characters twice as tall as wide, one mathematical unit takes
the same physical distance on both axes: circles look round and sideways
parabolas no longer look flattened. Actual proportions depend on your font.
The default 2D plots share this scale so their shapes and slopes remain comparable.

Curves and axes use dense `.` points. Every terminal cell along each axis
contains a dot, so the axes form continuous dotted lines across the original
**200 by 75** canvas. Whole-unit labels indicate the scale; the dots between
labels represent smaller fractions of a unit. The canvas size and coordinate
scale are unchanged. There is no outer border or background grid. One-variable
number lines also use continuous dots, with `*` for roots. Values outside the viewport are clipped with a notice;
undefined or non-finite samples are skipped. Samples are drawn independently
so no straight segment is invented across a discontinuity. Use `/tac off`
for a cleaner graph session. Allow about 216 terminal columns for the canvas
and labels; narrower windows may wrap the output.

The former `/graph` command is now `/graph2d`; `/graph` remains a compatibility
alias for existing scripts. Implicit curves are scanned along **both axes**,
and every detected root is drawn without the old four-branches-per-column limit.
Polynomial slices up to degree three use coefficient extraction; other expressions
use a numerical scan that refines sign changes and tangencies while rejecting poles.

To inspect another part of a 2D graph, use `/range xmin xmax ymin ymax`.
For example, `/range -20 20 -15 15` expands the view. `/samples 64` increases
sampling from the default 32 samples per terminal column (6,369 horizontal
positions in the standard canvas). Samples retain fractional coordinates
between the one-unit labels for smooth curves. Coordinate tables also use
one-unit steps; very wide views omit some labels to keep them readable. Custom ranges may
change the relative axis scale. Settings apply to subsequent plots in that mode.

### 3D graphs

```text
/tac off
/graph3d
vccw
x^2 + y^2 + z^2 = 25
z = sin(x) + cos(y)
x^2 + y^2 = 9
/range -6 6 -6 6 -6 6
/samples 96
x^2 + y^2 + z^2 = 25
```

Entering `/graph3d` shows a `rotation>` prompt. Choose one of these four
commands, then enter the equation at `graph3d>`:

| Command | Rotation axis | Direction |
|---------|---------------|-----------|
| `vccw` | Vertical, y | Counterclockwise |
| `hccw` | Horizontal, x | Counterclockwise |
| `vcw` | Vertical, y | Clockwise |
| `hcw` | Horizontal, x | Clockwise |

Clockwise/counterclockwise is viewed from the positive end of the chosen axis
toward the origin. These commands apply only inside `/graph3d`; the choice
persists for subsequent equations. Enter another rotation command to change
the next graph, or re-enter `/graph3d` to choose again. An equation is not
plotted until a rotation is selected. `/range`, `/samples`, `/help`, and mode
switches still work at the rotation prompt.

`/graph3d` accepts equations with expressions on either side, including spheres,
ellipsoids, cones, planes, cylinders, and surfaces with several branches.
A bare expression such as `sin(x)+cos(y)` means `z = sin(x)+cos(y)`.
Equations need not be solvable as a single `z = f(x,y)`: scanning along all
three axes captures vertical components and both sides of closed surfaces.
Missing coordinates remain free, so `x=2` is a plane and `x^2+y^2=9` is a
cylinder. `x`, `y`, `z` keep their conventional axis order; other variable
names are ordered alphabetically, with unused standard names filling empty axes.
`pi` and `e` remain constants, and graphing restores stored calculator variables.

Each 3D graph prints an isometric terminal preview and saves a new
`output/graph3d_NNN.html` file relative to the working directory (which must
contain a writable `output/` directory). Open the printed file in any modern browser.
The HTML graph starts rotating automatically at 30 degrees per second around
the selected coordinate axis through the view center. The default view places
y vertically and x horizontally, with z showing depth. Rotation preserves
the sampled coordinates and coordinate scale. Use **Pause rotation** /
**Resume rotation** to control animation; **Reset view** resets its angle and
camera. It works offline: drag or use arrow keys to adjust the camera, and
scroll or use `+`/`-` to zoom. Animation pauses while dragging or when the tab
is hidden. The terminal preview is static; open the printed HTML file to see
the selected rotation.
The x, y, and z axes form continuous dotted lines in both the terminal and
browser views. Browser axes use tiny dots at screen-pixel density (thousands
across a typical view), with larger markers at whole units. Red, green, and
blue distinguish the axes. Dot density follows zoom and display resolution
without changing mathematical scale or canvas size. If zero is outside the
view, reference axes move to the nearest visible boundary.
Existing graph files are preserved. Run `examples/graph3d_examples.ms` for a demo.

The default 3D range is `[-10,10]` on each axis with 64 subdivisions per axis.
Use `/range xmin xmax ymin ymax zmin zmax` and `/samples N` (integer 4–256)
to adjust the next plot. Higher resolution takes more time and memory.
2D and 3D settings are independent.

A continuous graph has infinitely many points: these are **finite numerical
samples within the chosen range**, not an exact enumeration of all solutions.
Very small components, closely spaced roots, isolated points, and rapid oscillations
can still be missed. Increase resolution or narrow the range to inspect them.
Undefined and non-finite values are skipped; coordinates outside the view are omitted.

Inside `/calc`, prefix a line with `gen ` (e.g. `gen (5+3)*2`) to
additionally generate an equivalent C program, compile it with the
system C compiler, run it, and print its output -- a live
demonstration of executable code generation from the same IR.

In `/eqn`, variable names such as `x`, `y`, and `z` all work. Use either
`^` or `pow` and put expressions on either side of `=`:

```text
/eqn
y^2 - 2*y + 5 = 0
# y = 1 - 2i, y = 1 + 2i
pow(z,3) - 1 = 0
# z = -0.5 - 0.866025i, z = -0.5 + 0.866025i, z = 1
```

Polynomial coefficients are collected from arithmetic expressions (including
products, parentheses, and division by constants). Degree 1–3 polynomials
include repeated roots and have no search-window restriction. Roots are
computed numerically and printed to six significant digits. An equation must
contain only one distinct variable; `pi` and `e` remain constants.

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
│   ├── graph.h / .c       /graph2d backend (ASCII plot: curves, 1- and 2-variable equations)
│   ├── graph3d.c          /graph3d backend (sampled surfaces + interactive HTML)
│   ├── solver.h / .c      /eqn backend
│   ├── poly.h / .c      Polynomial coefficients and degree 1–3 real/complex roots
│   ├── rootfind.h / .c    shared root-finder (polynomial extraction + numerical scan)
│   ├── codegen.h / .c     TAC -> C source -> compile -> run
│   ├── util.h / .c        Shared number formatting
│   └── main.c             REPL / script driver, mode dispatch
├── examples/              Sample .ms scripts for all four modes
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
python3 tests/test_polynomial_roots.py       # optional full root-set checks (Python 3)
python3 tests/test_graph_rendering.py        # geometry, clipping, and domain-gap checks
python3 tests/test_graph3d.py                # 3D surface geometry, branches, settings
node tests/test_graph3d_viewer.js            # offline viewer drawing and controls
.\build.ps1 ; .\tests\run_tests.ps1         # Windows
```

Both run the same set of checks: operator precedence, functions,
variables, all four error categories (lexical/syntax/semantic/math),
and every documented `/calc` `/graph2d` `/graph3d` `/eqn` example, including every
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
