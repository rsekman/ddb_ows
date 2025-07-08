#ifndef DDB_OWS_HPP
#define DDB_OWS_HPP

#include <spdlog/spdlog.h>

#include "cancellationtoken.hpp"
#include "config.hpp"
#include "deadbeef/deadbeef.h"
#include "job.hpp"
#include "logger.hpp"

typedef std::function<void(std::unique_ptr<ddb_ows::Job>)> job_cb_t;
typedef std::function<void()> cancel_cb_t;

using namespace ddb_ows;

typedef struct {
    DB_misc_t plugin;
    ddb_ows::Configuration& conf;
    bool (*queue_jobs)(std::vector<ddb_playlist_t*> playlists, Logger& logger);
    size_t (*jobs_count)();
    bool (*run)(bool dry, job_cb_t callback);
    bool (*save_playlists)(
        const char* ext,
        std::vector<ddb_playlist_t*> playlists,
        Logger& logger,
        bool dry
    );
    bool (*cancel)(cancel_cb_t callback);
    std::string (*get_output_path)(DB_playItem_t* it, char* format);
    plt_uuid (*plt_get_uuid)(ddb_playlist_t* plt);
} ddb_ows_plugin_t;

#endif
