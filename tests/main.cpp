#include <iostream>
#include <vector>
#include <string>
#include <chrono>

#include "tesfix/tesfix.hpp"

int main()
{
    tesfix::Database db;

    if (!db.open(":memory:"))
    {
        std::cerr << "Error: Failed to open Database: \n";
        return 1;
    }

    db.create_collection("users");

    nlohmann::json user = {{"name", "Ama"}, {"age", 28}, {"tags", {"admin", "dev"}}};
    db.insert_document("users", "1", user);

    // same id but collection must not overwrite the user
    db.insert_document("orders", "1", {{"item", "book"}});

    auto found = db.find_by_id("users", "1");
    if (found && (*found)["name"] == "Ama")
    {
        std::cout << "Pass: Found user Ama in users collection " << found->dump() << std::endl;
    }
    else
    {
        std::cerr << "Fail: User Ama not found in users collection" << std::endl;
        return 1;
    }
    if (!db.find_by_id("users", "999"))
    {
        std::cout << "Pass: missing id return nullopt\n";
    }

    // find all and delete documents
    db.insert_document("users", "2", {{"name", "Bob"}, {"age", 35}});
    db.insert_document("users", "3", {{"name", "Charlie"}, {"age", 22}});

    auto all_users = db.find_all("users");
    std::cout << "All users: " << (all_users.size() == 3 ? "Pass" : "Fail") << ": Find all returned " << all_users.size() << " users" << std::endl;

    // order collection must stay seperate
    std::cout << (db.find_all("orders").size() == 1 ? "Pass" : "Fail;") << ": orders collection isolated\n";

    std::cout << (db.delete_document("users", "2") ? "Pass" : "Fail") << ": delete existing document with id 2" << std::endl;
    std::cout << (db.delete_document("users", "2") ? "Fail" : "Pass") << ": delete again returns false" << std::endl;

    std::cout << (db.find_by_id("users", "2") ? "Fail" : "Pass") << ":deleted document with id 2 is gone\n";
    std::cout << (db.find_all("users").size() == 2 ? "Pass" : "Fail") << ": Exact 2 users remain\n";

    // adding a test after the refactor
    //
    std::cout << (db.find_all("no_such_collection").empty() ? "PASS" : "FAIL")
              << ": empty collection returns empty vector\n";

    // empty id should be stored and found like any other string
    db.insert_document("users", "", {{"name", "Empty"}});
    auto empty_id = db.find_by_id("users", "");
    std::cout << (empty_id && (*empty_id)["name"] == "Empty" ? "PASS" : "FAIL")
              << ": empty id works\n";

    // upsert: same id again should replace, not duplicate
    db.insert_document("users", "1", {{"name", "Ama Updated"}});
    auto updated = db.find_by_id("users", "1");
    std::cout << (updated && (*updated)["name"] == "Ama Updated" ? "PASS" : "FAIL")
              << ": upsert overwrites\n";
    //
    //
    db.insert_document("people", "1", {{"name", "Ama"}, {"age", 28}, {"active", true}, {"address", {{"city", "Accra"}}}});
    db.insert_document("people", "2", {{"name", "Kofi"}, {"age", 31}, {"active", false}, {"address", {{"city", "Kumasi"}}}});
    db.insert_document("people", "3", {{"name", "Esi"}, {"age", 28}, {"active", true}, {"address", {{"city", "Accra"}}}});

    std::cout << (db.find_where("people", "age", 28).size() == 2 ? "PASS" : "FAIL")
              << ": number match (age 28)\n";

    auto by_name = db.find_where("people", "name", "Kofi");
    std::cout << (by_name.size() == 1 && by_name[0]["age"] == 31 ? "PASS" : "FAIL")
              << ": string match (name Kofi)\n";

    std::cout << (db.find_where("people", "active", true).size() == 2 ? "PASS" : "FAIL")
              << ": bool match (active true)\n";

    std::cout << (db.find_where("people", "address.city", "Accra").size() == 2 ? "PASS" : "FAIL")
              << ": nested field (address.city)\n";

    std::cout << (db.find_where("people", "age", "28").empty() ? "PASS" : "FAIL")
              << ": text \"28\" does not match number 28\n";

    std::cout << (db.find_where("people", "salary", 100).empty() ? "PASS" : "FAIL")
              << ": missing field returns empty\n";

    std::cout << (db.find_where("users", "age", 28).empty() ? "PASS" : "FAIL")
              << ": other collections are not searched\n";

    // hostile / bad path field name must not break anything
    std::cout << (db.find_where("people", "age') OR 1=1 --", 28).empty() ? "PASS" : "FAIL")
              << ": injection attempt returns nothing\n";

    // create index - idx (Transaction) rollback, egin, and commit
    //  transactions
    db.begin();
    db.insert_document("people", "99", {{"name", "Temp"}});
    db.rollback();
    std::cout << (!db.find_by_id("people", "99") ? "PASS" : "FAIL")
              << ": rollback discards the insert\n";

    db.begin();
    db.insert_document("people", "98", {{"name", "Kept"}});
    db.commit();
    std::cout << (db.find_by_id("people", "98") ? "PASS" : "FAIL")
              << ": commit keeps the insert\n";

    //  index safety
    std::cout << (db.create_index("people", "age") ? "PASS" : "FAIL")
              << ": create_index on a plain field\n";
    std::cout << (db.create_index("people", "address.city") ? "PASS" : "FAIL")
              << ": create_index on a nested field\n";
    std::cout << (!db.create_index("people", "age); DROP TABLE doclite_collections; --") ? "PASS" : "FAIL")
              << ": hostile field name rejected\n";
    std::cout << (!db.create_index("bad name", "age") ? "PASS" : "FAIL")
              << ": hostile collection name rejected\n";
    std::cout << (!db.create_index("people", "a..b") ? "PASS" : "FAIL")
              << ": malformed path rejected\n";
    std::cout << (db.find_all("people").size() >= 3 ? "PASS" : "FAIL")
              << ": table still intact\n";

    // how create index improves the faster transactions
    db.begin(); // 20,000 inserts in ONE transaction
    for (int i = 0; i < 20000; ++i)
        db.insert_document("big", std::to_string(i), {{"n", i}});
    db.commit();

    auto time_lookups = [&db]()
    {
        auto start = std::chrono::steady_clock::now();
        size_t found_count = 0;
        for (int i = 0; i < 50; ++i)
            found_count = db.find_where("big", "n", 12345).size();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - start)
                      .count();
        std::cout << "  50 lookups took " << ms << " ms (found " << found_count << ")\n";
    };

    std::cout << "Without index:\n";
    time_lookups();
    db.create_index("big", "n");
    std::cout << "With index:\n";
    time_lookups();

    // find
    // .where -> limit, offset, and conditions.
    const char *cities[] = {"Accra", "Kumasi", "Accra", "Tamale", "Accra"};
    for (int i = 1; i <= 5; ++i)
        db.insert_document("staff", std::to_string(i),
                           {{"age", 15 + i * 5}, {"city", cities[i - 1]}}); // ages 20,25,30,35,40

    using tesfix::Query;

    std::cout << (db.find(Query("staff").where("age", ">", 25)).size() == 3 ? "PASS" : "FAIL")
              << ": age > 25\n";
    std::cout << (db.find(Query("staff").where("age", ">=", 25)).size() == 4 ? "PASS" : "FAIL")
              << ": age >= 25\n";
    std::cout << (db.find(Query("staff").where("age", "<", 30)).size() == 2 ? "PASS" : "FAIL")
              << ": age < 30\n";
    std::cout << (db.find(Query("staff").where("age", "!=", 30)).size() == 4 ? "PASS" : "FAIL")
              << ": age != 30\n";

    std::cout << (db.find(Query("staff").where("age", ">=", 25).where("age", "<=", 35)).size() == 3 ? "PASS" : "FAIL")
              << ": range 25..35 (two conditions on one field)\n";

    std::cout << (db.find(Query("staff").where("city", "=", "Accra").where("age", ">", 25)).size() == 2 ? "PASS" : "FAIL")
              << ": city Accra AND age > 25\n";

    auto page1 = db.find(Query("staff").limit(2));
    std::cout << (page1.size() == 2 && page1[0]["age"] == 20 ? "PASS" : "FAIL")
              << ": limit 2 (first page)\n";

    auto page2 = db.find(Query("staff").limit(2).offset(2));
    std::cout << (page2.size() == 2 && page2[0]["age"] == 30 ? "PASS" : "FAIL")
              << ": limit 2 offset 2 (second page)\n";

    std::cout << (db.find(Query("staff").offset(3)).size() == 2 ? "PASS" : "FAIL")
              << ": offset without limit\n";

    std::cout << (db.find(Query("staff")).size() == 5 ? "PASS" : "FAIL")
              << ": no conditions returns the whole collection\n";

    std::cout << (db.find(Query("staff").where("age", "; DROP TABLE x; --", 1)).empty() ? "PASS" : "FAIL")
              << ": hostile operator rejected\n";
    std::cout << (db.find(Query("staff").where("age", "LIKE", 1)).empty() ? "PASS" : "FAIL")
              << ": unsupported operator rejected\n";
    std::cout << (db.find_all("staff").size() == 5 ? "PASS" : "FAIL")
              << ": table still intact\n";

    // tests: patch
    db.insert_document("profiles", "1", {{"name", "Ama"}, {"age", 28}, {"address", {{"city", "Accra"}, {"zip", "GA1"}}}, {"tags", {"a", "b"}}});

    std::cout << (db.patch_document("profiles", "1", {{"age", 29}}) ? "PASS" : "FAIL")
              << ": patch returns true for existing document\n";

    auto p = db.find_by_id("profiles", "1");
    std::cout << (p && (*p)["age"] == 29 && (*p)["name"] == "Ama" ? "PASS" : "FAIL")
              << ": changed field updated, other fields untouched\n";

    db.patch_document("profiles", "1", {{"address", {{"city", "Kumasi"}}}});
    p = db.find_by_id("profiles", "1");
    std::cout << (p && (*p)["address"]["city"] == "Kumasi" && (*p)["address"]["zip"] == "GA1" ? "PASS" : "FAIL")
              << ": nested patch keeps sibling keys\n";

    db.patch_document("profiles", "1", {{"email", "ama@example.com"}});
    p = db.find_by_id("profiles", "1");
    std::cout << (p && (*p)["email"] == "ama@example.com" ? "PASS" : "FAIL")
              << ": patch adds a new field\n";

    db.patch_document("profiles", "1", {{"email", nullptr}});
    p = db.find_by_id("profiles", "1");
    std::cout << (p && !p->contains("email") ? "PASS" : "FAIL")
              << ": null removes a field\n";

    db.patch_document("profiles", "1", {{"tags", {"x"}}});
    p = db.find_by_id("profiles", "1");
    std::cout << (p && (*p)["tags"].size() == 1 && (*p)["tags"][0] == "x" ? "PASS" : "FAIL")
              << ": arrays are replaced, not merged\n";

    std::cout << (!db.patch_document("profiles", "404", {{"age", 1}}) ? "PASS" : "FAIL")
              << ": missing id returns false\n";
    std::cout << (!db.patch_document("nope", "1", {{"age", 1}}) ? "PASS" : "FAIL")
              << ": missing collection returns false\n";
    std::cout << (!db.patch_document("profiles", "1", 42) ? "PASS" : "FAIL")
              << ": non-object patch rejected\n";

    p = db.find_by_id("profiles", "1");
    std::cout << (p && (*p)["age"] == 29 ? "PASS" : "FAIL")
              << ": rejected patches changed nothing\n";

    // patch works with indexes and queries
    db.create_index("profiles", "age");
    db.patch_document("profiles", "1", {{"age", 30}});
    std::cout << (db.find_where("profiles", "age", 30).size() == 1 &&
                          db.find_where("profiles", "age", 29).empty()
                      ? "PASS"
                      : "FAIL")
              << ": queries see the patched value\n";

    // patch inside a transaction can be rolled back
    db.begin();
    db.patch_document("profiles", "1", {{"age", 99}});
    db.rollback();
    p = db.find_by_id("profiles", "1");
    std::cout << (p && (*p)["age"] == 30 ? "PASS" : "FAIL")
              << ": rollback undoes a patch\n";

    // find, order_y, count
    // --- Step 9 tests: count ---
    std::cout << (db.count(Query("staff")) == 5 ? "PASS" : "FAIL")
              << ": count whole collection\n";
    std::cout << (db.count(Query("staff").where("age", ">", 25)) == 3 ? "PASS" : "FAIL")
              << ": count with a condition\n";
    std::cout << (db.count(Query("staff").where("city", "=", "Accra").where("age", ">", 25)) == 2 ? "PASS" : "FAIL")
              << ": count with two conditions\n";
    std::cout << (db.count(Query("staff").limit(2).offset(1)) == 5 ? "PASS" : "FAIL")
              << ": count ignores limit and offset\n";
    std::cout << (db.count(Query("no_such_collection")) == 0 ? "PASS" : "FAIL")
              << ": count of nothing is 0\n";
    std::cout << (db.count(Query("staff").where("age", "LIKE", 1)) == -1 ? "PASS" : "FAIL")
              << ": count with bad operator returns -1\n";

    // --- Step 9 tests: order_by ---
    auto oldest_first = db.find(Query("staff").order_by("age", true));
    std::cout << (oldest_first.size() == 5 && oldest_first[0]["age"] == 40 && oldest_first[4]["age"] == 20 ? "PASS" : "FAIL")
              << ": order_by age descending\n";

    auto youngest_first = db.find(Query("staff").order_by("age"));
    std::cout << (youngest_first[0]["age"] == 20 && youngest_first[4]["age"] == 40 ? "PASS" : "FAIL")
              << ": order_by age ascending\n";

    auto by_city_then_age = db.find(Query("staff").order_by("city").order_by("age", true));
    std::cout << (by_city_then_age[0]["city"] == "Accra" && by_city_then_age[0]["age"] == 40 &&
                          by_city_then_age[2]["age"] == 20
                      ? "PASS"
                      : "FAIL")
              << ": two sort keys (city, then age descending)\n";

    auto top2 = db.find(Query("staff").order_by("age", true).limit(2));
    std::cout << (top2.size() == 2 && top2[0]["age"] == 40 && top2[1]["age"] == 35 ? "PASS" : "FAIL")
              << ": top 2 oldest\n";

    auto filtered_sorted = db.find(Query("staff").where("age", ">", 20).order_by("age", true));
    std::cout << (filtered_sorted.size() == 4 && filtered_sorted[0]["age"] == 40 ? "PASS" : "FAIL")
              << ": where + order_by together\n";

    // the "10 sorts before 2" problem from step 7, now solved
    db.insert_document("items", "1", {{"score", 5}});
    db.insert_document("items", "2", {{"score", 50}});
    db.insert_document("items", "10", {{"score", 7}});

    auto by_id = db.find(Query("items"));
    auto by_score = db.find(Query("items").order_by("score"));
    std::cout << (by_id[1]["score"] == 7 ? "PASS" : "FAIL")
              << ": default order is by id text (1, 10, 2)\n";
    std::cout << (by_score[0]["score"] == 5 && by_score[1]["score"] == 7 && by_score[2]["score"] == 50 ? "PASS" : "FAIL")
              << ": order_by score is numeric (5, 7, 50)\n";

    // a hostile sort field becomes a harmless bound path (it just matches no field)
    auto hostile = db.find(Query("staff").order_by("age); DROP TABLE x; --"));
    std::cout << (hostile.size() == 5 ? "PASS" : "FAIL")
              << ": hostile sort field is inert\n";
    std::cout << (db.find_all("staff").size() == 5 ? "PASS" : "FAIL")
              << ": table still intact\n";
    // handling proper error exception for the lib
    // Transaction guard
    {
        tesfix::Transaction tx(db);
        db.insert_document("tx_test", "1", {{"a", 1}});
    } // leaves scope without commit()
    std::cout << (!db.find_by_id("tx_test", "1") ? "PASS" : "FAIL")
              << ": no commit -> rolled back automatically\n";

    {
        tesfix::Transaction tx(db);
        db.insert_document("tx_test", "2", {{"a", 2}});
        bool first = tx.commit();
        bool second = tx.commit();
        std::cout << (first && !second ? "PASS" : "FAIL") << ": commit works once\n";
    }
    std::cout << (db.find_by_id("tx_test", "2") ? "PASS" : "FAIL")
              << ": committed data survives the guard\n";

    // early return: the guard cleans up without any manual rollback
    auto failing_batch = [&db]() -> bool
    {
        tesfix::Transaction tx(db);
        db.insert_document("tx_test", "3", {{"a", 3}});
        if (!db.patch_document("tx_test", "missing", {{"a", 1}}))
            return false; // early exit
        return tx.commit();
    };
    std::cout << (!failing_batch() && !db.find_by_id("tx_test", "3") ? "PASS" : "FAIL")
              << ": early return rolls the whole batch back\n";

    // a guard inside another transaction must not disturb the outer one
    {
        tesfix::Transaction outer(db);
        db.insert_document("tx_test", "4", {{"a", 4}});
        {
            tesfix::Transaction inner(db);
            std::cout << (!inner.ok() ? "PASS" : "FAIL") << ": nested guard reports not ok\n";
        }
        std::cout << (outer.commit() ? "PASS" : "FAIL") << ": outer transaction still commits\n";
    }
    std::cout << (db.find_by_id("tx_test", "4") ? "PASS" : "FAIL")
              << ": outer data kept after inner guard ended\n";

    // error reporting
    db.find_by_id("users", "1");
    std::cout << (db.get_last_error().empty() ? "PASS" : "FAIL")
              << ": success leaves no error\n";

    db.find_by_id("users", "no_such_id");
    std::cout << (db.get_last_error().empty() ? "PASS" : "FAIL")
              << ": not found is not an error\n";

    tesfix::Database closed_db;
    std::cout << (!closed_db.insert_document("x", "1", {{"a", 1}}) &&
                          closed_db.get_last_error().find("not open") != std::string::npos
                      ? "PASS"
                      : "FAIL")
              << ": closed database explains itself\n";

    db.execute("SELEKT 1;");
    std::cout << (db.get_last_error().find("syntax") != std::string::npos ? "PASS" : "FAIL")
              << ": bad SQL reports a syntax error\n";

    db.execute("INSERT INTO doclite_collections (collection_name, id, data) VALUES ('bad', '1', 'not json');");
    std::cout << (db.get_last_error().find("constraint") != std::string::npos ? "PASS" : "FAIL")
              << ": invalid JSON rejected by CHECK constraint\n";

    db.find(Query("staff").where("age", "LIKE", 1));
    std::cout << (db.get_last_error() == "Unsupported comparison operator" ? "PASS" : "FAIL")
              << ": bad operator explains itself\n";

    std::cout << (!db.delete_document("users", "no_such_id") && db.get_last_error() == "Document not found" ? "PASS" : "FAIL")
              << ": delete of missing id says why\n";

    std::cout << (!db.insert_document("users", "bad_utf8", {{"name", "\xff\xfe"}}) &&
                          !db.get_last_error().empty()
                      ? "PASS"
                      : "FAIL")
              << ": invalid UTF-8 returns false instead of throwing\n";

    {
        tesfix::Transaction tx(db);
        db.patch_document("users", "404", {{"a", 1}}); // fails
    }
    std::cout << (db.get_last_error() == "Document not found" ? "PASS" : "FAIL")
              << ": guard's rollback does not wipe the error\n";

    db.insert_document("users", "1", {{"name", "Again"}});
    std::cout << (db.get_last_error().empty() ? "PASS" : "FAIL")
              << ": next successful call clears the error\n";
}