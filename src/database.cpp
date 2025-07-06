#include "database.hpp"

#include <fmt/std.h>
#include <spdlog/spdlog.h>

#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>

#include "constants.hpp"

using namespace nlohmann;

namespace ddb_ows {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(db_meta_t, ver, last_write)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    db_entry_t, destination, timestamp, converter_preset
)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(db_t, meta, entries)

Database::Database(path root) : m() {
    logger = spdlog::get(DDB_OWS_PROJECT_ID);
    fname = root / DDB_OWS_DATABASE_FNAME;
    read();
}

bool Database::read() {
    logger->debug("Reading database from {}.", fname);
    std::ifstream in_file;
    in_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    json j;
    try {
        in_file.open(fname, std::ifstream::in);
        in_file >> j;
        db = j;
        logger->debug("Successfully read database from {}.", fname);
        return true;
    } catch (std::ifstream::failure e) {
        logger->warn("Could not read {}.", fname);
    } catch (json::exception e) {
        logger->error("Malformed database {}: {}.", fname, e.what());
    }
    db = db_t{
        .meta = db_meta_t{.ver = DDB_OWS_VERSION, .last_write = 0},
        .entries = entry_dict{}
    };
    logger->debug("Set default database.");
    return false;
}

Database::~Database() {
    std::ofstream out_file;
    out_file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    db.meta.last_write = std::time(nullptr);
    try {
        out_file.open(fname, std::ofstream::out);
        out_file << json(db);
    } catch (std::ofstream::failure e) {
        logger->error("Unable to write database to {}: {}.", fname, e.what());
    }
    logger->debug("Wrote database {}.", fname);
}

int Database::count(path key) {
    std::lock_guard lock(m);
    return db.entries.count(key);
}

std::optional<db_entry_t> Database::find_entry(path key) {
    std::lock_guard lock(m);
    auto res = db.entries.find(key);
    if (res != db.entries.end()) {
        return res->second;
    } else {
        return std::nullopt;
    }
}

void Database::insert_or_update(path key, db_entry_t entry) {
    std::lock_guard lock(m);
    db.entries.insert_or_assign(key, entry);
}

void Database::erase(path key) {
    std::lock_guard lock(m);
    db.entries.erase(key);
}

}  // namespace ddb_ows
