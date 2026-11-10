# Entity Manager "Probe" grammar

This document specifies the grammar and evaluation semantics of the `Probe`
field used in Entity Manager configuration.

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
(* A "Probe" denotes one boolean expression, evaluated strictly
   left-to-right with NO operator precedence (see "Precedence" below). *)

probe            = operand , { binary_op , operand } , [ match_modifier ] ;
operand          = { unary_op }, ( boolean_literal | dbus_probe ) | found_probe ;
unary_op         = "NOT" ;
binary_op        = "AND" | "OR" ;
match_modifier   = "MATCH_ONE" ;
boolean_literal  = "TRUE" | "FALSE" ;
found_probe      = "FOUND" , "(" , quoted_string , ")" ;
dbus_probe       = interface_name , "(" , match_object , ")" ;
interface_name   = interface_segment , { "." , interface_segment } ;
interface_segment = ( letter | "_" ) , { letter | digit | "_" } ;
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

## Evaluation semantics

Each operand evaluates to a boolean:

- `TRUE` / `FALSE`: the literal value.
- `FOUND('x')`: true if a probe named `x` has already passed
  (`scan->passedProbes`).
- `dbus_probe`: true if at least one D-Bus object exposes the given interface
  with **all** listed properties matching. String match values are compared as
  `std::regex`; other values by equality. Matched devices are recorded.
- `NOT`: negates the `TRUE`, `FALSE` or `dbus_probe` that immediately follows
  it; repeated `NOT`s negate in turn. It binds only that operand, so
  `NOT A AND B` negates `A` but not `B`, and `A AND NOT B` is true when `A`
  matches and no object matches `B`. A negated `dbus_probe` is true when no
  object matches, and records no devices. A statement with `NOT` before
  `FOUND(...)`, a binary operator or `MATCH_ONE`, or at its end is rejected: an
  error is logged and the configuration is not used. `FOUND(...)` cannot be
  negated: its result changes as probes pass, so `NOT FOUND('x')` can be true
  before `x` has passed.

Combination and modifiers:

- **Precedence:** none. Operators combine strictly left-to-right:
  `result := operand0`, then for each `binary_op operand_i`,
  `result := operand_i binary_op result` (`AND` = `&&`, `OR` = `||`). `NOT` is
  not combined this way: it applies only to the operand that follows it.
- `MATCH_ONE`: if the overall result is true, keep only the last matched device.
- **Empty-device fallback**: if the result is true but no device matched (e.g. a
  `TRUE`- or `FOUND`-only probe, or a probe whose only `dbus_probe` is negated),
  one empty device record is emitted.

## Precedence: why it stays left-to-right

An audit of `configurations/` found **some probes that mix `AND` and `OR`**, all
of the form:

```text
A OR B [OR C] AND FOUND('SomeBoard')
```

(e.g. Ampere Mt.Jade / Mt.Jefferson / Mt.Mitchell, Meta Harma, Meta Minerva).

Under the current left-to-right rule this means `(A OR B OR C) AND FOUND(...)`:
"one of these FRUs matched **and** the board was found", which is the authors'
intent.

Introducing conventional precedence (`AND` binding tighter than `OR`) would
reinterpret these as `A OR B OR (C AND FOUND(...))`, breaking existing
platforms. Because the language has no grouping syntax, authors could not
rewrite them to restore the intent either.
