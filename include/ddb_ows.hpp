#ifndef DDB_OWS_HPP
#define DDB_OWS_HPP

#include <spdlog/spdlog.h>

#include <future>
#include <mutex>
#include <string>

#include "cancellationtoken.hpp"
#include "config.hpp"
#include "deadbeef/deadbeef.h"
#include "job.hpp"
#include "logger.hpp"

typedef std::function<void(std::unique_ptr<ddb_ows::Job>)> job_cb_t;
typedef std::function<void()> cancel_cb_t;

typedef std::packaged_task<bool(bool, job_cb_t)> worker_thread_t;

typedef struct wt_futures_s {
    std::mutex m;
    std::condition_variable c;
    std::vector<std::shared_future<bool>> futures;
} wt_futures_t;

using namespace ddb_ows;

typedef struct {
    DB_misc_t plugin;
    ddb_ows::Configuration& conf;
    std::mutex running;
    std::shared_ptr<ddb_ows::CancellationToken> cancellationtoken;
    std::shared_ptr<spdlog::logger> logger;
    wt_futures_t worker_thread_futures;
    plt_uuid (*plt_get_uuid)(ddb_playlist_t* plt);
    std::string (*get_output_path)(DB_playItem_t* it, char* format);
    bool (*queue_jobs)(std::vector<ddb_playlist_t*> playlists, Logger& logger);
    int (*jobs_count)();
    bool (*run)(bool dry, job_cb_t callback);
    bool (*save_playlists)(
        const char* ext,
        std::vector<ddb_playlist_t*> playlists,
        Logger& logger,
        bool dry
    );
    bool (*cancel)(cancel_cb_t callback);
} ddb_ows_plugin_t;

#endif
