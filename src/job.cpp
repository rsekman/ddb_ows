#include "job.hpp"

#include <deadbeef/plugins/converter/converter.h>
#include <filesystem>
#include <system_error>

using namespace std::filesystem;

namespace ddb_ows {

void Job::register_job() {
    db->insert_or_update(
        from,
        db_entry_t {
        .destination = to,
        .timestamp = static_cast<uint64_t>(std::time(nullptr))
        }
    );
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
            success = copy_file(from, to);
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

MoveJob::MoveJob(Logger& _logger, Database* _db, std::string _from, std::string _to) :
    Job(_logger, _db, _from, _to)
{
} ;

bool MoveJob::run(bool dry) {
    logger.log("Moving from " + std::string(from) + " to " + std::string(to) + ".");
    bool success;
    try {
        if (!dry) {
            create_directories(to.parent_path());
            rename(from, to);
            register_job();
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
    it(_it)
{
    ddb->pl_item_ref(it);
}

bool ConvertJob::run(bool dry) {
    logger.log("Converting to " + std::string(to) + ".");
    if (!dry) {
        auto ddb_conv = (ddb_converter_t*) ddb->plug_get_for_id("converter");
        int pabort;
        // TODO implement cancelling
        int out = ddb_conv->convert2(
            &settings,
            it,
            std::string(to).c_str(),
            &pabort
        );
        register_job();
        return out;
    } else {
        return true;
    }
}

}
