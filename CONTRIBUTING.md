# Contributing

## Standards

- Keep modules small and named after the domain they own.
- Prefer explicit types and descriptive names over clever abstractions.
- Reuse seek, grid, and validation helpers instead of duplicating logic in separate layers.
- Comments should explain intent, invariants, or non-obvious tradeoffs. Do not narrate obvious code.
- Add tests when changing seek math, grid layout, validation, or serialization behavior.

## Review Checklist

- Public Rust APIs have `rustdoc` where the behavior is not obvious.
- Public TypeScript modules and complex component contracts have TSDoc.
- New UI logic lives in hooks or stores, not inside render functions.
- New commands or settings update the docs in `docs/` and the changelog when user-visible.

## Branch Workflow

1. Add or update tests.
2. Run local checks for the touched area.
3. Update docs for user-facing changes.
4. Keep commits focused by subsystem.

## API Documentation

- Generate Rust API docs with `pnpm docs:api:rust` or `cargo doc --no-deps -p vpg-core -p vpg-cli`.
- Generate TypeScript API docs with `pnpm docs:api:ts`.
- Use doc comments to explain purpose, invariants, and edge cases. Avoid narrating obvious syntax.

## Git Hygiene

- Use topic branches for features and fixes.
- Keep binary assets out of unrelated commits.
- Respect the shared line-ending and text normalization rules from `.editorconfig` and `.gitattributes`.
- If you create commits in this repo, use the Git identity already configured on your machine. Do not hardcode a different author in repo config.
