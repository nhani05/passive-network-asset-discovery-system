---
name: write-clear-comments
description: Write, revise, or review code comments and documentation comments so they are clear, clean, standard, useful, and maintainable. Use when Codex is asked to add comments, clarify confusing comments, clean noisy comments, standardize comment style, improve inline or block comments, write API/doc comments, or decide whether comments are needed in source code, tests, configuration, scripts, or examples.
---

# Write Clear Comments

## Core Standard

Prefer comments that explain intent, constraints, tradeoffs, side effects, invariants, domain rules, or non-obvious behavior. Avoid comments that restate the code, describe syntax, narrate each assignment, preserve outdated history, or compensate for unclear names that should be improved instead.

Make comments accurate, specific, and short. Use the local language, style, punctuation, and doc-comment format already present in the file unless the user asks for a different standard.

## Workflow

1. Read the nearby code before editing comments.
2. Identify what a future maintainer might misunderstand: why the code exists, what must stay true, what external contract it satisfies, or what edge case it protects.
3. Improve names or structure instead of adding a comment when the code can be made self-explanatory with a small, in-scope edit.
4. Add or revise comments only where they reduce real ambiguity.
5. Remove stale, redundant, misleading, decorative, or TODO-style comments unless the TODO is actionable and locally accepted.
6. Verify comments still match the final code after edits.

## Comment Types

Use inline comments sparingly for local surprises:

```cpp
// Keep the original timestamp so repeated sightings do not hide asset age.
asset.first_seen = existing.first_seen;
```

Use block comments for multi-step reasoning, protocol details, safety constraints, or dense algorithms. Keep them close to the code they explain.

Use API or doc comments for public interfaces, non-obvious parameters, ownership, lifetime, errors, return semantics, units, and thread-safety. Do not document obvious getters, setters, or constructors unless the interface has a meaningful contract.

Use TODO comments only when they include a concrete next action and enough context to evaluate it later. Prefer the repository's existing TODO format if one exists.

## Quality Checklist

Before finishing, ensure comments are:

- Clear: plain language, no vague wording like "handle stuff" or "magic".
- Correct: synchronized with the code's current behavior.
- Useful: explain why, not just what.
- Clean: no clutter, dead notes, jokes, or historical leftovers.
- Standard: match nearby formatting and comment conventions.
- Durable: avoid implementation details likely to drift unless those details are the point.

## Examples

Replace restatement:

```cpp
// Increment count by one.
++count;
```

with no comment, or with intent when needed:

```cpp
// Count only accepted packets so malformed traffic does not affect rate metrics.
++count;
```

Replace vague explanation:

```cpp
// Special case for bad data.
```

with the actual constraint:

```cpp
// Some captures omit Ethernet padding; skip length checks until the IP header is present.
```
