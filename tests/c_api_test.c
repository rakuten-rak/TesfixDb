/* Pure C on purpose: if this compiles and runs, the header really is C-safe. */
#include "tesfix/tesfix_c.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(int condition, const char *label)
{
      printf("%s: %s\n", condition ? "PASS" : "FAIL", label);
      if (!condition)
            failures++;
}

static int contains(const char *text, const char *needle)
{
      return text != NULL && strstr(text, needle) != NULL;
}

static int same(const char *a, const char *b)
{
      return a != NULL && strcmp(a, b) == 0;
}

static void release(char **text)
{
      tesfix_free(*text);
      *text = NULL;
}

static void remove_db_files(const char *path)
{
      char name[256];
      remove(path);
      snprintf(name, sizeof name, "%s-wal", path);
      remove(name);
      snprintf(name, sizeof name, "%s-shm", path);
      remove(name);
}

int main(void)
{
      tesfix_db *db = NULL;
      char *out = NULL;
      long long n = -1;

      check(tesfix_open(":memory:", &db) == TESFIX_OK && db != NULL, "open in-memory database");
      if (!db)
            return 1;
      check(strlen(tesfix_version()) > 0, "version string available");

      /* --- insert and get --- */
      check(tesfix_insert(db, "users", "1", "{\"name\":\"Ama\",\"age\":28}") == TESFIX_OK,
            "insert works on a fresh database (no create_collection needed)");
      check(tesfix_get(db, "users", "1", &out) == TESFIX_OK && contains(out, "Ama"),
            "get returns the document");
      release(&out);

      check(tesfix_get(db, "users", "999", &out) == TESFIX_NOT_FOUND && out == NULL,
            "get of a missing id -> NOT_FOUND, no output");
      check(same(tesfix_last_error(db), "Document not found"), "NOT_FOUND comes with a message");

      check(tesfix_insert(db, "users", "bad", "{not json") == TESFIX_ERROR &&
                contains(tesfix_last_error(db), "valid JSON"),
            "invalid JSON text rejected with a reason");
      check(tesfix_insert(db, "users", "bad", "[1,2]") == TESFIX_ERROR,
            "non-object document rejected");

      tesfix_insert(db, "users", "2", "{\"name\":\"Kofi\",\"age\":31}");
      tesfix_insert(db, "users", "3", "{\"name\":\"Esi\",\"age\":25}");

      /* --- find --- */
      check(tesfix_find(db, "users", NULL, &out) == TESFIX_OK && out[0] == '[' &&
                contains(out, "Ama") && contains(out, "Kofi") && contains(out, "Esi"),
            "find with NULL query returns everything as a JSON array");
      release(&out);

      check(tesfix_find(db, "users", "{\"where\":[[\"age\",\">\",26]]}", &out) == TESFIX_OK &&
                contains(out, "Ama") && contains(out, "Kofi") && !contains(out, "Esi"),
            "find with a where condition");
      release(&out);

      check(tesfix_find(db, "users", "{\"order_by\":[[\"age\",\"desc\"]],\"limit\":1}", &out) == TESFIX_OK &&
                contains(out, "Kofi") && !contains(out, "Ama"),
            "order_by desc + limit 1 returns the oldest");
      release(&out);

      check(tesfix_find(db, "users", "{\"where\":[[\"age\",\">\",100]]}", &out) == TESFIX_OK &&
                same(out, "[]"),
            "no matches gives an empty array, not an error");
      release(&out);

      check(tesfix_find(db, "users", "{\"where\":[[\"age\",\"LIKE\",1]]}", &out) == TESFIX_ERROR &&
                same(tesfix_last_error(db), "Unsupported comparison operator") && out == NULL,
            "bad operator -> ERROR with a reason");
      check(tesfix_find(db, "users", "{\"limt\":1}", &out) == TESFIX_ERROR &&
                contains(tesfix_last_error(db), "Unknown query key"),
            "a typo in the query is rejected");
      check(tesfix_find(db, "users", "{\"where\":[[\"age\",\">\"]]}", &out) == TESFIX_ERROR,
            "malformed condition rejected");
      check(tesfix_find(db, "users", "not json", &out) == TESFIX_ERROR,
            "query that is not JSON rejected");

      /* --- count --- */
      check(tesfix_count(db, "users", NULL, &n) == TESFIX_OK && n == 3, "count everything");
      check(tesfix_count(db, "users", "{\"where\":[[\"age\",\">\",26]],\"limit\":1}", &n) == TESFIX_OK && n == 2,
            "count with a condition ignores limit");
      check(tesfix_count(db, "users", "{\"where\":[[\"age\",\"??\",1]]}", &n) == TESFIX_ERROR,
            "count with a bad operator -> ERROR");

      /* --- patch and delete --- */
      check(tesfix_patch(db, "users", "1", "{\"age\":29}") == TESFIX_OK, "patch existing document");
      check(tesfix_get(db, "users", "1", &out) == TESFIX_OK && contains(out, "\"age\":29") &&
                contains(out, "Ama"),
            "patch changed one field and kept the rest");
      release(&out);
      check(tesfix_patch(db, "users", "404", "{\"age\":1}") == TESFIX_NOT_FOUND,
            "patch of a missing id -> NOT_FOUND");
      check(tesfix_patch(db, "users", "1", "42") == TESFIX_ERROR, "non-object patch rejected");

      check(tesfix_delete(db, "users", "2") == TESFIX_OK, "delete existing document");
      check(tesfix_delete(db, "users", "2") == TESFIX_NOT_FOUND, "delete again -> NOT_FOUND");

      /* --- index --- */
      check(tesfix_create_index(db, "users", "age") == TESFIX_OK, "create index");
      check(tesfix_create_index(db, "users", "age); DROP TABLE x; --") == TESFIX_ERROR,
            "hostile index field rejected");

      /* --- transactions --- */
      check(tesfix_begin(db) == TESFIX_OK, "begin");
      tesfix_insert(db, "tx", "1", "{\"a\":1}");
      check(tesfix_rollback(db) == TESFIX_OK, "rollback");
      check(tesfix_get(db, "tx", "1", &out) == TESFIX_NOT_FOUND, "rolled-back insert is gone");

      tesfix_begin(db);
      tesfix_insert(db, "tx", "2", "{\"a\":2}");
      check(tesfix_commit(db) == TESFIX_OK, "commit");
      check(tesfix_get(db, "tx", "2", &out) == TESFIX_OK, "committed insert survives");
      release(&out);

      /* --- misuse must never crash --- */
      check(tesfix_get(db, NULL, "1", &out) == TESFIX_ERROR, "NULL collection -> ERROR");
      check(tesfix_get(db, "users", "1", NULL) == TESFIX_ERROR, "NULL out-parameter -> ERROR");
      check(tesfix_insert(NULL, "users", "1", "{}") == TESFIX_ERROR, "NULL handle -> ERROR");
      check(strlen(tesfix_last_error(NULL)) > 0, "last_error works on a NULL handle");
      tesfix_free(NULL);
      tesfix_close(NULL);
      check(1, "free(NULL) and close(NULL) are harmless");

      tesfix_close(db);

      /* --- a real file: data must survive close and reopen --- */
      {
            const char *path = "tesfix_c_test.db";
            tesfix_db *a = NULL;
            tesfix_db *b = NULL;
            int wrote;
            int read_back;

            remove_db_files(path);
            wrote = tesfix_open(path, &a) == TESFIX_OK &&
                    tesfix_insert(a, "notes", "1", "{\"text\":\"hello\"}") == TESFIX_OK;
            tesfix_close(a);
            check(wrote, "write to a real file");

            read_back = tesfix_open(path, &b) == TESFIX_OK &&
                        tesfix_get(b, "notes", "1", &out) == TESFIX_OK && contains(out, "hello");
            release(&out);
            tesfix_close(b);
            check(read_back, "data survives close and reopen");
            remove_db_files(path);
      }

      {
            tesfix_db *bad = NULL;
            check(tesfix_open("/no/such/folder/x.db", &bad) == TESFIX_ERROR && bad != NULL &&
                      contains(tesfix_last_error(bad), "Cannot open"),
                  "failed open still returns a handle that explains why");
            tesfix_close(bad);
      }

      printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "SOME FAILED", failures,
             failures == 1 ? "" : "s");
      return failures == 0 ? 0 : 1;
}