#ifndef DDB_OWS_DATABASE_HPP
#define DDB_OWS_DATABASE_HPP

#include <spdlog/logger.h>
#include <sqlite3.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

using namespace std::filesystem;

namespace ddb_ows {

typedef uint64_t sync_id_t;

struct synced_file_data_t {
    sync_id_t sync_id;
    const path source;
    // nullopt represents that the file was deleted
    const std::optional<path> destination;
    const std::optional<std::string> converter_preset;
    std::chrono::seconds timestamp;
};

class Database {
  public:
    Database(path root);
    ~Database();

    std::optional<synced_file_data_t> find_entry(path);

    void register_file(const path& source);
    void register_synced_file(const synced_file_data_t& data);

    std::optional<sync_id_t> new_sync(
        const std::string& fn_format,
        bool cover_sync,
        const std::optional<std::string>& cover_fname,
        bool rm_unref
    );

  private:
    std::mutex m;
    std::shared_ptr<spdlog::logger> logger;

    sqlite3* sql_db;
    path db_fname;
    std::unordered_map<std::string, std::shared_ptr<sqlite3_stmt>> statements;

    sqlite3_stmt* _get_statement(const std::string& name);

  private:
    // Because of the mutex this class can neither be copied or moved, but we
    // need a non-default destructor for the sqlite pointer. By rule of five we
    // have to delete copy/move × constructor/assignment
    Database(const Database& other) = delete;
    Database(Database&& other) = delete;
    Database& operator=(const Database& other) = delete;
    Database& operator=(Database&& other) = delete;
};

typedef std::shared_ptr<Database> DatabaseHandle;

}  // namespace ddb_ows

#endif
