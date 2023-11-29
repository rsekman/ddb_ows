#include <ctime>
#include <cstring>
#include <condition_variable>
#include <filesystem>
#include <fmt/std.h>
#include <functional>
#include <limits.h>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

#include "ddb_ows.hpp"
#include "constants.hpp"
#include "config.hpp"
#include "database.hpp"
#include "job.hpp"
#include "jobsqueue.hpp"
#include "log.hpp"
#include "playlist_uuid.hpp"

#include <deadbeef/converter.h>
#include <deadbeef/artwork.h>


using namespace std::chrono_literals;
using namespace std::chrono;
using namespace std::filesystem;


namespace ddb_ows{

static DB_functions_t* ddb;
Configuration conf = Configuration();

// TODO: move these out of global scope

ddb_artwork_plugin_t* ddb_artwork = NULL;
ddb_converter_t* ddb_converter = NULL;

std::random_device rd;
std::mt19937 mersenne_twister(rd());
auto dist = std::uniform_int_distribution<long>(LONG_MIN, LONG_MAX);

void escape(std::string& s) {
    for (auto i = s.begin(); i != s.end(); i++) {
        switch (*i) {
            case '/':
            case '\\':
            case ':':
            case '*':
            case '?':
            case '"':
            case '<':
            case '>':
            case '|':
                *i = '-';
        }
    }
}

std::string get_output_path(DB_playItem_t* it, char* format) {
    DB_playItem_t* copy = ddb->pl_item_alloc();
    //std::basic_regex escape("[/\\:*?\"<>|]");
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
        //TODO change this?
        .idx = 0,
        .id = 0,
        .iter = PL_MAIN,
    };
    ddb->tf_eval(&ctx, format, out, sizeof(out));
    ddb->pl_item_unref(copy);
    return std::string(out);
}

JobsQueue* jobs = new JobsQueue();

typedef struct cover_req_s {
    std::mutex* m;
    std::condition_variable* c;
    bool returned;
    ddb_cover_info_t* cover;
} cover_req_t;

void callback_cover_art_found (int error, ddb_cover_query_t *query, ddb_cover_info_t *cover) {
    cover_req_t* creq = (cover_req_t*) (query->user_data);
    creq->returned = true;
    std::lock_guard<std::mutex> lock(*creq->m);
    if (
        (query->flags & DDB_ARTWORK_FLAG_CANCELLED) ||
        cover == NULL ||
        cover->image_filename == NULL
    ) {
        creq->cover = NULL;
    } else {
        DDB_OWS_DEBUG << "Found cover: " << cover->image_filename << std::endl;
        creq->cover = cover;
    }
    creq->c->notify_all();
    ddb->pl_item_unref(query->track);
    free(query);
}

bool is_newer(path a, path b) {
    return last_write_time(a) > last_write_time(b);
}

bool queue_cover_jobs(Logger& logger, Database* db, std::deque<DB_playItem_t*> items) {
    path from;
    path to;
    char* fmt = ddb->tf_compile(conf.get_fn_formats()[0].c_str());
    if (fmt == NULL) {
        return false;
    }
    auto root = path(conf.get_root());
    std::mutex m {};
    std::condition_variable c {};
    DB_playItem_t* it;
    while (!items.empty() ) {
        it = items.front();
        from = ddb->pl_find_meta (it, ":URI");
        to = root / get_output_path(it, fmt);
        path target_dir = to.parent_path();
        ddb_cover_query_t* cover_query = (ddb_cover_query_t*) calloc(sizeof(ddb_cover_query_t), 1);
        cover_query->flags = 0;
        cover_query->track = it;
        int64_t sid = dist(mersenne_twister);
        cover_query->source_id = sid;
        cover_query->_size = sizeof(ddb_cover_query_t);
        cover_req_t creq = {
            .m = &m,
            .c = &c,
            .returned = false,
            .cover = NULL
        };
        cover_query->user_data = &creq;
        std::unique_lock<std::mutex> lock(*creq.m);
        ddb_artwork->cover_get(cover_query, callback_cover_art_found);
        creq.c->wait_for(
            lock,
            DDB_OWS_COVER_TIMEOUT,
            [&creq] {
            return creq.returned;
            }
        );
        if (creq.returned && creq.cover != NULL) {
            path from = creq.cover->image_filename;
            path to = target_dir / conf.get_cover_fname();
            auto old = db->find_entry(from);
            if (exists(to) && last_write_time(to) > last_write_time(from)) {
                logger.verbose("Cover at {} is newer than source {}", to, from);
            } else if ( old != db->end()
                && old->second.destination != to
                && exists(old->second.destination)
                && is_newer(old->second.destination, from)
            ) {
                auto cover_job = std::unique_ptr<Job> (
                    new MoveJob(logger, db, old->second.destination, to, from)
                );
                jobs->push(std::move(cover_job));
            } else {
                auto cover_job = std::unique_ptr<Job>(
                    new CopyJob(
                        logger,
                        db,
                        creq.cover->image_filename,
                        to
                    )
                );
                jobs->push(std::move(cover_job));
            }
        } else if (!creq.returned) {
            DDB_OWS_DEBUG
                << "Cover request timed out for" << target_dir
                << " after "
                << duration_cast<milliseconds>(DDB_OWS_COVER_TIMEOUT).count()
                << std::endl;
        } else {
            DDB_OWS_DEBUG << "No cover found for " << target_dir << std::endl;
        }
        // it is unref'd in the callback; we don't need to do it here
        items.pop_front();
    }
    return true;
}

bool cancel_jobs () {
    jobs->cancel();
    return true;
}

ddb_converter_settings_t make_encoder_settings() {
    auto preset = ddb_converter->encoder_preset_get_list();
    auto sel = conf.get_conv_preset();
    ddb_converter_settings_t out {
        // these two mean to use the same sample format as input
        .output_bps = -1,
        .output_is_float = -1,
        .encoder_preset = NULL,
        .dsp_preset = NULL,
        .bypass_conversion_on_same_format = 0,
        .rewrite_tags_after_copy = 0,
    };
    while (preset != NULL) {
        if (sel == std::string(preset->title) ) {
            out.encoder_preset = preset;
            break;
        }
        preset = preset->next;
    }
    // TODO: DSP preset
    return out;
}

bool should_convert(DB_playItem_t* it){
    DB_decoder_t **decoders = ddb->plug_get_decoder_list ();
    // decoders and decoders[i]->exts are null-terminated arrays
    int i = 0;
    std::string::size_type n;
    std::set<std::string> sels = conf.get_conv_fts();
    const char *fname = ddb->pl_find_meta (it, ":URI");
    const char *ext = strrchr (fname, '.');
    if (ext) {
        ext++;
    } else {
        DDB_OWS_WARN << "Unable to determine filetype for " << fname << std::endl;
        return false;
    }
    while (decoders[i]) {
        std::string s(decoders[i]->plugin.name);
        if (
            (n = s.find(" decoder")) != std::string::npos ||
            (n = s.find(" player"))  != std::string::npos
        ) {
            s = s.substr(0, n);
        }
        if (sels.count(s)) {
            const char **exts = decoders[i]->exts;
            if (exts) {
                for (int j = 0; exts[j]; j++) {
                    if (!strcasecmp (exts[j], ext) || !strcmp (exts[j], "*")) {
                        return true;
                    }
                }
            }
        }
        i++;
    }
    return false;
}

std::vector<std::unique_ptr<Job>> make_job(
    Database* db,
    Logger& logger,
    DB_playItem_t* it,
    path from,
    path to,
    ddb_converter_settings_t conv_settings
) {
    auto old = db->find_entry(from);
    std::vector<std::unique_ptr<Job>> out {};
    if (should_convert(it) ) {
        to.replace_extension(conf.get_conv_ext());
        std::string preset_title = conv_settings.encoder_preset->title;
        std::unique_ptr<Job> cjob(
            new ConvertJob(logger, db, ddb, conv_settings, it, from, to)
        );
        if(old != db->end() && old->second.converter_preset == preset_title) {
            // This source file was synced previously and the same encoder preset is selected
            if ( exists(to) && is_newer(to, from) ) {
                // The destination exists and is newer than the source
                logger.verbose(
                    "Source {} was already converted with {}; skipping.", from, preset_title
                );
                return {};
            } else if(
                exists(old->second.destination)
                && is_newer(old->second.destination, from)
            ) {
                // The source was previously converted with a different destination, which is newer than the source
                out.push_back( std::unique_ptr<Job>(
                    new MoveJob(logger, db, old->second.destination, to, from)
                ));
            } else {
                // The source is newer => reconvert with new destination and delete the old destination
                if (old->second.destination != to) {
                    out.push_back( std::unique_ptr<Job> (
                        new DeleteJob(logger, db, old->second.destination)
                    ));
                }
                out.push_back(std::move(cjob));
            }
        } else {
            out.push_back(std::move(cjob));
        }
    } else if ( old != db->end()
        && old->second.destination != to
        && exists(old->second.destination)
    ) {
        // This source file was synced previously,
        if (is_newer(old->second.destination, from)) {
            // the destination file is newer than the source => move
            out.push_back( std::unique_ptr<Job>(
                new MoveJob(logger, db, old->second.destination, to, from)
            ));
        } else {
            // the source file is newer than the old copy => copy anew and delete the old copy
            out.push_back( std::unique_ptr<Job> (
                new CopyJob(logger, db, from, to)
            ));
            DDB_OWS_DEBUG << "Making delete job" << std::endl;
            out.push_back( std::unique_ptr<Job> (
                new DeleteJob(logger, db, old->second.destination)
            ));
        }
    }  else if (exists(to) && is_newer(to, from)) {
            logger.verbose(
                "Destination {} is newer than source {}; skipping.", to, from
            );
    } else {
        out.push_back(std::unique_ptr<Job>( new CopyJob(logger, db, from, to)));
    }
    return out;
}

std::string plt_get_title(ddb_playlist_t* plt) {
    char plt_title[PATH_MAX];
    ddb->plt_get_title(plt, plt_title, sizeof(plt_title));
    std::string out(plt_title);
    out.shrink_to_fit();
    return out;
}

bool save_playlist(ddb_playlist_t* plt, Logger& logger, bool dry) {
    path root(conf.get_root());
    std::string title = plt_get_title(plt);
    std::string escaped = title;
    escape(escaped);
    std::string pl_to(root / escaped);
    pl_to += ".dbpl";
    DDB_OWS_DEBUG << "Saving playlist to" << pl_to << std::endl;
    int out = 0;
    if (!dry) {
        auto head = ddb->plt_get_head_item(plt, PL_MAIN);
        auto tail = ddb->plt_get_tail_item(plt, PL_MAIN);
        if (!head || !tail) {
            logger.warn("Playlist {} is empty, not saving.", title);
            return false;
        }
        out = ddb->plt_save(plt, head, tail, pl_to.c_str(), NULL, NULL, NULL);
        ddb->pl_item_unref(head);
        ddb->pl_item_unref(tail);
    }
    if (out < 0) {
        logger.err("Failed to save playlist {}.", title);
        return false;
    } else {
        logger.log("Saved playlist {}", title);
        return true;
    }
}

bool save_playlists(std::vector<ddb_playlist_t*> playlists, Logger& logger, bool dry) {
    // returns true if all playlists were successfully saved
    bool out = true;
    for( ddb_playlist_t* plt : playlists ) {
        bool saved = save_playlist(plt, logger, dry);
        out = out && saved;
    }
    return out;
}

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
    std::unordered_set<path> cover_dirs {};
    std::deque<DB_playItem_t*> cover_its {};

    ddb_ows_plugin_t* ddb_ows = (ddb_ows_plugin_t*) ddb->plug_get_for_id("ddb_ows");
    path root(conf.get_root());
    ddb_ows->db = new Database(root);

    auto conv_settings = make_encoder_settings();

    std::vector<ddb_playItem_t*> its;

    ddb->pl_lock();
    for( auto plt : playlists) {
        DDB_OWS_DEBUG << "Looking for jobs from playlist " << plt_get_title(plt) << std::endl;

        DB_playItem_t* it;
        it = ddb->plt_get_first(plt, PL_MAIN);
        while (it != NULL) {
            its.push_back(it);
            it = ddb->pl_get_next(it, PL_MAIN);
        }
    }
    ddb->pl_unlock();

    for( auto it : its) {
        path from;
        path to;
        from = std::string(ddb->pl_find_meta (it, ":URI"));
        to = root / get_output_path(it, fmt);
        if (!exists(from)) {
            logger.err("Source file {} does not exist!", from);
        } else {
            auto new_jobs = make_job(ddb_ows->db, logger, it, from, to, conv_settings);
            for (auto j = new_jobs.begin(); j != new_jobs.end(); j++) {
                jobs->push(std::move(*j));
            }
        }

        path target_dir = to.parent_path();
        if ( ddb_ows->conf.get_cover_sync()  && !cover_dirs.count(target_dir)) {
            cover_its.push_back(it);
            ddb->pl_item_ref(it);
            DDB_OWS_DEBUG << "Copying cover to " << target_dir << std::endl;
            cover_dirs.insert(target_dir);
        }
        ddb->pl_item_unref(it);
    }

    // Now we can dispatch cover requests
    queue_cover_jobs(logger, ddb_ows->db, cover_its);
    // TODO: delete unreferenced files
    jobs->close();
    DDB_OWS_DEBUG << "Found " << jobs->size() << " jobs" << std::endl;
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
    while( (job = jobs->pop()) ) {
        // unique_ptr is falsey if there is no object
        bool status = job->run(dry);
        if ( status && callback ) {
            // callback is falsy if the function object is empty
            callback(std::move(job));
        }
    }
    return true;
}

bool run(bool dry, job_cb_t callback) {
    ddb_ows_plugin_t* ddb_ows = (ddb_ows_plugin_t*) ddb->plug_get_for_id("ddb_ows");
    int n_wts = conf.get_conv_wts();
    ddb_ows->worker_thread_futures.futures.clear();
    for(int i = 0; i < n_wts; i++) {
        auto task = worker_thread_t(worker_thread);
        ddb_ows->worker_thread_futures.futures.push_back(
            task.get_future()
        );
        std::thread t(std::move(task), dry, callback);
        t.detach();
    }
    std::lock_guard lock(ddb_ows->worker_thread_futures.m);
    for(
        auto t = ddb_ows->worker_thread_futures.futures.begin();
        t != ddb_ows->worker_thread_futures.futures.end();
        t++
    ){
        t->wait();
    }
    ddb_ows->worker_thread_futures.c.notify_all();
    delete ddb_ows->db;
    return true;
}

void cancel_thread(cancel_cb_t callback) {
    ddb_ows_plugin_t* ddb_ows = (ddb_ows_plugin_t*) ddb->plug_get_for_id("ddb_ows");
    jobs->cancel();
    std::lock_guard lock(ddb_ows->worker_thread_futures.m);
    for(
        auto t = ddb_ows->worker_thread_futures.futures.begin();
        t != ddb_ows->worker_thread_futures.futures.end();
        t++
    ){
        t->wait();
    }
    callback();
    ddb_ows->worker_thread_futures.c.notify_all();
}

bool cancel(cancel_cb_t callback) {
    auto t = std::thread(cancel_thread, callback);
    t.detach();
    return true;
}

int start() {
    return 0;
}

int stop() {
    return 0;
}

int disconnect(){
    return 0;
}

int connect(){
    ddb_converter = (ddb_converter_t*) ddb->plug_get_for_id ("converter");
    ddb_artwork = (ddb_artwork_plugin_t*) ddb->plug_get_for_id ("artwork2");
    jobs->close();
    return 0;
}

int handleMessage(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2){
    return 0;
}

plt_uuid _plt_get_uuid(ddb_playlist_t* plt) {
    return plt_get_uuid(plt, ddb);
}

const char* configDialog_ = "";

ddb_ows_plugin_t plugin = {
    .plugin = {
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
    },
    .conf = conf,
    .db = NULL,
    .worker_thread_futures = wt_futures_t {
        .m = std::mutex(),
        .c = std::condition_variable(),
        .futures = std::vector<std::shared_future<bool>> {}
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
    plugin.conf.update_conf();
}

DB_plugin_t* load(DB_functions_t* api) {
    ddb = api;
    init(api);
    return (DB_plugin_t*) &plugin;
}

extern "C" DB_plugin_t* ddb_ows_load(DB_functions_t* api) {
    return load(api);
}

}
