#include "ddb_ows.hpp"

#include <deadbeef/artwork.h>
#include <deadbeef/converter.h>
#include <fmt/chrono.h>
// for formatting std::filesystem::path
#include <fmt/std.h>
#include <limits.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

#include "config.hpp"
#include "constants.hpp"
#include "database.hpp"
#include "job.hpp"
#include "jobsqueue.hpp"
#include "playlist_uuid.hpp"

using namespace std::chrono_literals;
using namespace std::chrono;
using namespace std::filesystem;

namespace ddb_ows {

static DB_functions_t* ddb;
Configuration conf = Configuration();

// TODO: move these out of global scope

ddb_artwork_plugin_t* ddb_artwork = NULL;
ddb_converter_t* ddb_converter = NULL;

std::random_device rd;
std::mt19937 mersenne_twister(rd());
auto dist = std::uniform_int_distribution<long>(LONG_MIN, LONG_MAX);

void escape(std::string& s) {
    for (auto& i : s) {
        switch (i) {
            case '/':
            case '\\':
            case ':':
            case '*':
            case '?':
            case '"':
            case '<':
            case '>':
            case '|':
                i = '-';
        }
    }
}

std::string get_output_path(DB_playItem_t* it, char* format) {
    DB_playItem_t* copy = ddb->pl_item_alloc();
    // std::basic_regex escape("[/\\:*?\"<>|]");
    ddb->pl_lock();
    DB_metaInfo_t* meta = ddb->pl_get_metadata_head(it);
    while (meta != NULL) {
        std::string val(meta->value);
        escape(val);
        ddb->pl_add_meta(copy, meta->key, val.c_str());
        meta = meta->next;
    }
    ddb->pl_unlock();
    char out[PATH_MAX];
    ddb_tf_context_t ctx = {
        ._size = sizeof(ddb_tf_context_t),
        .flags = 0,
        .it = copy,
        .plt = NULL,
        // TODO change this?
        .idx = 0,
        .id = 0,
        .iter = PL_MAIN,
    };
    ddb->tf_eval(&ctx, format, out, sizeof(out));
    ddb->pl_item_unref(copy);
    return std::string(out);
}

JobsQueue* jobs = new JobsQueue();

struct cover_req_t {
    std::mutex m;
    std::condition_variable c;
    bool returned;
    bool timed_out = false;
    ddb_cover_info_t* cover;
};

void callback_cover_art_found(
    int error, ddb_cover_query_t* query, ddb_cover_info_t* cover
) {
    auto creq = static_cast<std::shared_ptr<cover_req_t>*>(query->user_data);

    auto logger = spdlog::get(DDB_OWS_PROJECT_ID);

    {
        std::lock_guard<std::mutex> lock((*creq)->m);
        if (!(*creq)->timed_out) {
            (*creq)->returned = true;
            if ((query->flags & DDB_ARTWORK_FLAG_CANCELLED) || cover == NULL ||
                cover->image_filename == NULL)
            {
                (*creq)->cover = NULL;
            } else {
                logger->debug("Found cover: {}", cover->image_filename);
                (*creq)->cover = cover;
            }
        }
    }
    (*creq)->c.notify_all();
    delete creq;
    ddb->pl_item_unref(query->track);
    free(query);
}

bool is_newer(path a, path b) {
    return last_write_time(a) > last_write_time(b);
}

// Returns false if cancelled, true if successful
bool queue_cover_jobs(
    Logger& logger,
    DatabaseHandle db,
    std::deque<std::shared_ptr<DB_playItem_t>>& items
) {
    path from;
    path to;
    char* fmt = ddb->tf_compile(conf.get_fn_formats()[0].c_str());
    if (fmt == NULL) {
        return false;
    }
    auto root = path(conf.get_root());

    ddb_ows_plugin_t* ddb_ows =
        (ddb_ows_plugin_t*)ddb->plug_get_for_id("ddb_ows");
    auto plug_logger = ddb_ows->logger;

    while (!items.empty()) {
        auto it = items.front().get();
        if (ddb_ows->cancellationtoken->get()) {
            plug_logger->debug("Cancelled while queueing cover jobs");
            break;
        }
        from = ddb->pl_find_meta(it, ":URI");
        to = root / get_output_path(it, fmt);
        path target_dir = to.parent_path();
        ddb_cover_query_t* cover_query =
            (ddb_cover_query_t*)calloc(sizeof(ddb_cover_query_t), 1);
        cover_query->flags = 0;
        cover_query->track = it;
        ddb->pl_item_ref(it);
        int64_t sid = dist(mersenne_twister);
        cover_query->source_id = sid;
        cover_query->_size = sizeof(ddb_cover_query_t);

        std::shared_ptr<cover_req_t> creq(new cover_req_t{
            .m = std::mutex(),
            .c = std::condition_variable(),
            .returned = false,
            .timed_out = false,
            .cover = nullptr
        });
        auto creq_copy = new std::shared_ptr(creq);
        cover_query->user_data = creq_copy;

        ddb_artwork->cover_get(cover_query, callback_cover_art_found);
        auto timeout =
            std::chrono::milliseconds(ddb_ows->conf.get_cover_timeout_ms());
        std::unique_lock<std::mutex> lock(creq->m);
        if (!creq->returned) {
            creq->c.wait_for(lock, timeout, [&creq] { return creq->returned; });
        }
        if (!creq->returned) {
            plug_logger->debug(
                "Cover request for {} timed out after {:%Q %q}",
                target_dir,
                timeout
            );
            creq->timed_out = true;
        } else if (creq->cover == NULL) {
            plug_logger->debug("No cover found for {}", target_dir);
        } else {
            path from = creq->cover->image_filename;
            path to = target_dir / conf.get_cover_fname();
            auto old = db->find_entry(from);
            auto old_dest =
                old ? std::optional{old->destination} : std::nullopt;

            bool dest_newer;
            try {
                dest_newer = is_newer(to, from);
            } catch (std::filesystem::filesystem_error& e) {
                dest_newer = false;
            }
            bool old_newer;
            try {
                old_newer = old_dest && is_newer(*old_dest, from);
            } catch (std::filesystem::filesystem_error& e) {
                old_newer = false;
            }

            if (dest_newer) {
                logger.verbose("Cover at {} is newer than source {}", to, from);
            } else if (old_newer && *old_dest != to) {
                auto cover_job =
                    std::make_unique<MoveJob>(logger, db, *old_dest, to, from);
                jobs->push_back(std::move(cover_job));
            } else {
                auto cover_job = std::make_unique<CopyJob>(
                    logger, db, creq->cover->image_filename, to
                );
                jobs->push_back(std::move(cover_job));
            }
        }
        items.pop_front();
    }
    return true;
}

ddb_converter_settings_t make_encoder_settings() {
    auto preset = ddb_converter->encoder_preset_get_list();
    auto sel = conf.get_conv_preset();
    ddb_converter_settings_t out{
        // these two mean to use the same sample format as input
        .output_bps = -1,
        .output_is_float = -1,
        .encoder_preset = NULL,
        .dsp_preset = NULL,
        .bypass_conversion_on_same_format = 0,
        .rewrite_tags_after_copy = 0,
    };
    while (preset != NULL) {
        if (sel == std::string(preset->title)) {
            out.encoder_preset = preset;
            break;
        }
        preset = preset->next;
    }
    // TODO: DSP preset
    return out;
}

bool should_convert(DB_playItem_t* it) {
    DB_decoder_t** decoders = ddb->plug_get_decoder_list();
    // decoders and decoders[i]->exts are null-terminated arrays
    int i = 0;
    std::string::size_type n;
    std::set<std::string> sels = conf.get_conv_fts();
    const char* fname = ddb->pl_find_meta(it, ":URI");
    const char* ext = strrchr(fname, '.');
    auto logger = spdlog::get(DDB_OWS_PROJECT_ID);

    if (ext) {
        ext++;
    } else {
        logger->warn("Unable to determine filetype for {}.");
        return false;
    }
    while (decoders[i]) {
        std::string s(decoders[i]->plugin.name);
        if ((n = s.find(" decoder")) != std::string::npos ||
            (n = s.find(" player")) != std::string::npos)
        {
            s = s.substr(0, n);
        }
        if (sels.count(s)) {
            const char** exts = decoders[i]->exts;
            if (exts) {
                for (int j = 0; exts[j]; j++) {
                    if (!strcasecmp(exts[j], ext) || !strcmp(exts[j], "*")) {
                        return true;
                    }
                }
            }
        }
        i++;
    }
    return false;
}

void make_job(
    DatabaseHandle db,
    JobsQueue* out,
    Logger& logger,
    DB_playItem_t* it,
    path from,
    path to,
    ddb_converter_settings_t conv_settings
) {
    // throws: can throw any filesystem error throw by checking ctime
    auto old = db->find_entry(from);
    auto old_dest = old ? std::optional{old->destination} : std::nullopt;

    bool dest_newer;
    try {
        dest_newer = is_newer(to, from);
    } catch (std::filesystem::filesystem_error& e) {
        dest_newer = false;
    }
    bool old_newer;
    try {
        old_newer = old_dest && is_newer(*old_dest, from);
    } catch (std::filesystem::filesystem_error& e) {
        old_newer = false;
    }

    if (should_convert(it)) {
        to.replace_extension(conf.get_conv_ext());
        std::string preset_title = conv_settings.encoder_preset->title;
        auto cjob = std::make_unique<ConvertJob>(
            logger, db, ddb, conv_settings, it, from, to
        );
        if (old && old->converter_preset == preset_title) {
            // This source file was synced previously and the same encoder
            // preset is selected

            if (dest_newer) {
                // The destination exists and is newer than the source
                logger.verbose(
                    "Source {} was already converted with {}; skipping.",
                    from,
                    preset_title
                );
                return;
            } else if (old_newer && *old_dest != to) {
                // The source was previously converted with a different
                // destination, which is newer than the source
                out->emplace_back(new MoveJob(logger, db, *old_dest, to, from));
            } else {
                // The source is newer => delete the old destination and
                // reconvert with new destination
                if (old_dest != to) {
                    out->emplace_back(new DeleteJob(logger, db, *old_dest));
                }
                out->push_back(std::move(cjob));
            }
        } else {
            out->push_back(std::move(cjob));
            if (old && old->converter_preset != preset_title) {
                // Clean up previous conversion with a different preset
                out->emplace_back(new DeleteJob(logger, db, *old_dest));
            }
        }
    } else if (old_dest && *old_dest != to && exists(*old_dest)) {
        // This source file was synced previously, and was not converted
        if (old_newer && old->converter_preset == "") {
            // the destination file is newer than the source => move
            out->emplace_back(new MoveJob(logger, db, *old_dest, to, from));
        } else {
            // the source file is newer than the old copy, or was previously
            // converted but should not be now => delete the old copy/conversion
            // and copy anew
            out->emplace_back(new DeleteJob(logger, db, *old_dest));
            out->emplace_back(new CopyJob(logger, db, from, to));
        }
    } else if (dest_newer) {
        logger.verbose(
            "Destination {} is newer than source {}; skipping.", to, from
        );
    } else {
        out->emplace_back(new CopyJob(logger, db, from, to));
    }
}

std::string plt_get_title(ddb_playlist_t* plt) {
    char plt_title[PATH_MAX];
    ddb->plt_get_title(plt, plt_title, sizeof(plt_title));
    std::string out(plt_title);
    out.shrink_to_fit();
    return out;
}

bool save_playlist(
    const char* ext, ddb_playlist_t* plt_in, Logger& logger, bool dry
) {
    path root(conf.get_root());
    std::string title = plt_get_title(plt_in);
    std::string escaped = title;
    escape(escaped);
    std::string pl_to(root / escaped);
    pl_to += ".";
    pl_to += ext;

    ddb_ows_plugin_t* ddb_ows =
        (ddb_ows_plugin_t*)ddb->plug_get_for_id("ddb_ows");
    auto plug_logger = ddb_ows->logger;

    plug_logger->debug("Saving playlist to {}", pl_to);
    int out = 0;

    char* fmt = ddb->tf_compile(conf.get_fn_formats()[0].c_str());
    if (fmt == NULL) {
        return false;
    }

    if (!dry) {
        auto head = ddb->plt_get_head_item(plt_in, PL_MAIN);
        auto tail = ddb->plt_get_tail_item(plt_in, PL_MAIN);
        if (!head || !tail) {
            logger.warn("Playlist {} is empty, not saving.", title);
            return false;
        }
        ddb->pl_item_unref(head);
        ddb->pl_item_unref(tail);
        ddb_playlist_t* plt_out = ddb->plt_alloc(title.c_str());

        ddb_playItem_t** its;
        ddb_playItem_t* after = ddb->plt_get_head_item(plt_out, PL_MAIN);

        size_t n_its = ddb->plt_get_items(plt_in, &its);
        for (size_t k = 0; k < n_its; k++) {
            DB_playItem_t* new_it = ddb->pl_item_alloc();
            ddb->pl_item_copy(new_it, its[k]);
            ddb->pl_item_unref(its[k]);
            path out_path = get_output_path(new_it, fmt);

            if (should_convert(new_it)) {
                out_path.replace_extension(conf.get_conv_ext());
            }

            ddb->pl_replace_meta(new_it, ":URI", out_path.c_str());
            after = ddb->plt_insert_item(plt_out, after, new_it);
            ddb->pl_item_unref(new_it);
        }
        free(its);

        head = ddb->plt_get_head_item(plt_out, PL_MAIN);
        tail = ddb->plt_get_tail_item(plt_out, PL_MAIN);
        out =
            ddb->plt_save(plt_out, head, tail, pl_to.c_str(), NULL, NULL, NULL);
        ddb->pl_item_unref(head);
        ddb->pl_item_unref(tail);
        ddb->plt_unref(plt_out);
    }
    if (out < 0) {
        logger.err("Failed to save playlist {}.", title);
        return false;
    } else {
        logger.log("Saved playlist {}", title);
        return true;
    }
}

bool save_playlists(
    const char* ext,
    std::vector<ddb_playlist_t*> playlists,
    Logger& logger,
    bool dry
) {
    // returns true if all playlists were successfully saved
    bool out = true;
    for (ddb_playlist_t* plt : playlists) {
        bool saved = save_playlist(ext, plt, logger, dry);
        out = out && saved;
    }
    return out;
}

struct job_source {
    std::shared_ptr<ddb_playItem_t> it;
};

// Returns false if cancelled, true if successful
bool queue_jobs(std::vector<ddb_playlist_t*> playlists, Logger& logger) {
    if (!jobs->empty()) {
        // To avoid double-queueing
        return false;
    }
    char* fmt = ddb->tf_compile(conf.get_fn_formats()[0].c_str());
    if (fmt == NULL) {
        return false;
    }
    jobs->open();

    // We need to lock the playlist to avoid data races as we're looping over
    // it, but cover requests run async in another thread and ALSO need to lock
    // the playlist. Therefore we have to queue up items to dispatch cover
    // requests for once we are done traversing the playlist.
    std::unordered_set<path> cover_dirs{};
    std::deque<std::shared_ptr<DB_playItem_t>> cover_its{};

    ddb_ows_plugin_t* ddb_ows =
        (ddb_ows_plugin_t*)ddb->plug_get_for_id("ddb_ows");
    auto plug_logger = ddb_ows->logger;

    path root(conf.get_root());
    DatabaseHandle db(root);

    auto conv_settings = make_encoder_settings();

    // std::vector<ddb_playItem_t*> its;
    std::vector<job_source> sources;

    ddb->pl_lock();
    for (auto plt : playlists) {
        plug_logger->debug(
            "Looking for jobs from playlist {}", plt_get_title(plt)
        );

        DB_playItem_t* it;
        it = ddb->plt_get_first(plt, PL_MAIN);
        while (it != NULL) {
            auto p = std::shared_ptr<DB_playItem_t>(it, ddb->pl_item_unref);
            sources.push_back({.it = p});
            it = ddb->pl_get_next(it, PL_MAIN);
        }
    }
    ddb->pl_unlock();

    std::unordered_set<std::string> visited_sources{};

    ddb_ows->cancellationtoken = std::make_shared<CancellationToken>();
    for (auto source : sources) {
        auto it = source.it.get();
        path from;
        path to;
        from = std::string(ddb->pl_find_meta(it, ":URI"));
        if (visited_sources.count(from) > 0) {
            // This source file was already processed, avoid queueing redundant
            // jobs
            continue;
        }
        // Items will be unref'd when sources goes out of scope
        if (ddb_ows->cancellationtoken->get()) {
            break;
        }
        visited_sources.insert(from);
        to = root / get_output_path(it, fmt);
        if (!exists(from)) {
            logger.err("Source file {} does not exist!", from);
        } else {
            try {
                make_job(db, jobs, logger, it, from, to, conv_settings);
            } catch (std::filesystem::filesystem_error& e) {
                logger.err("Could not queue job for {}: {}", from, e.what());
                continue;
            }
        }

        path target_dir = to.parent_path();
        if (ddb_ows->conf.get_cover_sync() && !cover_dirs.count(target_dir)) {
            cover_its.push_back(source.it);
            plug_logger->debug("Copying cover to {}", target_dir);
            cover_dirs.insert(target_dir);
        }
    }
    if (ddb_ows->cancellationtoken->get()) {
        plug_logger->debug("Cancelled while queueing jobs");
        free(fmt);
        return false;
    }

    // Now we can dispatch cover requests
    if (!queue_cover_jobs(logger, db, cover_its)) {
        free(fmt);
        return false;
    }
    // TODO: delete unreferenced files
    jobs->close();
    plug_logger->debug("Found {} jobs", jobs->size());
    free(fmt);
    return true;
}

int jobs_count() {
    if (jobs) {
        return jobs->size();
    } else {
        return 0;
    }
}

bool worker_thread(bool dry, job_cb_t callback) {
    std::unique_ptr<Job> job;
    while ((job = jobs->pop())) {
        // unique_ptr is falsey if there is no object
        bool status = job->run(dry);
        if (status && callback) {
            // callback is falsy if the function object is empty
            callback(std::move(job));
        }
    }
    return true;
}

bool run(bool dry, job_cb_t callback) {
    ddb_ows_plugin_t* ddb_ows =
        (ddb_ows_plugin_t*)ddb->plug_get_for_id("ddb_ows");
    int n_wts = conf.get_conv_wts();
    ddb_ows->worker_thread_futures.futures.clear();
    for (int i = 0; i < n_wts; i++) {
        auto task = worker_thread_t(worker_thread);
        ddb_ows->worker_thread_futures.futures.push_back(task.get_future());
        std::thread t(std::move(task), dry, callback);
        t.detach();
    }
    std::lock_guard lock(ddb_ows->worker_thread_futures.m);
    for (auto t = ddb_ows->worker_thread_futures.futures.begin();
         t != ddb_ows->worker_thread_futures.futures.end();
         t++)
    {
        t->wait();
    }
    ddb_ows->worker_thread_futures.c.notify_all();
    return true;
}

bool cancel(cancel_cb_t callback) {
    ddb_ows_plugin_t* ddb_ows =
        (ddb_ows_plugin_t*)ddb->plug_get_for_id("ddb_ows");
    ddb_ows->logger->debug("Cancelling");
    ddb_ows->cancellationtoken->cancel();
    jobs->cancel();
    std::lock_guard lock(ddb_ows->worker_thread_futures.m);
    for (auto t = ddb_ows->worker_thread_futures.futures.begin();
         t != ddb_ows->worker_thread_futures.futures.end();
         t++)
    {
        t->wait();
    }
    callback();
    ddb_ows->worker_thread_futures.c.notify_all();
    return true;
}

int start() { return 0; }

int stop() { return 0; }

int disconnect() { return 0; }

int connect() {
    ddb_converter = (ddb_converter_t*)ddb->plug_get_for_id("converter");
    ddb_artwork = (ddb_artwork_plugin_t*)ddb->plug_get_for_id("artwork2");
    jobs->close();
    return 0;
}

int handleMessage(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2) {
    return 0;
}

plt_uuid _plt_get_uuid(ddb_playlist_t* plt) { return plt_get_uuid(plt, ddb); }

const char* configDialog_ = "";

ddb_ows_plugin_t plugin = {
    .plugin =
        {
            .plugin =
                {
                    .type = DB_PLUGIN_MISC,
                    .api_vmajor = 1,
                    .api_vminor = 8,
                    .version_major = DDB_OWS_VERSION_MAJOR,
                    .version_minor = DDB_OWS_VERSION_MINOR,
                    .id = DDB_OWS_PROJECT_ID,
                    .name = DDB_OWS_PROJECT_NAME,
                    .descr = DDB_OWS_PROJECT_DESC,
                    .copyright = DDB_OWS_LICENSE_TEXT,
                    .website = DDB_OWS_PROJECT_URL,
                    .start = start,
                    .stop = stop,
                    .connect = connect,
                    .disconnect = disconnect,
                    .message = handleMessage,
                    .configdialog = configDialog_,
                },
        },
    .conf = conf,
    .cancellationtoken = std::make_shared<ddb_ows::CancellationToken>(),
    .logger = std::shared_ptr<spdlog::logger>(),
    .worker_thread_futures =
        wt_futures_t{
            .m = std::mutex(),
            .c = std::condition_variable(),
            .futures = std::vector<std::shared_future<bool>>{}
        },
    .plt_get_uuid = _plt_get_uuid,
    .get_output_path = get_output_path,
    .queue_jobs = queue_jobs,
    .jobs_count = jobs_count,
    .run = run,
    .save_playlists = save_playlists,
    .cancel = cancel,
};

void init(DB_functions_t* api) {
    plugin.conf.set_api(api);
    plugin.logger = spdlog::stderr_color_mt(DDB_OWS_PROJECT_ID);
    plugin.logger->set_level(spdlog::level::debug);
    plugin.logger->set_pattern("[%n] [%^%l%$] [thread %t] %v");
    plugin.conf.load_conf();
}

DB_plugin_t* load(DB_functions_t* api) {
    ddb = api;
    init(api);
    return (DB_plugin_t*)&plugin;
}

extern "C" DB_plugin_t* ddb_ows_load(DB_functions_t* api) { return load(api); }

}  // namespace ddb_ows
