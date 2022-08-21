#ifndef DDB_OWS_DATABASE_HPP
#define DDB_OWS_DATABASE_HPP

#define DDB_OWS_DATABASE_FNAME ".ddb_ows.json"

#include <unordered_map>
#include <filesystem>
#include <mutex>
#include <string>

using namespace std::filesystem;

namespace ddb_ows {
    typedef struct db_meta_s {
        std::string ver;
        uint64_t last_write;
    } db_meta_t;

    typedef struct db_entry_s {
        path destination;
        uint64_t timestamp;
        std::string converter_preset;
    } db_entry_t;

    typedef std::unordered_map<path, db_entry_t> entry_dict;

    typedef struct db_s {
        db_meta_t meta;
        entry_dict entries;
    } db_t;

    class Database {
        public:
            Database(path root);
            ~Database();
            bool write();
            int count(path);
            entry_dict::iterator find_entry(path);
            entry_dict::iterator begin();
            entry_dict::iterator end();
            void insert_or_update(path key, db_entry_t entry);
        private:
            std::mutex m;
            bool read();
            path fname;
            db_t db;
    };
}

#endif
