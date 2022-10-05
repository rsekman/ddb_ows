#include "constants.hpp"
#include "database.hpp"
#include "log.hpp"

#include <ctime>
#include <exception>
#include <fstream>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>

using namespace nlohmann;

namespace ddb_ows {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(db_meta_t, ver, last_write)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(db_entry_t, destination, timestamp, converter_preset)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(db_t, meta, entries)

Database::Database(path root) : m() {
    fname = root / DDB_OWS_DATABASE_FNAME;
    read();
}

bool Database::read() {
    DDB_OWS_DEBUG << "Reading database from " << fname << "." << std::endl;
    std::ifstream in_file;
    in_file.exceptions ( std::ifstream::failbit | std::ifstream::badbit );
    json j;
    try {
        in_file.open(fname, std::ifstream::in);
        in_file >> j;
        db = j;
        DDB_OWS_DEBUG << "Successfully read database from " << fname << "." << std::endl;
        return true;
    } catch (std::ifstream::failure e) {
        DDB_OWS_WARN << "Could not read " << fname << "." << std::endl;
    } catch (json::exception e) {
        DDB_OWS_ERR << "Malformed database " << fname << ": " << e.what() << std::endl;
    }
    db = db_t {
        .meta = db_meta_t {
            .ver = DDB_OWS_VERSION,
            .last_write = 0
        },
        .entries = entry_dict {}
    };
    DDB_OWS_DEBUG << "Set default database" << "." << std::endl;
    return false;
}

Database::~Database() {
    std::ofstream out_file;
    out_file.exceptions ( std::ofstream::failbit | std::ofstream::badbit );
    db.meta.last_write = std::time(nullptr);
    try {
        out_file.open(fname, std::ofstream::out);
        out_file << json ( db ) ;
    } catch (std::ofstream::failure e) {
        DDB_OWS_ERR
            << "Unable to write database to " << fname
            << ": " << e.what()
            << std::endl;
    }
    DDB_OWS_DEBUG << "Wrote database " << fname << std::endl;
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

entry_dict::iterator Database::find_entry(path key) {
    std::lock_guard lock(m);
    return db.entries.find(key);
}

void Database::insert_or_update(path key, db_entry_t entry) {
    std::lock_guard lock(m);
    db.entries.insert_or_assign(key, entry);
}

void Database::erase(path key) {
    std::lock_guard lock(m);
    db.entries.erase(key);
}

}
