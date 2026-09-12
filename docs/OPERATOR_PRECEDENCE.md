# Operator Precedence in MathScript

This note explains, for the teacher/defense, exactly how MathScript's
grammar resolves operator precedence -- what code does it, why it is
correct, and how to demonstrate it live.

## 1. The problem: one rule, many meanings

The core of the grammar (`src/parser.y`) has a single, flat rule for
binary operators:

```
expr : expr '+' expr
     | expr '-' expr
     | expr '*' expr
     | expr '/' expr
     | expr '^' expr
     | ...
     ;
```

Written this way the grammar is **ambiguous**: the string `2 + 3 * 4`
can be parsed as `(2 + 3) * 4` (= 20) or `2 + (3 * 4)` (= 14) --
both are valid parses of the same rule. A parser generator like
Bison builds a table-driven shift/reduce automaton, and an ambiguous
grammar shows up as a **shift/reduce conflict**: when the parser has
just read `2 + 3` and the lookahead token is `*`, it must decide
whether to *reduce* `2 + 3` to `expr` first, or *shift* the `*` and
keep building the right-hand operand.

## 2. The fix: precedence and associativity declarations

Instead of rewriting the grammar into the classic layered form
(`expr -> expr + term`, `term -> term * factor`, ...), MathScript
keeps the flat rule above and tells Bison how to resolve the
conflict directly, with these two lines near the top of
`src/parser.y`:

```c
%left '<' '>' GE LE EQ NE
%left '+' '-'
%left '*' '/'
%precedence UMINUS
%right '^'
```

Two independent pieces of information are declared per line:

- **Precedence** -- lines listed *later* bind *tighter*. `^` (last
  line) has the highest precedence; comparisons (first line) have
  the lowest. When the parser is deciding between reducing with a
  lower-precedence operator or shifting a higher-precedence one, it
  shifts (the tighter operator wins).
- **Associativity** -- `%left` means "reduce" when the same operator
  repeats (so `a - b - c` groups as `(a - b) - c`), `%right` means
  "shift" instead (so `a ^ b ^ c` groups as `a ^ (b ^ c)`).
  `%precedence` gives unary minus a level with no associativity of
  its own, since it never repeats (`- - a` is two separate unary
  minus nodes, not decided by this rule at all).

This is exactly the same information the doc's original diagrams
describe as "MathScript follows normal mathematical precedence" --
these five lines *are* that rule, made executable.

## 3. Worked derivations

### `2 + 3 * 4` -> 14

When the parser holds `2 + 3` on its stack and sees `*` next, it
compares the precedence of `+` (the rule it could reduce) against
`*` (the token it could shift). `*` is declared with higher
precedence, so the parser **shifts** instead of reducing:

```
2 + 3 * 4
    -> 2 + (3 * 4)      ('*' binds tighter, so it groups first)
    -> 2 + 12
    -> 14
```

Verify it live:

```
calc> 2 + 3 * 4
IR (TAC):
t1 = 3 * 4
t2 = 2 + t1
RESULT = t2
14
```

Notice the generated TAC itself shows the grouping: `t1 = 3 * 4` is
computed *before* it is added to `2` -- the IR is a direct
reflection of the precedence-resolved parse tree.

### `-2 ^ 2` -> -4, not 4

This is the subtler case, and the reason `UMINUS` sits *below* `^`
in the precedence list. Mathematically, `-2^2` is universally read as
`-(2^2) = -4` (this matches Python's `**`, for example) -- **not**
`(-2)^2 = 4`. Because `^` has higher precedence than unary minus in
`parser.y`, the parser shifts the `^ 2` onto the `2` before applying
the unary minus:

```
- 2 ^ 2
    -> - (2 ^ 2)        ('^' binds tighter than unary minus)
    -> - (4)
    -> -4
```

```
calc> -2^2
IR (TAC):
t1 = 2 ^ 2
t2 = 0 - t1
RESULT = t2
-4
```

If the two lines were swapped (`UMINUS` given higher precedence than
`^`), the same input would instead compute `(-2)^2 = 4` -- a good
one-line demonstration of what precedence actually controls.

### `2 ^ 3 ^ 2` -> 512, not 64

`^` is declared `%right`, so repeated `^` groups from the right:

```
2 ^ 3 ^ 2
    -> 2 ^ (3 ^ 2)      ('^' is right-associative)
    -> 2 ^ 9
    -> 512
```

If `^` were `%left` instead, the same input would group as
`(2 ^ 3) ^ 2 = 8 ^ 2 = 64`.

```
calc> 2^3^2
IR (TAC):
t1 = 3 ^ 2
t2 = 2 ^ t1
RESULT = t2
512
```

## 4. How to show this live in the defense

1. Run `mathscript`, enter `/precedence` to print the table above
   from the compiler itself (`main.c`'s `PRECEDENCE_TEXT`).
2. Enter `/calc` and type the three examples above; point out that
   the printed IR/TAC line order *is* the resolved parse order.
3. Open `src/parser.y` and show the five `%left` / `%right` /
   `%precedence` lines are the entire mechanism -- nothing else in
   the grammar encodes precedence.
4. Optional: comment out the `%precedence`/`%right` lines, rebuild
   with `bison -d -o build/parser.tab.c src/parser.y`, and show
   Bison now reports shift/reduce conflicts on stderr -- proof that
   those lines are doing real work, not decoration.
