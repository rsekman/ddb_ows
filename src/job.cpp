#include "job.hpp"

#include <fmt/std.h>

#include <filesystem>
#include <optional>
#include <system_error>

using namespace std::filesystem;

namespace ddb_ows {

void clean_parents(path p) {
    std::error_code e;
    if (!is_directory(p, e)) {
        return;
    }
    if (remove(p, e)) {
        clean_parents(p.parent_path());
    }
}

std::chrono::seconds now() {
    using namespace std::chrono;
    return duration_cast<std::chrono::seconds>(system_clock::now().time_since_epoch());
}

bool CopyJob::run(bool dry) {
    bool success;
    std::string from_to_str = fmt::format("from {} to {}", source, destination);
    try {
        if (!dry) {
            create_directories(destination.parent_path());
            success = copy_file(source, destination, copy_options::update_existing);
            if (success) {
                register_job();
                logger->log("Copied {}.", from_to_str);
            } else {
                logger->err("Failed to copy {}.", from_to_str);
            }
        } else {
            success = true;
            logger->log("Would copy {}.", from_to_str);
        }
    } catch (filesystem_error& e) {
        logger->err("Failed to copy {}: {}.", from_to_str, e.what());
        success = false;
    }
    return success;
}

void CopyJob::register_job() {
    db->register_synced_file(
        {.sync_id = sync_id,
         .source = source,
         .destination = destination,
         .converter_preset = std::nullopt,
         .timestamp = now()}
    );
}

CopyJob::CopyJob(
    std::shared_ptr<Logger> _logger,
    DatabaseHandle _db,
    sync_id_t _sync_id,
    path _source,
    path _destination
) :
    Job(_logger, _db, _sync_id, _source, _destination) {};

MoveJob::MoveJob(
    std::shared_ptr<Logger> _logger,
    DatabaseHandle _db,
    sync_id_t _sync_id,
    path _source,
    path _old_destination,
    path _destination,
    std::optional<std::string> _converter_preset
) :
    Job(_logger, _db, _sync_id, _source, _destination),
    old_destination(_old_destination),
    converter_preset(_converter_preset) {};

bool MoveJob::run(bool dry) {
    std::string from_to_str =
        fmt::format("from {} to {} (original source: {})", old_destination, destination, source);
    bool success;
    try {
        if (!dry) {
            create_directories(destination.parent_path());
            rename(old_destination, destination);
            register_job();
            clean_parents(old_destination.parent_path());
            logger->log("Moved from {}.", from_to_str);
        } else {
            logger->log("Would move {}.", from_to_str);
        }
        success = true;
    } catch (filesystem_error& e) {
        logger->err("Failed to move {}: {}.", from_to_str, e.what());
        success = false;
    }
    return success;
}

void MoveJob::register_job() {
    // a move is registered as a delete followed by a recreation
    db->register_synced_file(
        {.sync_id = sync_id,
         .source = source,
         .destination = std::nullopt,
         .converter_preset = std::nullopt,
         .timestamp = now()}
    );
    db->register_synced_file(
        {.sync_id = sync_id,
         .source = source,
         .destination = destination,
         .converter_preset = converter_preset,
         .timestamp = now()}
    );
}

ConvertJob::~ConvertJob() { ddb->pl_item_unref(it); }

ConvertJob::ConvertJob(
    std::shared_ptr<Logger> _logger,
    DatabaseHandle _db,
    DB_functions_t* _ddb,
    ddb_converter_settings_t _settings,
    DB_playItem_t* _it,
    sync_id_t _sync_id,
    path _source,
    path _destination
) :
    Job(_logger, _db, _sync_id, _source, _destination),
    ddb(_ddb),
    settings(_settings),
    it(_it),
    pabort(0) {
    ddb->pl_item_ref(it);
}

bool ConvertJob::run(bool dry) {
    std::string from_to_str =
        fmt::format("{} using {} to {}", source, settings.encoder_preset->title, destination);
    if (!dry) {
        logger->verbose("Converting  {}.", from_to_str);
        auto* ddb_conv = reinterpret_cast<ddb_converter_t*>(ddb->plug_get_for_id("converter"));
        // TODO implement cancelling
        create_directories(destination.parent_path());
        int out = ddb_conv->convert2(&settings, it, std::string(destination).c_str(), &pabort);
        if (!out) {
            register_job();
            logger->log("Conversion of {} successful.", from_to_str);
        } else {
            logger->err("Converting {} failed.", from_to_str);
        }
        return out == 0;
    } else {
        logger->log("Would convert {}.", from_to_str);
        return true;
    }
}

void ConvertJob::register_job() {
    db->register_synced_file(
        {.sync_id = sync_id,
         .source = source,
         .destination = destination,
         .converter_preset = settings.encoder_preset->title,
         .timestamp = now()}
    );
}

void ConvertJob::abort() { pabort = 1; }

DeleteJob::DeleteJob(
    std::shared_ptr<Logger> _logger,
    DatabaseHandle _db,
    sync_id_t _sync_id,
    path _source,
    path _destination
) :
    Job(_logger, _db, _sync_id, _source, _destination) {};

bool DeleteJob::run(bool dry) {
    bool success;
    if (!dry) {
        try {
            success = remove(destination);
        } catch (filesystem_error& e) {
            logger->log("Failed to delete {}: {}.", destination, e.what());
            success = false;
        }
        if (success) {
            register_job();
            clean_parents(destination.parent_path());
            logger->log("Deleted {}.", destination);
        }
    } else {
        logger->log("Would delete {}.", destination);
        success = true;
    }
    return success;
}

void DeleteJob::register_job() {
    db->register_synced_file(
        {.sync_id = sync_id,
         .source = source,
         .destination = std::nullopt,
         .converter_preset = std::nullopt,
         .timestamp = now()}
    );
}

}  // namespace ddb_ows
