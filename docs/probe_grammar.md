# Entity Manager "Probe" grammar

This document specifies the grammar and evaluation semantics of the `Probe`
field used in Entity Manager configuration records. It is the reference used by
the probe lexer in `src/entity_manager/probe_lexer.cpp` and the parser in
`src/entity_manager/probe_parser.cpp`.

## Surface syntax

A record's `Probe` value is either:

- a single statement string, or
- a JSON array of statement strings (used mainly for readability of long
  probes).

Both forms denote a single boolean expression. Before lexing, an array is joined
into one string with a single space between members, giving the lexer a single
entry point and avoiding array/string edge cases.

## Grammar (ISO/IEC 14977 EBNF)

```ebnf
(* A "Probe" denotes one boolean expression. Operators are evaluated strictly
   left-to-right with NO precedence (see "Precedence" below); parentheses are
   the only way to change the evaluation order. *)

probe            = expr , [ match_modifier ] ;
expr             = term , { binary_op , term } ;
term             = operand | "(" , expr , ")" ;
operand          = boolean_literal | found_probe | dbus_probe ;
binary_op        = "AND" | "OR" ;
match_modifier   = "MATCH_ONE" ;
boolean_literal  = "TRUE" | "FALSE" ;
found_probe      = "FOUND" , "(" , quoted_string , ")" ;
dbus_probe       = interface_name , "(" , match_object , ")" ;
interface_name   = identifier , { "." , identifier } ;
identifier       = ( letter | "_" ) , { letter | digit | "_" } ;
match_object     = "{" , [ match_pair , { "," , match_pair } ] , "}" ;
match_pair       = quoted_string , ":" , match_value ;
match_value      = quoted_string      (* string values are std::regex *)
                 | number
                 | json_boolean       (* lowercase true / false *)
                 | json_value ;       (* nested object / array *)
quoted_string    = "'" , { character - "'" } , "'" ;
json_boolean     = "true" | "false" ;
number           = [ "-" ] , digit , { digit } , [ "." , digit , { digit } ] ;
letter           = "A" | "..." | "Z" | "a" | "..." | "z" ;
digit            = "0" | "..." | "9" ;
```

Note: `boolean_literal` (`TRUE` / `FALSE`) is a probe operand and is upper-case;
a `json_boolean` inside a `match_object` is a JSON value and is lower-case
(`true` / `false`).

The parentheses in `found_probe` and `dbus_probe` belong to the _call_ and are
consumed with it by the lexer. A parenthesis appearing anywhere an operand or
`expr` is expected is a **grouping** parenthesis (`term = "(" expr ")"`).

## Evaluation semantics

Each operand evaluates to a boolean:

- `TRUE` / `FALSE` &mdash; the literal value.
- `FOUND('x')` &mdash; true if a probe named `x` has already passed
  (`scan->passedProbes`).
- `dbus_probe` &mdash; true if at least one D-Bus object exposes the given
  interface with **all** listed properties matching. String match values are
  compared as `std::regex`; other values by equality. Matched devices are
  recorded.

Combination and modifiers:

- **Precedence:** none. Within one `expr`, operators combine strictly
  left-to-right: `result := term0`, then for each `op term_i`,
  `result := result op term_i` (`AND` = `&&`, `OR` = `||`). Parentheses nest a
  sub-`expr` and are the only way to override this order.
- **No short-circuit:** every operand is evaluated, even when the running result
  already determines the outcome, so that all `dbus_probe` operands run and
  their matched devices are collected.
- `MATCH_ONE` &mdash; a whole-probe modifier that may appear only as the final
  token. If the overall result is true, keep only the last matched device.
- **Empty-device fallback** &mdash; if the result is true but no device matched
  (e.g. a `TRUE`- or `FOUND`-only probe), one empty device record is emitted.

## Precedence: why the default stays left-to-right

An audit of `configurations/` found **13 probes that mix `AND` and `OR`**, all
of the form:

```text
A OR B [OR C] AND FOUND('SomeBoard')
```

(e.g. Ampere Mt.Jade / Mt.Jefferson / Mt.Mitchell, Meta Harma, Meta Minerva).

Under the left-to-right rule this means `(A OR B OR C) AND FOUND(...)` &mdash;
"one of these FRUs matched **and** the board was found", which is the authors'
intent.

Introducing conventional precedence (`AND` binding tighter than `OR`) would
reinterpret these as `A OR B OR (C AND FOUND(...))`, breaking all 13 platforms.
Rather than change the default, grouping is added as an **opt-in**: the default
order is unchanged, so every existing probe parses and evaluates exactly as
before, and authors who want a different order write it explicitly.

## Grouping (parentheses)

Parentheses let an author override the default left-to-right order without
changing it globally:

```text
A OR (B AND FOUND('Board'))
```

evaluates `B AND FOUND('Board')` first, then ORs the result with `A` &mdash;
i.e. `A OR (B AND FOUND('Board'))`, which the flat form cannot express.

Rules:

- Groups may nest to any depth: `(A OR (B AND C))`.
- Redundant parentheses are allowed and collapse away: `(A)` is identical to
  `A`.
- A group must contain a complete `expr`; `()` is a syntax error.
- Parentheses must balance; an unmatched `(` or `)` is a syntax error.
- `MATCH_ONE` is a whole-probe modifier, not an operand, so it may not appear
  inside a group; it is only valid as the final token of the whole probe.

### Backward compatibility

Before grouping was added, a top-level parenthesis (one not immediately
following an interface name or `FOUND`) was a **lexer syntax error**, so no
existing configuration could contain one. Adding grouping therefore cannot
change the meaning of any existing probe: flat probes lex and parse into a
single left-to-right chain exactly as before, and parentheses only ever appear
in newly written probes.
