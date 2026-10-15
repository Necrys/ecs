# Code style

Rules for the layout of `ecs` code. Formatting is enforced mechanically by
`.clang-format`; everything else is a convention that has to be followed
deliberately.

The project is built as **C++17**. Catch2 v3, which the tests use, requires at
least C++14, and the standard was moved straight to 17 to avoid a second
migration.

Autoformatting is mandatory and not up for discussion: **do not align code by
hand**. To apply and to check:

```powershell
./tools/format.ps1            # format in place
./tools/format.ps1 -Check     # check only, change nothing
```

On any platform the same is done by `python tools/format.py` (use `--check` to
check). For individual files, use `-Paths` / `--files`.

---

## 1. Principles

1. **Clarity beats brevity, brevity beats generality.** Code should read without
   comments; abstraction is introduced when a second use case appears, not in
   advance.
2. **Headers must not drag in more than they need.** Every `#include` in a `*.h`
   is paid for in build time by everyone who includes it. Include only what is
   required.
3. **Zero cost wherever possible.** The library is positioned as a performant
   one: virtual calls, allocations and copies on the hot path must be
   justified.
4. **Interface symmetry.** If there is a non-`const` overload, there is a
   `const` one; if there is `add`, there is `remove`; and so on.
5. **Data and logic stay separate.** Containers hold data; behaviour lives in
   systems. See 6.5.
6. **Resolve at compile time what can be resolved then.** Templates are the
   deliberate mechanism for this. See 6.3.

---

## 2. Naming

The style is inherited from the existing code and is mandatory for new code.

| Entity | Style | Example |
|---|---|---|
| Type, class, struct | `snake_case` | `sparse_vector`, `components_storage`, `eid_t` |
| Template type parameter | `CamelCase` | `template < typename T, size_t PageSize >` |
| Function, method | `snake_case` | `get_entity_component`, `add_system` |
| Variable, parameter | `snake_case` | `page_idx`, `min_index` |
| Private class member | `m_` + `snake_case` | `m_pages`, `m_entityIdCounter` |
| Static member, constant | `snake_case` | `bad_index`, `bad_id` |
| Compile-time constant | `snake_case` | `PageSize` (as a parameter) |
| Namespace | `snake_case` | `ecs` |
| Macro | `UPPER_SNAKE` | (avoid) |

Notes:

- Existing names such as `m_componentStorages` mix styles - that is historical
  baggage. New code follows the table; renaming inherited names is a separate
  task, not something to do along the way.
- Identifiers are **in English**. Comments and documentation are in English too:
  see the language rule in `AGENTS.md`.
- Abbreviations are acceptable only when widely understood (`id`, `idx`, `pos`,
  `ptr`, `count`). `page_idx` yes, `pgi` no.
- Boolean functions read as a statement: `exist`, `enabled`, `ready`,
  `is_system_active`.

---

## 3. Formatting

Enforced by `.clang-format` (verified with clang-format 19 and 22; the results
are identical apart from one line break). The key traits, so it is clear why the
code looks the way it does:

- **2-space** indentation, no tabs;
- the opening brace stays **on the same line** - for functions and for classes;
- spaces **inside** brackets: `foo( arg )`, `arr[ i ]`, `vector< T >`,
  `template < typename T >`;
- line length - 120;
- pointers and references are **left-aligned**: `registry* owner`,
  `T& add(...)`;
- the body of `namespace ecs {` is **not indented**;
- consecutive assignments and declarations are **not aligned**.

Two tool limitations are worth knowing:

- empty argument lists are always `foo()`, the `foo( )` form is impossible -
  the option was removed from clang-format in v19;
- `SECTION` blocks in tests are formatted as ordinary nested blocks and get
  indented, even though historically they sat at column zero.

Tests use Catch2 v3 (`<catch2/catch_test_macros.hpp>`). The test runner is
provided by the `Catch2WithMain` target, so test sources must not define
`CATCH_CONFIG_MAIN` and must not provide their own `main`.

---

## 4. Headers and file layout

- `*.h` - declarations: classes, signatures, members. Heavy implementations do
  not belong here.
- `*.hpp` - template and `inline` implementations, included at the end of the
  `*.h`.
- `src/*.cpp` - non-template implementations.
- Include guards are `#pragma once`.
- Include order: the corresponding header first, then the standard library, then
  project headers. The formatter does **not** sort includes - the order is a
  deliberate decision.
- Inside a class: `public` first, then `protected`, then `private`. Do not
  repeat sections.

---

## 5. Comments and documentation

Priority order, from most to least important.

### 5.1. Self-documenting code comes first

A name should answer "why" without a comment. Before writing a comment, try
renaming. If a comment is still needed after renaming, it is justified.

```cpp
// bad: the comment explains the obvious
// returns true if the page exists
bool exist( size_t pos ) const;

// good: the name speaks for itself, no explanation needed
bool has_page( size_t page_idx ) const;
```

### 5.2. Comments are lightweight

- **One or two lines.** Only what is not obvious: an invariant, the reason
  behind an unusual decision, a pitfall.
- Do not restate the code and do not duplicate the signature.
- **Do not write history or research rationale** - that has no place in code.
- A comment that goes stale faster than the code is worse than no comment. If
  you doubt it will survive a refactor, do not write it.

```cpp
// acceptable: a non-obvious invariant
// m_back_index stores positions in insertion order, so erase shifts it too
std::array< size_t, PageSize > m_back_index;

// not acceptable: history and research rationale
// std::map was used here originally, but an experiment showed that vector is
// three times faster on our workload, so it was decided...
```

Allowed forms: `//` for lines, `/* ... */` for section separators (as in
`sparse_vector.hpp`). No Doxygen markup - it is not used and is not planned.

### 5.3. Separate documentation

- `README.md` - an introduction to the project: what it is, how to build it, how
  to use it, examples.
- Documentation generated from source - **only when there is a real need**; do
  not set it up in advance.
- Plans, history and design rationale do not go into the repository.
- **The repository must not reference material that is not in it** - a reader
  clones the project and should see only what belongs to it.

---

## 6. Code

### 6.1. Resources and ownership

- Ownership is expressed explicitly: `std::unique_ptr` for sole ownership,
  `std::shared_ptr` for shared. Raw `new`/`delete` only inside low-level
  containers (such as `sparse_vector`), where it is a deliberate decision.
- A class that owns a resource must release it in its destructor. **A missing
  destructor on an owning class is a defect**, not an optimisation.
- Rule of five: if a destructor, copy or move operation is declared, declare or
  delete all the others (`= default` / `= delete`).

### 6.2. Functions

- A single exit point is preferred, but an early `return` to reject invalid
  states is acceptable and encouraged.
- Pass cheap types by value, larger ones by `const&`, and mutable outputs by
  pointer or reference deliberately.
- A method that does not modify the object **must** be `const`.
- Error signalling: `nullptr` for "not found", exceptions only for genuinely
  exceptional situations (for example, running out of entity ids).

### 6.3. Templates

Templates are the main tool for the top-priority goal: work that can be decided
at compile time must not be deferred to run time. Type ids, storage lookups and
iteration over a pack of component types are all expected to collapse into
constant code.

- Prefer a compile-time mechanism over a run-time one. Virtual dispatch, type
  erasure and tables keyed by a run-time value are the fallback, not the default;
  if one is needed, the reason belongs in a comment.
- Use `static_assert` to state requirements on parameters, so violations are
  reported where they are made rather than deep inside instantiation.
- Prefer `if constexpr` and concepts over `std::enable_if` and tag dispatch;
  `enable_if` remains in the current code and is worth replacing when touched.
- Lazy initialisation through local `static` variables in templates is
  acceptable, but keep initialisation order across translation units in mind.
- Templating is a cost, not a virtue in itself. Do not template something whose
  parameters are never varied at compile time, and do not buy readability with
  metaprogramming that no one can follow - raise it instead of layering more on
  top.

### 6.4. Data design

Containers store data. They do not implement behaviour on that data: no
component-specific logic, no callbacks that know about a particular component
type, no policy hooks that exist only to serve one use case. Traversal and
storage are the whole contract.

The containers are POD-first. A component is expected to be plain, trivially
destructible data, and the hot paths are written on that assumption. Types that
own resources are a different problem: their lifetimes need explicit management,
and that is not solved by making the ordinary container heavier.

- Do not add destructor or exception handling to the existing containers to
  accommodate non-POD types. A separate container variant is the intended place
  for that work.
- Do not blur the line by letting a container know what a component means. If a
  container needs to ask what type it holds in order to do its job, the logic is
  in the wrong place.
- When touching iteration, keep the cost proportional to the number of matching
  entities: walk the smallest storage involved rather than the span of
  identifiers, so that a sparse world does not cost more than a dense one.

### 6.5. Forbidden

- `using namespace` in headers.
- Macros for constants and "functions" - use `constexpr` and templates.
- C-style casts - use `static_cast` and friends.
- Magic numbers - name your constants.
- Commented-out code and debug output in a commit.
- Uninitialised POD members.

---

## 7. Checklist before committing

1. `./tools/format.ps1` (or `python tools/format.py`) - the code is formatted.
2. The build is warning-free at the highest warning level.
3. The tests pass; a regression test was added for the fixed defect.
4. Comments do not restate the code and contain no history.
5. No commented-out code, debug output or dead members.
6. A change on the hot path is backed by a measurement.
