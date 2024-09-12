#include "database.hpp"

#include <fmt/std.h>

#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>

#include "constants.hpp"
#include "log.hpp"

using namespace nlohmann;

namespace ddb_ows {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(db_meta_t, ver, last_write)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    db_entry_t, destination, timestamp, converter_preset
)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(db_t, meta, entries)

Database::Database(path root) : m() {
    fname = root / DDB_OWS_DATABASE_FNAME;
    read();
}

bool Database::read() {
    DDB_OWS_DEBUG("Reading database from {}.", fname);
    std::ifstream in_file;
    in_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    json j;
    try {
        in_file.open(fname, std::ifstream::in);
        in_file >> j;
        db = j;
        DDB_OWS_DEBUG("Successfully read database from {}.", fname);
        return true;
    } catch (std::ifstream::failure e) {
        DDB_OWS_WARN("Could not read {}.", fname);
    } catch (json::exception e) {
        DDB_OWS_ERR("Malformed database {}: {}.", fname, e.what());
    }
    db = db_t{
        .meta = db_meta_t{.ver = DDB_OWS_VERSION, .last_write = 0},
        .entries = entry_dict{}
    };
    DDB_OWS_DEBUG("Set default database.");
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
        DDB_OWS_ERR("Unable to write database to {}: {}.", fname, e.what());
    }
    DDB_OWS_DEBUG("Wrote database {}.", fname);
}

int Database::count(path key) {
    std::lock_guard lock(m);
    return db.entries.count(key);
}

entry_dict::iterator Database::begin() {
    std::lock_guard lock(m);
    return db.entries.begin();
}

entry_dict::iterator Database::end() {
    std::lock_guard lock(m);
    return db.entries.end();
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

DatabaseHandle::DatabaseHandle(path root) {
    db = std::make_shared<Database>(root);
}

Database& DatabaseHandle::operator*() const noexcept { return *db.get(); };
Database* DatabaseHandle::operator->() const noexcept { return db.get(); }

}  // namespace ddb_ows
