#include "job.hpp"

#include <filesystem>
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
    logger.log("Copying from " + std::string(from) + " to " + std::string(to) + ".");
    bool success;
    try {
        if (!dry) {
            create_directories(to.parent_path());
            success = copy_file(from, to, copy_options::update_existing);
            register_job();
        } else {
            success = true;
        }
    } catch (filesystem_error& e) {
        logger.err(e.what());
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
    logger.log("Moving from " + std::string(from) + " to " + std::string(to) + ".");
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
        }
        success = true;
    } catch (filesystem_error& e) {
        logger.err(e.what());
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
    logger.log(
        "Converting " + std::string(from)
        + " using " + settings.encoder_preset->title
        + " to " + std::string(to));
    if (!dry) {
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
        } else {
            logger.err("Converting " + std::string(from) + " failed.");
        }
        return out == 0;
    } else {
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
    logger.log ( "Deleting " + std::string(target) + "." );
    bool success;
    if (!dry) {
        try {
            success = remove(target);
        } catch (filesystem_error& e) {
            logger.err(e.what());
            success = false;
        }
        if (success) {
            register_job();
            clean_parents(to.parent_path());
        }
    } else {
        success = true;
    }
    return success;
}

void DeleteJob::register_job() {
    db->erase(target);
}

}
