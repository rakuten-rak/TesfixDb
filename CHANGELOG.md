# Changelog

While Tesfix is in alpha (versions starting with 0), the API can change between releases.
Changes that can break your code are marked **Breaking**.

## [Unreleased]

## [0.1.0-alpha.1] - 2026-11-20

First public alpha, for evaluation.

### Added

- Store and read JSON documents: `insert`, `get`, `patch` and `delete`.
- Search with `find` and `count`: conditions (`=`, `!=`, `<`, `<=`, `>`, `>=`), sorting,
  `limit` and `offset`.
- Indexes on fields with `createIndex`.
- All-or-nothing changes with `transaction`.
- Databases kept in memory, or saved in the browser's IndexedDB.
- TypeScript types.
- A `wasmUrl` option to say where the `.wasm` file is served from.
- npm i tesfix@next

### Notes

- Tesfix is free to evaluate under the Tesfix Evaluation License. See `LICENSE`.
- Known limits of the alpha (two tabs, the short delay before saving, and others) are listed
  in the README.