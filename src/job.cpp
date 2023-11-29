#include "job.hpp"

#include <filesystem>
#include <fmt/std.h>
#include <system_error>

using namespace std::filesystem;

namespace ddb_ows {

void clean_parents(path p) {
    std::error_code e;
    if (!is_directory(p, e)) {
        return;
    }
    if (remove(p, e) ) {
        clean_parents(p.parent_path());
    }
}

db_entry_t Job::make_entry(){
    return db_entry_t {
        .destination = to,
        .timestamp = static_cast<uint64_t>(std::time(nullptr)),
        .converter_preset = ""
    };
}

void Job::register_job() {
    db->insert_or_update( from, make_entry() );
}
void Job::register_job(db_entry_t entry) {
    db->insert_or_update( from, entry );
}

CopyJob::CopyJob(Logger& _logger, Database* _db, path _from, path _to) :
    Job(_logger, _db, _from, _to)
{
} ;

bool CopyJob::run(bool dry) {
    bool success;
    std::string from_to_str = fmt::format("from {} to {}", from, to);
    try {
        if (!dry) {
            create_directories(to.parent_path());
            success = copy_file(from, to, copy_options::update_existing);
            if( success ) {
                register_job();
                logger.log("Copied " + from_to_str + ".");
            } else {
                logger.err("Failed to copy " + from_to_str + ".");
            }
        } else {
            success = true;
            logger.log("Would copy " + from_to_str + ".");
        }
    } catch (filesystem_error& e) {
        logger.err("Failed to copy {}: {}.", from_to_str, e.what());
        success = false;
    }
    return success;
}

MoveJob::MoveJob(Logger& _logger, Database* _db, path _from, path _to, path _source) :
    Job(_logger, _db, _from, _to),
    source(_source)
{
} ;

void MoveJob::register_job(db_entry_t entry) {
    db->insert_or_update( source, entry );
}
bool MoveJob::run(bool dry) {
    std::string from_to_str = fmt::format("from {} to {}", from, to);
    bool success;
    try {
        if (!dry) {
            create_directories(to.parent_path());
            rename(from, to);
            db_entry_t entry = make_entry();
            auto old = db->find_entry(from);
            if (old != db->end() ) {
                entry.converter_preset = old->second.converter_preset;
            }
            register_job(entry);
            clean_parents(from.parent_path());
            logger.log("Moved from " + from_to_str + ".");
        } else {
            logger.log("Would move " + from_to_str + ".");
        }
        success = true;
    } catch (filesystem_error& e) {
        logger.err("Failed to move {}: {}.", from_to_str, e.what());
        success = false;
    }
    return success;
}

ConvertJob::~ConvertJob() {
    ddb->pl_item_unref(it);
}

ConvertJob::ConvertJob(
    Logger& _logger,
    Database* _db,
    DB_functions_t* _ddb,
    ddb_converter_settings_t _settings,
    DB_playItem_t* _it,
    path _from,
    path _to
) :
    Job(_logger, _db, _from, _to),
    ddb(_ddb),
    settings(_settings),
    it(_it),
    pabort(0)
{
    ddb->pl_item_ref(it);
}


bool ConvertJob::run(bool dry) {
    std::string from_to_str = fmt::format(
        "{} using {} to {}",
       from, settings.encoder_preset->title, to
    );
    if (!dry) {
        logger.verbose("Converting  {}.", from_to_str);
        auto ddb_conv = (ddb_converter_t*) ddb->plug_get_for_id("converter");
        // TODO implement cancelling
        create_directories(to.parent_path());
        int out = ddb_conv->convert2(
            &settings,
            it,
            std::string(to).c_str(),
            &pabort
        );
        if (!out) {
            db_entry_t entry = make_entry();
            entry.converter_preset = settings.encoder_preset->title;
            register_job(entry);
            logger.log("Conversion of {} successful.", from_to_str);
        } else {
            logger.err("Converting {} failed.", from_to_str);
        }
        return out == 0;
    } else {
        logger.log("Would convert {}.", from_to_str);
        return true;
    }
}

void ConvertJob::abort(){
    pabort = 1;
}

DeleteJob::DeleteJob(Logger& _logger, ddb_ows::Database* _db, path _target) :
   Job(_logger, _db, "", ""),
   target(_target)
{
};

bool DeleteJob::run(bool dry) {
    bool success;
    if (!dry) {
        try {
            success = remove(target);
        } catch (filesystem_error& e) {
            logger.log ("Failed to delete {}: {}.", target, e.what());
            success = false;
        }
        if (success) {
            register_job();
            clean_parents(to.parent_path());
            logger.log ("Deleted {} .", target);
        }
    } else {
        logger.log ("Would delete {} .", target);
        success = true;
    }
    return success;
}

void DeleteJob::register_job() {
    db->erase(target);
}

}
