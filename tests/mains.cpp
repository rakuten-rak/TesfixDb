#include <iostream>
#include "doclite/doclite.hpp"
#include "json.hpp"

int main()
{
    std::cout << "--- starting Doclite integration test ---" << std::endl;

    // let initialize the new RAII database
    doclite::Database db;

    // attempting to create and open a local database file
    std::string db_patth = "test_library.db";
    std::cout << "Opening database file: " << db_patth << std::endl;

    if (!db.open(db_patth))
    {
        std::cerr << "Failed to open database file: " << db_patth << std::endl;
        return 1;
    }
    std::cout << "Database file opened successfully." << std::endl;

    // attempting to initialize a collection name
    std::string collection_name = "books";
    std::cout << "Creating collection: " << collection_name << "..." << std::endl;

    if (!db.create_collection(collection_name))
    {
        std::cerr << "Failed to create a collection: Error " << db.get_last_error() << std::endl;
        return 1;
    }

    // create a very rick c++ JSON Object natively
    nlohmann::json book_doc;
    book_doc["title"] = "The Great Lord";
    book_doc["author"] = "John Doe";
    book_doc["year"] = 2026;
    book_doc["attributes"] = {{"genre", "fiction"}, {"pages", 350}, {"language", "English"}};

    std::cout << "Inserting Document 'lotr_01' into collection books...:" << std::endl;

    if (!db.insert_document(collection_name, "lotr_01", book_doc))
    {
        std::cerr << "Failed to insert document into collection: Error " << db.get_last_error() << std::endl;
        return 1;
    }
    std::cout << "Document inserted successfully." << std::endl;
    // std::cout << "-- Doclite Test integratin completed successfully ---" << std::endl;

    // attempting to find the document by if
    std::cout << "Finding/Retrieving document 'lotr_01' in collection books..." << std::endl;
    auto fetch_doc = db.find_by_id(collection_name, "lotr_01");
    if (fetch_doc.has_value())
    {
        std::cout << "Document found: " << fetch_doc.value() << std::endl;
        // Access field natively using the nlohmann::json's clear interface
        std::cout << " Title: " << fetch_doc.value()["title"] << std::endl;
        std::cout << " Author: " << fetch_doc.value()["author"] << std::endl;
        std::cout << " Year: " << fetch_doc.value()["year"] << std::endl;
        std::cout << " Pages: " << fetch_doc.value()["attributes"]["pages"] << std::endl;
    }
    else
    {
        std::cerr << "Failed to find document 'lotr_01' in collection by id Error: " << db.get_last_error() << std::endl;
        return 1;
    }

    // Test missing document handling behaviour
    std::cout << "Searching for non existing document... 'fake_id' " << std::endl;
    auto missing_doc = db.find_by_id(collection_name, "fake_id");
    if (!missing_doc.has_value())
    {
        std::cout << "Correctly handled missing value scenario (return nulopt) " << std::endl;
    }
    else
    {
        std::cerr << "Error: Found a document that should not exist: " << missing_doc.value() << std::endl;
        return 1;
    }

    std::cout << "colletion created successfully." << collection_name << ".. and initialized with safe index." << std::endl;

    std::cout << "--- Doclite integration test completed successfully ---" << std::endl;
    return 0;
}