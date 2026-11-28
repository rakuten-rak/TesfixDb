/** Any value that can be stored in a document. */
export type JsonValue =
  | string
  | number
  | boolean
  | null
  | JsonValue[]
  | { [key: string]: JsonValue };

/** A plain JSON object. Documents must be objects (not arrays or single values). */
export type Document = { [key: string]: JsonValue };

/** Comparison operators allowed in a condition. */
export type Operator = "=" | "!=" | "<" | "<=" | ">" | ">=";

/**
 * [field, operator, value]. The field may be nested with dots: "address.city".
 * The value must be a string, number or boolean.
 */
export type Condition = [
  field: string,
  operator: Operator,
  value: string | number | boolean
];

/** [field] or [field, "asc" | "desc"]. The default direction is "asc". */
export type SortKey =
  | [field: string]
  | [field: string, direction: "asc" | "desc"];

export interface Query {
  /** Every condition must match (AND). */
  where?: Condition[];
  /** Sort keys; later keys break ties. Documents with equal keys are ordered by id. */
  order_by?: SortKey[];
  /** Maximum number of documents to return. */
  limit?: number;
  /** Number of matching documents to skip (for paging). */
  offset?: number;
}

export interface OpenOptions {
  /**
   * URL of tesfix_wasm.wasm, for apps where it is served from a different place than the
   * JavaScript (a CDN, a /public folder, a bundler asset URL).
   * Only the first open() call in a page uses it.
   */
  wasmUrl?: string;
}

/** Thrown for every failure; `message` explains what went wrong. */
export class TesfixError extends Error {
  name: "TesfixError";
}

export class Tesfix {
  /**
   * Open a database.
   * - `Tesfix.open()` gives a temporary database in memory.
   * - `Tesfix.open('myapp')` saves to the browser's IndexedDB under that name
   *   (letters, digits, `_`, `.` and `-`; it cannot start with `.` or `-`).
   *
   * Saved databases only work in a browser. Do not call this during server-side rendering.
   */
  static open(name?: string | null, options?: OpenOptions): Promise<Tesfix>;

  /** Create a document, or replace the one with the same id. */
  insert<T extends object = Document>(
    collection: string,
    id: string | number,
    doc: T
  ): void;

  /** The document, or `null` if the id does not exist. */
  get<T extends object = Document>(
    collection: string,
    id: string | number
  ): T | null;

  /**
   * Merge `changes` into an existing document (JSON Merge Patch, RFC 7396):
   * nested objects merge, arrays are replaced whole, and `null` removes a key.
   * Returns `false` if the id does not exist.
   */
  patch(
    collection: string,
    id: string | number,
    changes: { [key: string]: JsonValue }
  ): boolean;

  /** Returns `false` if the id does not exist. */
  delete(collection: string, id: string | number): boolean;

  /** Matching documents; the whole collection when `query` is omitted. */
  find<T extends object = Document>(
    collection: string,
    query?: Query | null
  ): T[];

  /** Number of matching documents. `order_by`, `limit` and `offset` are ignored. */
  count(collection: string, query?: Query | null): number;

  /** Speed up queries on a field (nested fields use dots, e.g. "address.city"). */
  createIndex(collection: string, field: string): void;

  /**
   * Run `fn` as one all-or-nothing unit: it commits when `fn` returns and rolls back
   * when `fn` throws. `fn` must be synchronous: do not use `async` or `await` inside it.
   */
  transaction<R>(fn: (db: Tesfix) => R): R;

  /**
   * Wait until everything written so far is saved in IndexedDB.
   * Writes are also saved automatically about 300 ms after the last one.
   */
  flush(): Promise<void>;

  /** Save and close. The object cannot be used afterwards. */
  close(): Promise<void>;
}
