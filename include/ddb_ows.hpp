#ifndef DDB_OWS_HPP
#define DDB_OWS_HPP

#include <spdlog/spdlog.h>

#include "cancellationtoken.hpp"
#include "config.hpp"
#include "deadbeef/deadbeef.h"
#include "job.hpp"
#include "logger.hpp"

namespace ddb_ows {

typedef std::function<void(const char*)> playlist_save_cb_t;
typedef std::function<void(size_t)> sources_gathered_cb_t;
typedef std::function<void(size_t)> queueing_complete_cb_t;
typedef std::function<void()> job_queued_cb_t;
typedef std::function<void(std::unique_ptr<ddb_ows::Job>, bool)>
    job_finished_cb_t;
typedef std::function<void()> cancel_cb_t;

struct callback_t {
    playlist_save_cb_t on_playlist_save;
    sources_gathered_cb_t on_sources_gathered;
    job_queued_cb_t on_job_queued;
    queueing_complete_cb_t on_queueing_complete;
    job_finished_cb_t on_job_finished;
};
}  // namespace ddb_ows

using namespace ddb_ows;

typedef struct {
    DB_misc_t plugin;
    std::shared_ptr<ddb_ows::Configuration> conf;
    bool (*run)(
        bool dry,
        const std::vector<ddb_playlist_t*>& playlists,
        Logger& logger,
        callback_t callbacks
    );
    bool (*cancel)(cancel_cb_t callback);
    std::string (*get_output_path)(DB_playItem_t* it, char* format);
    plt_uuid (*plt_get_uuid)(ddb_playlist_t* plt);
} ddb_ows_plugin_t;

#endif
