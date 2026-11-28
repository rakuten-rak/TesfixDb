# Tesfix

> **Alpha (0.1.x).** The API may still change, and there are known limits
> Do not use it as the only copy of important data yet.

```js
import { Tesfix } from 'tesfix';

const db = await Tesfix.open('myapp');            // saved in this browser's IndexedDB

db.insert('users', '1', { name: 'Ama', age: 28, city: 'Accra' });
db.insert('users', '2', { name: 'Kofi', age: 31, city: 'Kumasi' });

db.get('users', '1');                             // { name: 'Ama', age: 28, city: 'Accra' }

db.find('users', {
  where: [['age', '>', 25]],
  order_by: [['age', 'desc']],
  limit: 10,
});                                               // [ { name: 'Kofi', ... }, { name: 'Ama', ... } ]

db.patch('users', '1', { age: 29 });              // change one field, keep the rest
```

Tesfix is a plain ES module with no framework code, so React, Vue, Svelte, Angular and plain
JavaScript all use it the same way.

## Install

download the tar -tgz or the prebuilt source codes files then, run
```bash
 npm install /path/to/download/tesfix-0.1.0-aplha.0.tgz
```
or 
```bash
npm install tesfix@next
```

## Using it with a bundler

Tesfix loads a WebAssembly file (`tesfix_wasm.wasm`). The most reliable way across bundlers is to
tell it where that file is, with the `wasmUrl` option.

**Vite**

```js
import { Tesfix } from 'tesfix';
import wasmUrl from 'tesfix/tesfix.wasm?url';

const db = await Tesfix.open('myapp', { wasmUrl });
```

and in `vite.config.js`, so Vite does not pre-bundle the module and break the file lookup:

```js
export default { optimizeDeps: { exclude: ['tesfix'] }, build: { target: 'esnext' } };
```

**webpack, Next.js, Angular and others**

Copy the file into your static folder and point to it:

```bash
# macOS / Linux / Windows PowerShell
cp node_modules/tesfix/dist/tesfix_wasm.wasm public/

# Windows Command Prompt (cmd)
copy node_modules\tesfix\dist\tesfix_wasm.wasm public\
```

```js
const db = await Tesfix.open('myapp', { wasmUrl: '/tesfix_wasm.wasm' });
```

**Server-side rendering (Next.js, Nuxt, SvelteKit)**

Saved databases only exist in a browser. Open them inside the browser only, for example in
`useEffect` in React, `onMounted` in Vue, or `onMount` in Svelte:

```js
useEffect(() => {
  let db;
  (async () => {
    const { Tesfix } = await import('tesfix');
    db = await Tesfix.open('myapp', { wasmUrl: '/tesfix_wasm.wasm' });
    setNotes(db.find('notes'));
  })();
  return () => { db?.close(); };
}, []);
```

## API

Everything is synchronous except `open`, `flush` and `close`, which touch IndexedDB.

| Method | What it does |
|---|---|
| `await Tesfix.open(name?, options?)` | Open a database. No name gives a temporary in-memory one. |
| `insert(collection, id, doc)` | Create a document, or replace the one with the same id. |
| `get(collection, id)` | The document, or `null`. |
| `patch(collection, id, changes)` | Merge changes in. `false` if the id does not exist. |
| `delete(collection, id)` | Remove a document. `false` if the id does not exist. |
| `find(collection, query?)` | Array of matching documents. |
| `count(collection, query?)` | Number of matching documents. |
| `createIndex(collection, field)` | Speed up queries on a field. |
| `transaction(fn)` | Run `fn(db)` all-or-nothing. |
| `await flush()` | Wait until everything is saved to IndexedDB. |
| `await close()` | Save and close. |

Ids are strings (numbers are converted). Documents must be JSON objects. Collections need no
setup: inserting into a new name creates it.

## Queries

```js
db.find('users', {
  where: [['age', '>=', 18], ['address.city', '=', 'Accra']],   // all must match
  order_by: [['age', 'desc'], ['name']],
  limit: 20,
  offset: 40,
});
```

- Operators: `=`, `!=`, `<`, `<=`, `>`, `>=`.
- Values must be strings, numbers or booleans. Fields may be nested with dots.
- Unknown keys in a query are an error, so a typo like `limt` cannot silently return everything.
- A document that does not have the field never matches, not even with `!=`.
- `3` and `"3"` are different values. Compare numbers with numbers.
- Without `order_by`, results are sorted by id **as text** (`"10"` comes before `"2"`).
- `createIndex('users', 'age')` makes `age` queries much faster. Index only the fields you search.

## Patching

`patch` follows JSON Merge Patch (RFC 7396):

```js
db.patch('users', '1', {
  age: 29,                   // replaced
  address: { city: 'Tema' }, // merged: other address keys survive
  nickname: null,            // removes the key
  tags: ['a'],               // arrays are replaced whole
});
```

Because `null` means "remove", a patch cannot set a field to `null`. Use `insert` to replace the
whole document if you need that.

## Transactions

```js
db.transaction((tx) => {
  tx.insert('orders', 'o1', { total: 40 });
  tx.patch('stock', 'apple', { count: 9 });
}); // commits here. If anything inside throws, everything is rolled back.
```

The callback must be synchronous. Wrapping many inserts in one transaction is also much faster.

## Errors

Every failure throws a `TesfixError` whose message says what went wrong. A missing id is not an
error: `get` returns `null`, and `patch` and `delete` return `false`.

## Limits (read this first)

This is an alpha. Please know these before you build on it:

- **Two tabs can overwrite each other.** Saved data uses IndexedDB without cross-tab locking, so
  if two tabs write to the same database, the tab that saves last wins and the other's writes are
  lost. Use one tab at a time for now.
- **A save happens about 300 ms after your last write.** If the tab closes sooner, those writes
  can be lost. Call `await db.flush()` after anything important.
- **Browsers can clear site storage** (low disk space, privacy settings, long inactivity in some
  browsers). Keep a server or exported copy of anything valuable.
- **Calls block the page.** Everything runs on the main thread, so a very slow query can freeze
  the UI. A Web Worker version is planned.
- **Whole-file saves.** The whole database file is rewritten on each save, so large databases
  (many megabytes) will be slow to save.
- **Number limits.** `count()` is exact up to 2,147,483,647. JavaScript numbers lose precision
  above 2^53, and `BigInt` values cannot be stored.

<!-- ## Building from source

You need CMake and the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html).

```bash
emcmake cmake -B build-wasm-web -DCMAKE_BUILD_TYPE=Release -DTESFIX_WASM_ENV=web,worker
cmake --build build-wasm-web

cd npm
npm run build      # copies the build into npm/dist
npm pack           # creates tesfix-0.1.0-alpha.0.tgz
``` -->

## License & Third-Party Notices

Copyright © 2026 Patrick Kwame Ayensu. All Rights Reserved.

Tesfix is distributed under the **Tesfix Proprietary Evaluation & Confidential Source License**. You are welcome to download, install, and test this release for personal, educational, and internal evaluation purposes. Commercial deployment, redistribution, or offering Tesfix as a hosted service requires a separate written commercial license. See [LICENSE](./LICENSE) for full terms.

Third-party components (SQLite and `nlohmann/json`) are listed with their respective licenses in [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md).
