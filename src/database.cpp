#include "constants.hpp"
#include "database.hpp"
#include "log.hpp"

#include <ctime>
#include <exception>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>

using namespace nlohmann;

namespace ddb_ows {

/* void to_json(json& j, const db_meta_t& ed) {
    j = json{
        {"ver", ed.ver},
        {"last_write", ed.last_write}
    };
}

void from_json(const json& j, db_meta_t& e) {
    j.at("ver").get_to(e.ver);
    j.at("last_write").get_to(e.last_write);
}

void to_json(json& j, const db_entry_t& e) {
    j = json{
        {"destination", e.destination},
        {"timestamp", e.timestamp}
    };
}

void from_json(const json& j, db_entry_t& e) {
    j.at("destination").get_to(e.destination);
    j.at("timestamp").get_to(e.timestamp);
}

void to_json(json& j, const db_t db) {
    j = json {
        {"meta", db.meta},
        {"entries", db.entries}
    };
}

void from_json(const json& j, db_t db) {
    j.at("entries").get_to(db.entries);
    j.at("meta").get_to(db.meta);
}

*/

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(db_meta_t, ver, last_write)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(db_entry_t, destination, timestamp)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(db_t, meta, entries)

Database::Database(path root) {
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
        return true;
    } catch (std::ifstream::failure e) {
        DDB_OWS_WARN << "Could not read " << fname << "." << std::endl;
    } catch (json::exception e) {
        DDB_OWS_ERR << "Malformed database " << fname << ": " << e.what() << std::endl;
        DDB_OWS_ERR << "Malformed database " << j << std::endl;
    }
    db = db_t {
        .meta = db_meta_t {
            .ver = DDB_OWS_VERSION,
            .last_write = 0
        },
        .entries = entry_dict {}
    };
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
}

int Database::count(path key) {
   return db.entries.count(key);
}

entry_dict::iterator Database::begin() {
    return db.entries.begin();
}

entry_dict::iterator Database::end() {
    return db.entries.end();
}

entry_dict::iterator Database::find_entry(path key) {
    return db.entries.find(key);
}

void Database::insert_or_update(path key, db_entry_t entry) {
    db.entries.insert_or_assign(key, entry);
}

}
