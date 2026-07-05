#include <iostream>
#include <iomanip>
#include <thread>

#include <casket/db/txt/txt_database.hpp>
#include <casket/db/txt/txt_statement.hpp>
#include <casket/db/field_extractor.hpp>

int main()
{
    using namespace casket::db;

    try
    {
        std::vector<std::type_index> types = {typeid(unsigned int),
                                              typeid(unsigned int),
                                              typeid(std::chrono::system_clock::time_point),
                                              typeid(std::chrono::system_clock::time_point),
                                              typeid(std::string),
                                              typeid(std::vector<uint8_t>)};

        TxtDatabase db(types);

        db.createIndex(0);

        auto now = std::chrono::system_clock::now();
        auto tomorrow = now + std::chrono::hours(24);
        auto twoHoursAgo = now - std::chrono::hours(2);

        std::vector<uint8_t> binaryData1 = {0x01, 0x02, 0x03, 0x04, 0x05};
        std::vector<uint8_t> binaryData2 = {0xFF, 0xFE, 0xFD, 0xFC};
        std::vector<uint8_t> binaryData3 = {0x00, 0x00, 0x00, 0x01};

        db.insert(1u, 100u, now, tomorrow, std::string("Task 1"), binaryData1);

        db.insert(2u, 200u, twoHoursAgo, now, std::string("Task 2"), binaryData2);

        db.insert(
            3u, 150u, now - std::chrono::hours(1), now + std::chrono::hours(5), std::string("Task 3"), binaryData3);

        db.insert(4u,
                  300u,
                  now + std::chrono::hours(2),
                  now + std::chrono::hours(4),
                  std::string("Task 4"),
                  std::vector<uint8_t>());

        std::cout << "=== All Records ===" << std::endl;
        for (size_t i = 0; i < db.size(); ++i)
        {
            const auto& row = db.getRow(i);
            std::cout << "ID: " << extractField<unsigned int>(row, 0)
                      << ", Value: " << extractField<unsigned int>(row, 1)
                      << ", Name: " << extractField<std::string>(row, 4)
                      << ", Binary size: " << extractField<std::vector<uint8_t>>(row, 5).size() << std::endl;
        }

        auto stmt = db.createStatement();
        stmt->whereBetween(1, makeFieldValue(120u), makeFieldValue(250u));
        stmt->orderBy(1, true);
        stmt->spin();

        std::cout << "\n=== Records with value between 120 and 250 ===" << std::endl;
        while (stmt->step())
        {
            auto row = stmt->current();
            std::cout << "ID: " << extractField<unsigned int>(*row, 0)
                      << ", Value: " << extractField<unsigned int>(*row, 1)
                      << ", Name: " << extractField<std::string>(*row, 4) << std::endl;
        }

        stmt->clear();
        stmt->where(
            [](const IRow& row)
            {
                auto start = row.getField(2);
                auto end = row.getField(3);
                if (!start || start->isNull() || !end || end->isNull())
                    return false;

                auto startTp = extractField<std::chrono::system_clock::time_point>(row, 2);
                auto endTp = extractField<std::chrono::system_clock::time_point>(row, 3);
                auto now = std::chrono::system_clock::now();

                return startTp <= now && now <= endTp;
            });
        stmt->spin();

        std::cout << "\n=== Active Tasks ===" << std::endl;
        while (stmt->step())
        {
            auto row = stmt->current();
            auto start = extractField<std::chrono::system_clock::time_point>(*row, 2);
            auto end = extractField<std::chrono::system_clock::time_point>(*row, 3);

            std::cout << "Name: " << extractField<std::string>(*row, 4)
                      << ", Start: " << start.time_since_epoch().count() << ", End: " << end.time_since_epoch().count()
                      << std::endl;
        }

        db.writeToFile("database_with_types.txt");

        std::cout << "\n=== Statistics ===" << std::endl;
        auto stats = db.createStatement();
        stats->spin();
        std::cout << "Total: " << stats->count() << std::endl;
        std::cout << "Avg value: " << stats->avg(1) << std::endl;
        std::cout << "Max value: " << stats->max(1) << std::endl;
        std::cout << "Min value: " << stats->min(1) << std::endl;

        std::cout << "\n=== Loading from file ===" << std::endl;
        TxtDatabase loadedDb(types);
        loadedDb.readFromFile("database_with_types.txt");
        loadedDb.createIndex(0);
        std::cout << "Loaded " << loadedDb.size() << " records" << std::endl;

        for (size_t i = 0; i < loadedDb.size(); ++i)
        {
            const auto& row = loadedDb.getRow(i);
            std::cout << "ID: " << extractField<unsigned int>(row, 0) << ", Name: " << extractField<std::string>(row, 4)
                      << ", Binary size: " << extractField<std::vector<uint8_t>>(row, 5).size() << std::endl;
        }

        auto loadedRow = loadedDb.findByIndex(0, makeFieldValue(2u));
        if (loadedRow)
        {
            auto binary = extractField<std::vector<uint8_t>>(*loadedRow, 5);
            std::cout << "\n=== Binary data for ID 2 ===" << std::endl;
            std::cout << "Size: " << binary.size() << std::endl;
            std::cout << "Hex: ";
            for (uint8_t b : binary)
            {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
            }
            std::cout << std::dec << std::endl;
        }
    }
    catch (const Exception& e)
    {
        std::cerr << "Database error: " << e.what() << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}