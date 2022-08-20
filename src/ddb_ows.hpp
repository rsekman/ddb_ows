#ifndef DDB_OWS_PROJECT_ID

#include <string>

#include "deadbeef/deadbeef.h"

#include "config.hpp"
#include "constants.hpp"
#include "database.hpp"
#include "log.hpp"
#include "logger.hpp"
#include "job.hpp"

typedef std::function<void(std::unique_ptr<ddb_ows::Job>)> job_cb_t;

typedef struct {
    DB_misc_t plugin;
    ddb_ows::Configuration& conf;
    ddb_ows::Database* db;
    std::string (*get_output_path)(DB_playItem_t* it, char* format);
    bool (*queue_jobs)(std::vector<ddb_playlist_t*> playlists, Logger& logger);
    int (*jobs_count)();
    bool (*run)(bool dry, job_cb_t callback);
} ddb_ows_plugin_t;

#endif
