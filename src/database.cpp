#include "database.hpp"

#include <fmt/std.h>
#include <giomm/resource.h>
#include <spdlog/spdlog.h>
#include <sqlite3.h>

#include <mutex>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "constants.hpp"

#define DDB_OWS_DATABASE_FNAME ".ddb_ows.json"
#define DDB_OWS_SQL_DATABASE_FNAME ".ddb_ows.sqlite3"
#define DDB_OWS_DATABASE_SCHEMA_VERSION 1

using namespace nlohmann;

namespace ddb_ows {

int execute_from_resource(
    sqlite3* db,
    const char* resource_name,
    int (*callback)(void*, int, char**, char**),
    void* user_data,
    char** errmsg
) {
    auto res = Gio::Resource::lookup_data_global(resource_name);
    auto size = res->get_size();
    auto buf = static_cast<const char*>(res->get_data(size));

    return sqlite3_exec(db, buf, callback, user_data, errmsg);
}

struct db_version_info_t {
    int schema_version;
    std::string app_version;
};

Database::Database(path root) : m() {
    logger = spdlog::get(DDB_OWS_PROJECT_ID);
    db_fname = root / DDB_OWS_SQL_DATABASE_FNAME;
    int status = sqlite3_open_v2(
        db_fname.c_str(),
        &sql_db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
        nullptr
    );
    if (status != SQLITE_OK) {
        const auto err_msg = fmt::format(
            "Unable to open database (errno {}: {})",
            status,
            sqlite3_errmsg(sql_db)
        );
        throw std::runtime_error(err_msg);
    }
    // Database opened successfully

    // Make sure a meta table exists and get the schema and app versions from
    // it.
    // Preparing the statement can't fail because we control it fully.
    db_version_info_t version_info;
    char* sqlite_errmsg;
    auto store_values =
        [](void* user_data, int n_cols, char** cols, char** col_names) -> int {
        auto out = static_cast<db_version_info_t*>(user_data);
        out->schema_version = atoi(cols[0]);
        out->app_version = std::string(cols[1]);
        return 0;
    };
    status = execute_from_resource(
        sql_db,
        "/ddb_ows/sql/init.sql",
        store_values,
        &version_info,
        &sqlite_errmsg
    );
    if (status != SQLITE_OK) {
        const auto err_msg = fmt::format(
            "Unable to determine schema and app version of database ({}: {})",
            status,
            sqlite3_errmsg(sql_db),
            sqlite_errmsg != nullptr ? sqlite_errmsg : "N/A"
        );
        sqlite3_free(sqlite_errmsg);
        throw std::runtime_error(err_msg);
    }

    logger->debug(
        "Determined schema version: {} (current schema version: {}) and {} "
        "version {} (current version: {})",
        version_info.schema_version,
        DDB_OWS_DATABASE_SCHEMA_VERSION,
        DDB_OWS_PROJECT_NAME,
        version_info.app_version,
        DDB_OWS_VERSION
    );

    if (version_info.schema_version > DDB_OWS_DATABASE_SCHEMA_VERSION) {
        const auto err_msg = fmt::format(
            "Database uses schema version {}, which is newer than current "
            "schema version: {}. Try upgrading to {}>={}",
            version_info.schema_version,
            DDB_OWS_DATABASE_SCHEMA_VERSION,
            DDB_OWS_PROJECT_NAME,
            version_info.app_version
        );
        throw std::runtime_error(err_msg);
    } else {
        for (unsigned int k = version_info.schema_version + 1;
             k <= DDB_OWS_DATABASE_SCHEMA_VERSION;
             k++)
        {
            auto res = fmt::format("/ddb_ows/sql/schema_v{}.sql", k);
            status = execute_from_resource(
                sql_db, res.c_str(), nullptr, nullptr, &sqlite_errmsg
            );
            if (status != SQLITE_OK) {
                auto err_msg = fmt::format(
                    "Unable to apply migration {} ({}: {})",
                    res,
                    sqlite3_errmsg(sql_db),
                    sqlite_errmsg != nullptr ? sqlite_errmsg : "N/A"
                );
                sqlite3_free(sqlite_errmsg);
                throw std::runtime_error(err_msg);
            }
            logger->debug("Applied database migration {}", res);
        }
    }

    // Prepare all the statements we will need later from embedded resources
    const std::vector<std::string> stmt_names{
        "latest_file_sync", "new_sync", "register_file", "register_synced_file"
    };
    for (const auto& n : stmt_names) {
        const auto resource_name = fmt::format("/ddb_ows/sql/{}.sql", n);
        const auto res = Gio::Resource::lookup_data_global(resource_name);
        auto size = res->get_size();
        const auto sql = static_cast<const char*>(res->get_data(size));

        sqlite3_stmt* stmt = nullptr;
        const char* tail;
        int status = sqlite3_prepare_v2(sql_db, sql, size, &stmt, &tail);
        if (status != SQLITE_OK) {
            const auto err_msg = fmt::format(
                "Could not prepare statement {}: {}",
                resource_name,
                sqlite3_errmsg(sql_db)
            );
            // no need to call sqlite3_finalize; stmt will be nullptr if an
            // error occurs
            throw std::runtime_error(err_msg);
        }
        statements.try_emplace(n, stmt, sqlite3_finalize);
    }
}

Database::~Database() {
    // We have to destruct statements before closing the database
    statements.clear();
    sqlite3_close(sql_db);

    logger->debug("Closed database {}.", db_fname);
}

// Convenience function to bind a text parameter from a std::string, or from
// anything that *can be converted* to a std::string -- the compiler will write
// the boilerplate for us. The default destructor is SQLITE_TRANSIENT because
// if an argument is converted, the resulting string's data only lives until
// the end of this function. Thus the default is always correct but sometimes
// inefficient. A caller passing a std::string known to live long enough can
// pass SQLITE_STATIC to avoid unnecessary copies.
int sqlite3_bind_str(
    sqlite3_stmt* stmt,
    const char* param,
    const std::string& str,
    void (*destructor)(void*) = SQLITE_TRANSIENT
) {
    auto idx = sqlite3_bind_parameter_index(stmt, param);
    return sqlite3_bind_text(stmt, idx, str.c_str(), str.length(), destructor);
}

// Get a prepared statement and make sure it is reset and ready to be used
sqlite3_stmt* Database::_get_statement(const std::string& name) {
    sqlite3_stmt* stmt = statements.at(name).get();
    sqlite3_reset(stmt);
    return stmt;
}

std::optional<sync_id_t> Database::new_sync(
    const std::string& fn_format,
    bool cover_sync,
    const std::optional<std::string>& cover_fname,
    bool rm_unref
) {
    sqlite3_stmt* stmt = _get_statement("new_sync");
    sqlite3_bind_str(stmt, ":ddb_ows_version", DDB_OWS_VERSION);
    sqlite3_bind_str(stmt, ":fn_format", fn_format);
    sqlite3_bind_int(
        stmt, sqlite3_bind_parameter_index(stmt, ":cover_sync"), cover_sync
    );
    sqlite3_bind_int(
        stmt, sqlite3_bind_parameter_index(stmt, ":rm_unref"), rm_unref
    );
    if (cover_fname) {
        sqlite3_bind_str(stmt, ":cover_fname", *cover_fname);
    } else {
        auto idx = sqlite3_bind_parameter_index(stmt, ":cover_fname");
        sqlite3_bind_null(stmt, idx);
    }
    int status = sqlite3_step(stmt);
    if (status != SQLITE_ROW) {
        logger->warn(
            "Could not create a new sync in database (errno {}: {})",
            status,
            sqlite3_errmsg(sql_db)
        );
        return std::nullopt;
    } else {
        return sqlite3_column_int64(stmt, 0);
    }
}

std::optional<synced_file_data_t> Database::find_entry(path key) {
    std::lock_guard lock(m);

    sqlite3_stmt* stmt = _get_statement("latest_file_sync");
    sqlite3_bind_str(stmt, ":source", key);

    int status = sqlite3_step(stmt);
    if (status == SQLITE_ROW) {
        const auto source =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));

        std::optional<path> destination;
        const auto dest =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (dest != nullptr) {
            destination = dest;
        }

        std::optional<std::string> conv_preset;
        const auto conv_preset_ =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (conv_preset_ != nullptr) {
            conv_preset = conv_preset_;
        }

        const uint64_t ts = sqlite3_column_int64(stmt, 3);
        const sync_id_t sync_id = sqlite3_column_int64(stmt, 4);

        return {{
            .sync_id = sync_id,
            .source = path(source),
            .destination = dest,
            .converter_preset = conv_preset,
            .timestamp = std::chrono::seconds(ts),
        }};
    } else if (status == SQLITE_DONE) {  // No data returned
        return std::nullopt;
    } else {
        logger->warn(
            "Could not query database for latest sync of {} (errno {}): {}",
            key,
            status,
            sqlite3_errmsg(sql_db)
        );
        return std::nullopt;
    }
}

void Database::register_file(const path& source) {
    sqlite3_stmt* stmt = _get_statement("register_file");

    sqlite3_bind_str(stmt, ":source", source);

    int status = sqlite3_step(stmt);
    if (status != SQLITE_DONE) {
        logger = spdlog::get(DDB_OWS_PROJECT_ID);
        logger->warn(
            "Could not register file {} (errno {}): {}",
            source,
            status,
            sqlite3_errmsg(sql_db)
        );
    }
}

void Database::register_synced_file(const synced_file_data_t& data) {
    std::lock_guard lock(m);

    sqlite3_stmt* stmt = _get_statement("register_synced_file");

    sqlite3_bind_str(stmt, ":source", data.source);
    if (data.destination) {
        sqlite3_bind_str(stmt, ":destination", *data.destination);
    }
    if (data.converter_preset) {
        sqlite3_bind_str(
            stmt, ":conv_preset", *data.converter_preset, SQLITE_STATIC
        );
    }
    sqlite3_bind_int64(
        stmt,
        sqlite3_bind_parameter_index(stmt, ":timestamp"),
        data.timestamp.count()
    );
    sqlite3_bind_int64(
        stmt, sqlite3_bind_parameter_index(stmt, ":sync_id"), data.sync_id
    );

    int status = sqlite3_step(stmt);
    if (status != SQLITE_DONE) {
        logger = spdlog::get(DDB_OWS_PROJECT_ID);
        logger->warn(
            "Could not register job for {} (errno {}): {}",
            data.source,
            status,
            sqlite3_errmsg(sql_db)
        );
    }
}

}  // namespace ddb_ows
