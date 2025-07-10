#ifndef DDB_OWS_CONFIG_H

#define DDB_OWS_CONFIG_H

#include <mutex>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "deadbeef/deadbeef.h"
#include "playlist_uuid.hpp"

#define DDB_OWS_CONFIG_MAIN "ddb_ows.settings"

#define DDB_OWS_CONFIG_METHODS(name, type) \
    type get_##name() {                    \
        std::lock_guard lk(m);             \
        return _conf.name;                 \
    };                                     \
    bool set_##name(type val) {            \
        std::lock_guard lk(m);             \
        _conf.name = val;                  \
        write_conf();                      \
        return true;                       \
    };

namespace ddb_ows {

typedef struct sync_pls_s {
    bool dbpl;
    bool m3u8;
} sync_pls_t;

typedef struct {
    std::string root;
    std::vector<std::string> fn_formats;
    std::unordered_set<plt_uuid> pl_selection;
    bool cover_sync;
    std::string cover_fname;
    unsigned int cover_timeout_ms;
    sync_pls_t sync_pls;
    bool rm_unref;
    std::set<std::string> conv_fts;
    std::string conv_preset;
    std::string conv_ext;
    int conv_wts;
} ddb_ows_config;

class Configuration {
  public:
    // plumbing
    Configuration(DB_functions_t* api);
    ddb_ows_config get();
    void set(const ddb_ows_config& c);
    bool load_conf();

    DDB_OWS_CONFIG_METHODS(root, std::string)
    DDB_OWS_CONFIG_METHODS(fn_formats, std::vector<std::string>)
    DDB_OWS_CONFIG_METHODS(pl_selection, std::unordered_set<plt_uuid>)
    DDB_OWS_CONFIG_METHODS(cover_sync, bool)
    DDB_OWS_CONFIG_METHODS(cover_fname, std::string)
    DDB_OWS_CONFIG_METHODS(cover_timeout_ms, unsigned int)
    DDB_OWS_CONFIG_METHODS(sync_pls, sync_pls_t)
    DDB_OWS_CONFIG_METHODS(rm_unref, bool)
    DDB_OWS_CONFIG_METHODS(conv_fts, std::set<std::string>)
    DDB_OWS_CONFIG_METHODS(conv_preset, std::string)
    DDB_OWS_CONFIG_METHODS(conv_ext, std::string)
    DDB_OWS_CONFIG_METHODS(conv_wts, int)

  private:
    DB_functions_t* ddb;
    ddb_ows_config _conf;
    bool load_conf_from_buffer(const char* buf);
    bool write_conf();

    std::mutex m;
};

}  // namespace ddb_ows

#endif
