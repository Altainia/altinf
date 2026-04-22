# AltInf — Claude Guidelines

## Coding style

- **Naming**: Everything is `snake_case` — variables, functions, classes, structs, enums, enum values, constants, concepts, filenames — **except** template typenames, which are `SnakeCase` (PascalCase).
- **Member variables**: Private and protected members use an `m_` prefix and no suffix. Public members have no prefix or suffix.
- **Const by default**: Declare variables `const` unless mutation is required.
- **Formatting**: Run `clang-format -i` on every `.cpp` and `.h` file you create or modify. The project `.clang-format` is at the repo root.

## Git workflow

### Feature branches
When working on a new feature:
1. Create a branch: `git checkout -b feature/<short-description>`
2. Commit automatically after each logical unit of work is complete (no waiting for the user to ask).
3. Leave the branch for the user to review — do not merge or delete it unless the user asks.

### Bug branches
When fixing a bug:
1. Create a branch: `git checkout -b bug/<short-description>`
2. Commit automatically once the fix is complete and verified.
3. Leave the branch for the user to review — do not merge or delete it unless the user asks.

### Main branch
Do not commit directly to `master`/`main` — always work through a feature or bug branch unless the user explicitly says otherwise.
