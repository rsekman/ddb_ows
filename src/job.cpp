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
    return duration_cast<std::chrono::seconds>(
        system_clock::now().time_since_epoch()
    );
}

bool CopyJob::run(bool dry) {
    bool success;
    std::string from_to_str = fmt::format("from {} to {}", from, to);
    try {
        if (!dry) {
            create_directories(to.parent_path());
            success = copy_file(from, to, copy_options::update_existing);
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
         .source = from,
         .destination = to,
         .converter_preset = std::nullopt,
         .timestamp = now()}
    );
}

CopyJob::CopyJob(
    std::shared_ptr<Logger> _logger,
    DatabaseHandle _db,
    sync_id_t _sync_id,
    path _from,
    path _to
) :
    Job(_logger, _db, _sync_id, _from, _to) {};

MoveJob::MoveJob(
    std::shared_ptr<Logger> _logger,
    DatabaseHandle _db,
    sync_id_t _sync_id,
    path _from,
    path _to,
    path _source,
    std::optional<std::string> _converter_preset
) :
    Job(_logger, _db, _sync_id, _from, _to),
    source(_source),
    converter_preset(_converter_preset) {};

bool MoveJob::run(bool dry) {
    std::string from_to_str = fmt::format("from {} to {}", from, to);
    bool success;
    try {
        if (!dry) {
            create_directories(to.parent_path());
            rename(from, to);
            register_job();
            clean_parents(from.parent_path());
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
         .destination = to,
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
    path _from,
    path _to
) :
    Job(_logger, _db, _sync_id, _from, _to),
    ddb(_ddb),
    settings(_settings),
    it(_it),
    pabort(0) {
    ddb->pl_item_ref(it);
}

bool ConvertJob::run(bool dry) {
    std::string from_to_str = fmt::format(
        "{} using {} to {}", from, settings.encoder_preset->title, to
    );
    if (!dry) {
        logger->verbose("Converting  {}.", from_to_str);
        auto ddb_conv = (ddb_converter_t*)ddb->plug_get_for_id("converter");
        // TODO implement cancelling
        create_directories(to.parent_path());
        int out =
            ddb_conv->convert2(&settings, it, std::string(to).c_str(), &pabort);
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
         .source = from,
         .destination = to,
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
    path _target
) :
    Job(_logger, _db, _sync_id, _source, _target) {};

bool DeleteJob::run(bool dry) {
    bool success;
    if (!dry) {
        try {
            success = remove(to);
        } catch (filesystem_error& e) {
            logger->log("Failed to delete {}: {}.", to, e.what());
            success = false;
        }
        if (success) {
            register_job();
            clean_parents(to.parent_path());
            logger->log("Deleted {}.", to);
        }
    } else {
        logger->log("Would delete {}.", to);
        success = true;
    }
    return success;
}

void DeleteJob::register_job() {
    db->register_synced_file(
        {.sync_id = sync_id,
         .source = from,
         .destination = std::nullopt,
         .converter_preset = std::nullopt,
         .timestamp = now()}
    );
}

}  // namespace ddb_ows
