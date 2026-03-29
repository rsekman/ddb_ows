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
#include <string_view>
#include <unordered_map>

namespace ddb_ows {

using sync_id_t = uint64_t;

struct synced_file_data_t {
    using path = std::filesystem::path;
    sync_id_t sync_id;
    const path source;
    // nullopt represents that the file was deleted
    const std::optional<path> destination;
    const std::optional<std::string> converter_preset;
    std::chrono::seconds timestamp;
};

class Database {
    using path = std::filesystem::path;

  public:
    Database(path root);
    ~Database();

    std::optional<synced_file_data_t> find_entry(path);

    void register_file(const path& source);
    void register_synced_file(const synced_file_data_t& data);

    void register_playlist(std::string_view uuid, std::string_view title);
    void register_synced_playlist(std::string_view uuid, sync_id_t sync_id);
    void register_file_in_playlist(const path& source, std::string_view plt_uuid);
    void clear_playlist(std::string_view plt_uuid);

    std::optional<std::vector<std::tuple<path, path>>> get_unreferenced_files();

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

using DatabaseHandle = std::shared_ptr<Database>;

}  // namespace ddb_ows

#endif
