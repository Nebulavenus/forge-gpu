# KaTeX on GitHub — Style Guide

GitHub renders math blocks using a restricted subset of KaTeX. This guide
documents the patterns that work and the ones that break, based on testing
against GitHub's actual renderer.

## Display equations

Use `$$` fences:

```markdown
$$
f(x) = x^2 + 1
$$
```

## Subscripts

A single `_` is the subscript operator. Use it freely:

```latex
d_1, d_2, p_x, g_y
```

**Two consecutive subscripts are an error.** KaTeX rejects two `_` or `\_`
in sequence without an intervening group:

```latex
%% BAD — "Double subscripts: use braces to clarify"
\text{sdf}\_\text{smooth}\_\text{union}
```

## Code identifiers with underscores

GitHub's KaTeX does not support putting multiple underscores inside
function names in equations. Every approach we tested fails:

- **Separate text groups** with two `\_` between them → "Double subscripts"
- **Single text group** with `\_` inside `\text` → "`_` allowed only in math mode"
- **Same pattern** with `\texttt` or `\mathrm` → same errors as above
- **`\mathtt`** with braced `{\_}` inside → "Extra close brace"
- **`\mathtt`** groups joined by braced `{\_}` → "Extra close brace"
- **`\operatorname`** → "Macro not allowed" (blocked on GitHub)
- **`\textunderscore`** inside `\text` → does not render (even in
  `` ```math `` fence)

**The solution:** put the code name in a markdown code span before the
equation and use a short math variable inside:

```markdown
The `sdf_smooth_union` function blends the two distances:

$$
f(d_1, d_2, k) = \text{lerp}(d_2, d_1, h) - k \cdot h \cdot (1 - h)
$$
```

This is clean, readable, and avoids all KaTeX underscore issues.

## Single-underscore names

A single `\_` between `\text{}` groups works fine:

```latex
%% GOOD — one underscore
\text{sdf2}\_\text{circle}(p, r)
```

The double-subscript error only triggers with **two or more** `\_`
operators in the same expression — a single `\_` is fine because there
is only one subscript.

## Math operation names

For named operations without underscores (lerp, clamp, min, max), use
`\text{}`:

```latex
\text{lerp}(a, b, t)
\text{clamp}(x, 0, 1)
```

Note: `\operatorname{}` is blocked on GitHub for custom names.

## Inline math

Inline math (`$...$`) has an additional hazard: GitHub's Markdown parser
may interpret `_` as emphasis before KaTeX sees it. If an inline formula
breaks, use the backtick-dollar fence:

```markdown
$`\text{lerp}(a, b, t)`$
```

Display blocks (`$$`) do not have this problem.

## Quick reference

| Want | Write |
|---|---|
| Subscript | `d_1` |
| Operation name (no underscores) | `\text{lerp}(a, b, t)` |
| Code name (one underscore) | `\text{sdf2}\_\text{circle}(p, r)` |
| Code name (multiple underscores) | Code span before equation, math variable inside |

## Common errors

| Error message | Cause | Fix |
|---|---|---|
| Double subscripts | Two `\_` in sequence | Move name to code span, use math variable |
| `_` allowed only in math mode | Bare `_` inside `\text{}` | Use `\_` (single only) or code span |
| Macro not allowed | `\operatorname` or other blocked macros | Use `\text{}` instead |
| Rendering as italic text | Markdown ate `_` before KaTeX | Use backtick-dollar fence for inline |
