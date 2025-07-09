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
#include <future>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <vector>

#include "constants.hpp"
#include "database.hpp"
#include "job.hpp"
#include "jobsqueue.hpp"
#include "playlist_uuid.hpp"

using namespace std::chrono_literals;
using namespace std::chrono;
using namespace std::filesystem;

#ifndef DDB_OWS_LOGLEVEL
#define DDB_OWS_LOGLEVEL info
#endif

namespace ddb_ows {

typedef std::packaged_task<bool(bool, job_finished_cb_t)> worker_thread_t;

typedef struct wt_futures_s {
    std::mutex m;
    std::condition_variable c;
    std::vector<std::shared_future<bool>> futures;
} wt_futures_t;

struct ddb_ows_plugin_int {
    ddb_ows_plugin_t pub;
    std::mutex running;
    std::shared_ptr<ddb_ows::CancellationToken> cancellationtoken;
    std::shared_ptr<JobsQueue> jobs;
    std::shared_ptr<spdlog::logger> logger;
    wt_futures_t worker_thread_futures;
};

static DB_functions_t* ddb;

// TODO: move these out of global scope

ddb_artwork_plugin_t* ddb_artwork = nullptr;
ddb_converter_t* ddb_converter = nullptr;

int start() { return 0; }
int stop() { return 0; }
int disconnect() { return 0; }
int connect();

int handleMessage(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2) {
    return 0;
}

const char* configDialog_ = "";

DB_misc_t plugin_ddb{
    .plugin = {
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
};

plt_uuid _plt_get_uuid(ddb_playlist_t* plt) { return plt_get_uuid(plt, ddb); }

// These public interface function are so-to-speak non-static, viz., they need
// the address of the plugin, so they have to be forward-declared. We use some
// introspection to make it somewhat more readable.
std::remove_reference_t<decltype(*ddb_ows_plugin_t::run)> run;
std::remove_reference_t<decltype(*ddb_ows_plugin_t::cancel)> cancel;
std::remove_reference_t<decltype(*ddb_ows_plugin_t::get_output_path)>
    get_output_path;

ddb_ows_plugin_t plugin_public = {
    .plugin = plugin_ddb,
    .run = run,
    .cancel = cancel,
    .get_output_path = get_output_path,
    .plt_get_uuid = _plt_get_uuid,
};

ddb_ows_plugin_int plugin = {
    .pub = plugin_public,
    .cancellationtoken = std::make_shared<ddb_ows::CancellationToken>(),
    .logger = std::shared_ptr<spdlog::logger>(),
    .worker_thread_futures = wt_futures_t{
        .m = std::mutex(),
        .c = std::condition_variable(),
        .futures = std::vector<std::shared_future<bool>>{}
    },
};

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
    while (meta != nullptr) {
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
        .plt = nullptr,
        // TODO change this?
        .idx = 0,
        .id = 0,
        .iter = PL_MAIN,
    };
    ddb->tf_eval(&ctx, format, out, sizeof(out));
    ddb->pl_item_unref(copy);
    return std::string(out);
}

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
            if ((query->flags & DDB_ARTWORK_FLAG_CANCELLED) ||
                cover == nullptr || cover->image_filename == nullptr)
            {
                (*creq)->cover = nullptr;
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
    bool dry,
    Logger& logger,
    DatabaseHandle db,
    sync_id_t sync_id,
    std::deque<std::shared_ptr<DB_playItem_t>>& items,
    job_queued_cb_t queued_cb
) {
    path from;
    path to;
    auto conf = plugin.pub.conf;
    auto jobs = plugin.jobs;
    auto plug_logger = plugin.logger;

    char* fmt = ddb->tf_compile(conf->get_fn_formats()[0].c_str());
    if (fmt == nullptr) {
        return false;
    }

    auto root = path(conf->get_root());
    std::random_device rd;
    std::mt19937 mersenne_twister(rd());
    auto dist = std::uniform_int_distribution<long>(LONG_MIN, LONG_MAX);

    while (!items.empty()) {
        auto it = items.front().get();
        if (plugin.cancellationtoken->get()) {
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
        auto timeout = std::chrono::milliseconds(conf->get_cover_timeout_ms());
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
        } else if (creq->cover == nullptr) {
            plug_logger->debug("No cover found for {}", target_dir);
        } else {
            path from = creq->cover->image_filename;
            if (!dry) {
                db->register_file(from);
            }
            path to = target_dir / conf->get_cover_fname();
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
                auto cover_job = std::make_unique<MoveJob>(
                    logger, db, sync_id, *old_dest, to, from, ""
                );
                jobs->push_back(std::move(cover_job));
            } else {
                auto cover_job = std::make_unique<CopyJob>(
                    logger, db, sync_id, creq->cover->image_filename, to
                );
                jobs->push_back(std::move(cover_job));
            }
            if (queued_cb) {
                queued_cb();
            }
        }
        items.pop_front();
    }
    return true;
}

std::optional<ddb_converter_settings_t> make_encoder_settings() {
    if (ddb_converter == nullptr) {
        return {};
    }
    auto preset = ddb_converter->encoder_preset_get_list();
    auto sel = plugin.pub.conf->get_conv_preset();
    ddb_converter_settings_t out{
        // these two mean to use the same sample format as input
        .output_bps = -1,
        .output_is_float = -1,
        .encoder_preset = nullptr,
        .dsp_preset = nullptr,
        .bypass_conversion_on_same_format = 0,
        .rewrite_tags_after_copy = 0,
    };
    while (preset != nullptr) {
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
    std::set<std::string> sels = plugin.pub.conf->get_conv_fts();
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
    std::shared_ptr<JobsQueue> out,
    Logger& logger,
    DB_playItem_t* it,
    sync_id_t sync_id,
    path from,
    path to,
    const std::optional<ddb_converter_settings_t>& conv_settings
) {
    // throws: can throw any filesystem error throw by checking ctime
    auto conf = plugin.pub.conf;
    const auto old = db->find_entry(from);
    const std::optional<path> old_dest = old ? old->destination : std::nullopt;

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
        to.replace_extension(conf->get_conv_ext());
        if (!conv_settings) {
            logger.warn(
                "Source {} should be converted, but converter plugin is not "
                "available.",
                from
            );
            return;
        }
        to.replace_extension(conf->get_conv_ext());
        std::string preset_title = conv_settings->encoder_preset->title;
        auto cjob = std::make_unique<ConvertJob>(
            logger, db, ddb, *conv_settings, it, sync_id, from, to
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
            } else if (old_newer && old_dest != to) {
                // The source was previously converted with a different
                // destination, which is newer than the source
                out->emplace_back(new MoveJob(
                    logger,
                    db,
                    sync_id,
                    *old_dest,
                    to,
                    from,
                    old->converter_preset
                ));
            } else {
                // The source is newer => delete the old destination and
                // reconvert with new destination
                if (old_dest != to) {
                    out->emplace_back(
                        new DeleteJob(logger, db, sync_id, from, *old_dest)
                    );
                }
                out->push_back(std::move(cjob));
            }
        } else if (old) {
            // This source file was previously synced, but with a different
            // encoder. Convert it, and clean up the old file.
            out->push_back(std::move(cjob));
            out->emplace_back(
                new DeleteJob(logger, db, sync_id, from, *old_dest)
            );
        } else {
            // This source file was not previously synced. All we have to do is
            // convert it.
            out->push_back(std::move(cjob));
        }
    } else if (old_dest && *old_dest != to && exists(*old_dest)) {
        // This source file was synced previously, and was not converted
        if (old_newer && !old->converter_preset) {
            // the destination file is newer than the source => move
            out->emplace_back(
                new MoveJob(logger, db, sync_id, *old_dest, to, from, "")
            );
        } else {
            // the source file is newer than the old copy, or was previously
            // converted but should not be now => delete the old copy/conversion
            // and copy anew
            out->emplace_back(
                new DeleteJob(logger, db, sync_id, from, *old_dest)
            );
            out->emplace_back(new CopyJob(logger, db, sync_id, from, to));
        }
    } else if (dest_newer) {
        logger.verbose(
            "Destination {} is newer than source {}; skipping.", to, from
        );
    } else {
        out->emplace_back(new CopyJob(logger, db, sync_id, from, to));
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
    auto conf = plugin.pub.conf;
    path root(conf->get_root());
    std::string title = plt_get_title(plt_in);
    std::string escaped = title;
    escape(escaped);
    std::string pl_to(root / escaped);
    pl_to += ".";
    pl_to += ext;

    auto plug_logger = spdlog::get(DDB_OWS_PROJECT_ID);

    plug_logger->debug("Saving playlist to {}", pl_to);
    int out = 0;

    char* fmt = ddb->tf_compile(conf->get_fn_formats()[0].c_str());
    if (fmt == nullptr) {
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
                out_path.replace_extension(conf->get_conv_ext());
            }

            ddb->pl_replace_meta(new_it, ":URI", out_path.c_str());
            after = ddb->plt_insert_item(plt_out, after, new_it);
            ddb->pl_item_unref(new_it);
        }
        free(its);

        head = ddb->plt_get_head_item(plt_out, PL_MAIN);
        tail = ddb->plt_get_tail_item(plt_out, PL_MAIN);
        out = ddb->plt_save(
            plt_out, head, tail, pl_to.c_str(), nullptr, nullptr, nullptr
        );
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

bool _save_playlists(
    bool dry,
    const std::vector<ddb_playlist_t*>& playlists,
    const char* ext,
    Logger& logger,
    playlist_save_cb_t callback
) {
    // returns true if all playlists were successfully saved
    bool out = true;
    for (ddb_playlist_t* plt : playlists) {
        bool saved = save_playlist(ext, plt, logger, dry);
        out = out && saved;
    }
    return out;
}

bool save_playlists(
    bool dry,
    const std::vector<ddb_playlist_t*>& playlists,
    Logger& logger,
    playlist_save_cb_t callback
) {
    bool out = true;
    auto conf = plugin.pub.conf;
    if (conf->get_sync_pls().dbpl) {
        out = out && _save_playlists(dry, playlists, "dbpl", logger, callback);
    }
    if (conf->get_sync_pls().m3u8) {
        out = out && _save_playlists(dry, playlists, "m3u8", logger, callback);
    }
    return out;
}

struct job_source {
    std::shared_ptr<ddb_playItem_t> it;
};

// Returns false if cancelled, true if successful
bool queue_jobs(
    bool dry,
    const std::vector<ddb_playlist_t*>& playlists,
    Logger& logger,
    sources_gathered_cb_t gathered_cb,
    job_queued_cb_t queued_cb,
    queueing_complete_cb_t complete_cb
) {
    auto jobs = plugin.jobs;
    if (!jobs->empty()) {
        // To avoid double-queueing
        return false;
    }

    ddb_ows_plugin_int* ddb_ows =
        (ddb_ows_plugin_int*)ddb->plug_get_for_id("ddb_ows");
    auto plug_logger = ddb_ows->logger;

    auto conf = plugin.pub.conf;
    path root(conf->get_root());
    DatabaseHandle db = std::make_shared<Database>(root);

    const auto tf_str = conf->get_fn_formats()[0];
    const auto cover_sync = conf->get_cover_sync();
    const auto cover_fname = conf->get_cover_fname();
    const auto rm_unref = conf->get_rm_unref();
    // Use 0 as a dummy value for dry runs. Probably would be more correct to
    // use a sum type, but then we have to touch all the Job interfaces.
    const auto sync_id =
        dry ? std::make_optional<sync_id_t>(0)
            : db->new_sync(tf_str, cover_sync, cover_fname, rm_unref);

    if (!sync_id) {
        logger.err("Could not create a new sync in the database.");
        return false;
    }
    char* fmt = ddb->tf_compile(tf_str.c_str());
    if (fmt == nullptr) {
        return false;
    }
    jobs->open();

    // We need to lock the playlist to avoid data races as we're looping over
    // it, but cover requests run async in another thread and ALSO need to lock
    // the playlist. Therefore we have to queue up items to dispatch cover
    // requests for once we are done traversing the playlist.
    std::unordered_set<path> cover_dirs{};
    std::deque<std::shared_ptr<DB_playItem_t>> cover_its{};

    const auto conv_settings = make_encoder_settings();

    // std::vector<ddb_playItem_t*> its;
    std::vector<job_source> sources;

    ddb->pl_lock();
    for (auto plt : playlists) {
        plug_logger->debug(
            "Looking for jobs from playlist {}", plt_get_title(plt)
        );

        DB_playItem_t* it;
        it = ddb->plt_get_first(plt, PL_MAIN);
        while (it != nullptr) {
            auto p = std::shared_ptr<DB_playItem_t>(it, ddb->pl_item_unref);
            sources.push_back({.it = p});
            it = ddb->pl_get_next(it, PL_MAIN);
        }
    }
    ddb->pl_unlock();
    if (gathered_cb) {
        gathered_cb(sources.size());
    }

    std::unordered_set<std::string> visited_sources{};

    ddb_ows->cancellationtoken = std::make_shared<CancellationToken>();
    const bool artwork_available = ddb_artwork != nullptr;
    for (auto source : sources) {
        auto it = source.it.get();
        path from;
        path to;
        from = std::string(ddb->pl_find_meta(it, ":URI"));
        if (!dry) {
            db->register_file(from);
        }
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
                make_job(
                    db, jobs, logger, it, *sync_id, from, to, conv_settings
                );
            } catch (std::filesystem::filesystem_error& e) {
                logger.err("Could not queue job for {}: {}", from, e.what());
                continue;
            }
        }

        if (queued_cb) {
            queued_cb();
        }

        path target_dir = to.parent_path();
        if (cover_sync && !cover_dirs.count(target_dir)) {
            cover_its.push_back(source.it);
            if (artwork_available) {
                if (gathered_cb) {
                    gathered_cb(sources.size() + cover_dirs.size());
                }
                plug_logger->debug("Copying cover to {}", target_dir);
                cover_dirs.insert(target_dir);
            } else {
                logger.warn(
                    "Would sync cover to directory {}, but artwork plugin is "
                    "not available.",
                    target_dir
                );
            }
        }
    }
    if (ddb_ows->cancellationtoken->get()) {
        plug_logger->debug("Cancelled while queueing jobs");
        free(fmt);
        return false;
    }

    // Now we can dispatch cover requests
    if (artwork_available &&
        !queue_cover_jobs(dry, logger, db, *sync_id, cover_its, queued_cb))
    {
        free(fmt);
        return false;
    }
    // TODO: delete unreferenced files
    jobs->close();
    const size_t n_jobs = jobs->size();
    if (complete_cb) {
        complete_cb(n_jobs);
    }
    plug_logger->debug("Found {} jobs", n_jobs);
    free(fmt);
    return true;
}

bool worker_thread(bool dry, job_finished_cb_t callback) {
    std::unique_ptr<Job> job;
    while ((job = plugin.jobs->pop())) {
        // unique_ptr is falsey if there is no object
        bool status = job->run(dry);
        if (callback) {
            // callback is falsy if the function object is empty
            callback(std::move(job), status);
        }
    }
    return true;
}

bool execute(bool dry, job_finished_cb_t callback) {
    auto conf = plugin.pub.conf;
    ddb_ows_plugin_int* ddb_ows =
        (ddb_ows_plugin_int*)ddb->plug_get_for_id("ddb_ows");
    int n_wts = conf->get_conv_wts();
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

bool run(
    bool dry,
    const std::vector<ddb_playlist_t*>& playlists,
    Logger& logger,
    callback_t callbacks
) {
    return save_playlists(dry, playlists, logger, callbacks.on_playlist_save) &&
           queue_jobs(
               dry,
               playlists,
               logger,
               callbacks.on_sources_gathered,
               callbacks.on_job_queued,
               callbacks.on_queueing_complete
           ) &&
           execute(dry, callbacks.on_job_finished);
}

bool cancel(cancel_cb_t callback) {
    ddb_ows_plugin_int* ddb_ows =
        (ddb_ows_plugin_int*)ddb->plug_get_for_id("ddb_ows");
    ddb_ows->logger->debug("Cancelling");
    ddb_ows->cancellationtoken->cancel();
    plugin.jobs->cancel();
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

void init(DB_functions_t* api) {
    plugin.logger = spdlog::stderr_color_mt(DDB_OWS_PROJECT_ID);
    plugin.logger->set_level(spdlog::level::DDB_OWS_LOGLEVEL);
    plugin.logger->set_pattern("[%n] [%^%l%$] [thread %t] %v");

    plugin.jobs = std::make_shared<JobsQueue>();

    plugin.pub.conf = std::make_shared<Configuration>(api);
    plugin.pub.conf->load_conf();
}

DB_plugin_t* load(DB_functions_t* api) {
    ddb = api;
    init(api);
    return (DB_plugin_t*)&plugin;
}

int connect() {
    ddb_converter = (ddb_converter_t*)ddb->plug_get_for_id("converter");
    if (ddb_converter == nullptr) {
        plugin.logger->warn(
            "Converter plugin not available. Conversion jobs will be skipped."
        );
    }

    ddb_artwork = (ddb_artwork_plugin_t*)ddb->plug_get_for_id("artwork2");
    if (ddb_artwork == nullptr) {
        plugin.logger->warn(
            "Artwork plugin not available. Cover art will not be synced."
        );
    }
    plugin.jobs->close();
    spdlog::get(DDB_OWS_PROJECT_ID)->info("Initialized successfully.");
    return 0;
}

extern "C" DB_plugin_t* ddb_ows_load(DB_functions_t* api) { return load(api); }

}  // namespace ddb_ows
