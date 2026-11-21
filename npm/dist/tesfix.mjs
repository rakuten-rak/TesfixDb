// tesfix for the browser: a friendly JavaScript layer over the C API
// that was compiled to WebAssembly (tesfix_wasm.mjs / tesfix_wasm.wasm).
//
//   const db = await Tesfix.open('myapp');        // saved in IndexedDB
//   db.insert('users', '1', { name: 'Ama', age: 28 });
//   db.find('users', { where: [['age', '>', 25]], order_by: [['age', 'desc']], limit: 10 });
//
// Calls are synchronous (SQLite runs inside the page). Only open() and
// flush()/close() are async, because they touch IndexedDB.

import createModule from "./tesfix_wasm.mjs";

const OK = 0;
const NOT_FOUND = 2;
const FLUSH_DELAY_MS = 300;

export class TesfixError extends Error {
  constructor(message) {
    super(message);
    this.name = "TesfixError";
  }
}

// ---------------------------------------------------------------------------
// One shared WebAssembly module per page. Every database lives in the same
// virtual file system, so they must share one module and one IndexedDB sync.
// ---------------------------------------------------------------------------
let modulePromise = null;
let storagePromise = null;
let flushDirty = false;
let flushTimer = null;
let flushRunning = null;

// options.wasmUrl lets an app say where tesfix_wasm.wasm is served from
// (a CDN, a /public folder, a bundler asset URL). Only the first call matters,
// because the module is loaded once per page.
function loadModule(options) {
  if (!modulePromise) {
    const settings = {};
    if (options && options.wasmUrl) {
      settings.locateFile = (file, scriptDirectory) =>
        file.endsWith(".wasm") ? options.wasmUrl : scriptDirectory + file;
    }
    modulePromise = createModule(settings).catch((e) => {
      modulePromise = null; // allow a retry, e.g. after a failed network fetch
      throw e;
    });
  }
  return modulePromise;
}

function syncFs(M, populate) {
  return new Promise((resolve, reject) => {
    M.FS.syncfs(populate, (err) => (err ? reject(err) : resolve()));
  });
}

// Mount IndexedDB-backed storage at /data and load what was saved last time.
function loadStorage(M) {
  if (typeof indexedDB === "undefined") {
    throw new TesfixError(
      "Saved databases need a browser with IndexedDB. " +
        "On the server (SSR) use Tesfix.open() with no name, or open the database inside the browser only."
    );
  }
  if (!storagePromise) {
    storagePromise = (async () => {
      M.FS.mkdir("/data");
      M.FS.mount(M.FS.filesystems.IDBFS, {}, "/data");
      await syncFs(M, true); // true = IndexedDB -> memory
    })();
  }
  return storagePromise;
}

// Save memory -> IndexedDB. Writes that arrive during a save trigger another round.
function flushNow(M) {
  if (!flushRunning) {
    flushRunning = (async () => {
      while (flushDirty) {
        flushDirty = false;
        try {
          await syncFs(M, false); // false = memory -> IndexedDB
        } catch (e) {
          flushDirty = true; // still unsaved
          throw e;
        }
      }
    })().finally(() => {
      flushRunning = null;
    });
  }
  return flushRunning;
}

function scheduleFlush(M) {
  flushDirty = true;
  if (flushTimer) return;
  flushTimer = setTimeout(() => {
    flushTimer = null;
    flushNow(M).catch((e) =>
      console.error("tesfix: saving to IndexedDB failed", e)
    );
  }, FLUSH_DELAY_MS);
}

// Copy JS strings into WebAssembly memory (as UTF-8), run fn with the
// pointers, then free them. null becomes a C NULL pointer.
// We use the heap, not ccall's 'string' type, because ccall copies onto the
// small stack and a large document would overflow it.
function withCStrings(M, strings, fn) {
  const pointers = [];
  try {
    for (const s of strings) {
      if (s === null || s === undefined) {
        pointers.push(0);
        continue;
      }
      const size = M.lengthBytesUTF8(s) + 1; // +1 for the terminating zero
      const p = M._malloc(size);
      M.stringToUTF8(s, p, size);
      pointers.push(p);
    }
    return fn(...pointers);
  } finally {
    for (const p of pointers) if (p) M._free(p);
  }
}

export class Tesfix {
  // Use `await Tesfix.open()`, not `new Tesfix()`.
  constructor(M, handle, persistent) {
    this.M = M;
    this.h = handle;
    this.persistent = persistent;
    this.slot = M._malloc(8); // reusable out-parameter: a pointer or a 64-bit count
  }

  // open()            -> temporary database in memory
  // open('myapp')     -> saved in the browser's IndexedDB under that name
  // options.wasmUrl: where to load tesfix_wasm.wasm from (see the README)
  static async open(name = null, options = {}) {
    const M = await loadModule(options);
    let path = ":memory:";

    if (name !== null) {
      if (!/^[A-Za-z0-9_][A-Za-z0-9_.-]*$/.test(name)) {
        throw new TesfixError(
          "Database name may only use letters, digits, _ . - and cannot start with . or -"
        );
      }
      await loadStorage(M);
      path = `/data/${name}`;
    }

    const slot = M._malloc(4);
    try {
      const status = withCStrings(M, [path], (p) => M._tesfix_open(p, slot));
      const handle = M.getValue(slot, "i32");
      if (status !== OK) {
        const message = handle
          ? M.UTF8ToString(M._tesfix_last_error(handle))
          : "Cannot allocate a database handle";
        if (handle) M._tesfix_close(handle);
        throw new TesfixError(message);
      }
      return new Tesfix(M, handle, name !== null);
    } finally {
      M._free(slot);
    }
  }

  // documents

  insert(collection, id, doc) {
    this._ensureOpen();
    const status = withCStrings(
      this.M,
      [collection, String(id), JSON.stringify(doc)],
      (c, i, d) => this.M._tesfix_insert(this.h, c, i, d)
    );
    this._check(status);
    this._wrote();
  }

  // returns the document, or null if the id does not exist
  get(collection, id) {
    this._ensureOpen();
    this.M.setValue(this.slot, 0, "i32");
    const status = withCStrings(this.M, [collection, String(id)], (c, i) =>
      this.M._tesfix_get(this.h, c, i, this.slot)
    );
    if (status === NOT_FOUND) return null;
    this._check(status);
    return JSON.parse(this._takeString());
  }

  // merge changes into a document; null removes a key. false if the id does not exist
  patch(collection, id, changes) {
    this._ensureOpen();
    const status = withCStrings(
      this.M,
      [collection, String(id), JSON.stringify(changes)],
      (c, i, p) => this.M._tesfix_patch(this.h, c, i, p)
    );
    if (status === NOT_FOUND) return false;
    this._check(status);
    this._wrote();
    return true;
  }

  // false if the id does not exist
  delete(collection, id) {
    this._ensureOpen();
    const status = withCStrings(this.M, [collection, String(id)], (c, i) =>
      this.M._tesfix_delete(this.h, c, i)
    );
    if (status === NOT_FOUND) return false;
    this._check(status);
    this._wrote();
    return true;
  }

  // query: { where: [[field, op, value], ...], order_by: [[field, 'desc'], ...], limit, offset }
  // omit it to get the whole collection. Returns an array of documents.
  find(collection, query = null) {
    this._ensureOpen();
    this.M.setValue(this.slot, 0, "i32");
    const status = withCStrings(
      this.M,
      [collection, query === null ? null : JSON.stringify(query)],
      (c, q) => this.M._tesfix_find(this.h, c, q, this.slot)
    );
    this._check(status);
    return JSON.parse(this._takeString());
  }

  // number of matching documents (limit and offset are ignored).
  // Reads the low 32 bits, so it is exact up to 2,147,483,647.
  count(collection, query = null) {
    this._ensureOpen();
    const status = withCStrings(
      this.M,
      [collection, query === null ? null : JSON.stringify(query)],
      (c, q) => this.M._tesfix_count(this.h, c, q, this.slot)
    );
    this._check(status);
    return this.M.getValue(this.slot, "i32");
  }

  createIndex(collection, field) {
    this._ensureOpen();
    const status = withCStrings(this.M, [collection, field], (c, f) =>
      this.M._tesfix_create_index(this.h, c, f)
    );
    this._check(status);
    this._wrote();
  }

  // Runs fn(db) inside a transaction: commits if fn returns, rolls back if it throws.
  // This is the JavaScript version of the C++ Transaction guard. fn must be synchronous.
  transaction(fn) {
    this._ensureOpen();
    this._check(this.M._tesfix_begin(this.h));

    let result;
    try {
      result = fn(this);
    } catch (e) {
      this.M._tesfix_rollback(this.h);
      throw e;
    }

    if (result && typeof result.then === "function") {
      this.M._tesfix_rollback(this.h);
      throw new TesfixError(
        "transaction() callback must be synchronous (do not use async/await inside it)"
      );
    }

    if (this.M._tesfix_commit(this.h) !== OK) {
      const message = this._lastError();
      this.M._tesfix_rollback(this.h);
      throw new TesfixError(message);
    }
    this._wrote();
    return result;
  }

  // ----- saving and closing --------------------------------------------

  // Wait until everything written so far is saved in IndexedDB.
  // (Writes are also saved automatically about 300 ms after the last one.)
  async flush() {
    if (!this.persistent) return;
    if (flushTimer) {
      clearTimeout(flushTimer);
      flushTimer = null;
    }
    await flushNow(this.M);
  }

  async close() {
    if (this.h === 0) return;
    this.M._tesfix_close(this.h);
    this.h = 0;
    this.M._free(this.slot);
    if (this.persistent) {
      flushDirty = true;
      await this.flush();
    }
  }

  // ----- internals -------------------------------------------------------

  _ensureOpen() {
    if (this.h === 0) throw new TesfixError("Database is closed");
  }

  _lastError() {
    return this.M.UTF8ToString(this.M._tesfix_last_error(this.h));
  }

  _check(status) {
    if (status !== OK) throw new TesfixError(this._lastError());
  }

  // Read the text the library returned through this.slot, then give it back.
  _takeString() {
    const p = this.M.getValue(this.slot, "i32");
    const text = this.M.UTF8ToString(p);
    this.M._tesfix_free(p);
    return text;
  }

  _wrote() {
    if (this.persistent) scheduleFlush(this.M);
  }
}
