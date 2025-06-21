#include "config.hpp"

#include <giomm/resource.h>
#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>
#include <stdexcept>

using nlohmann::json;

namespace ddb_ows {

void to_json(json& j, const plt_uuid& uuid) { j = uuid.str(); }

void from_json(const json& j, plt_uuid& uuid) {
    if (!j.is_string()) {
        auto e = std::invalid_argument(
            "plt_uuid: uuid must be string but is " + std::string(j.type_name())
        );
        throw e;
    }
    uuid_t id;
    if (uuid_parse(std::string(j).c_str(), id) < 0) {
        throw std::invalid_argument("plt_uuid: provided uuid is invalid.");
    } else {
        uuid = plt_uuid(id);
    }
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(sync_pls_s, dbpl, m3u8);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    ddb_ows_config,
    root,
    pl_selection,
    fn_formats,
    cover_sync,
    cover_fname,
    sync_pls,
    rm_unref,
    conv_fts,
    conv_preset,
    conv_ext,
    conv_wts
)

Configuration::Configuration() { _update_conf_with_default(); }

void Configuration::set_api(DB_functions_t* api) { ddb = api; }

bool Configuration::update_conf() {
    ddb->conf_lock();
    const char* buf;
    buf = ddb->conf_get_str_fast(DDB_OWS_CONFIG_MAIN, "{}");
    _update_conf(buf);
    ddb->conf_unlock();
    return true;
}

bool Configuration::_update_conf(const char* buf) {
    auto logger = spdlog::get(DDB_OWS_PROJECT_ID);
    json upd;
    try {
        upd = json::parse(buf);
    } catch (json::exception& e) {
        logger->error("Configuration contains malformed JSON: {}", e.what());
    } catch (std::exception& e) {
        logger->error("Error reading configuration: {}", e.what());
    }
    if (!upd.is_object()) {
        logger->error(
            "Configuration is not a JSON object. Falling back to default "
            "configuration."
        );
        return _update_conf_with_default();
    }
    // ensures proper copying of strings
    json conf = {};
    conf.merge_patch(upd);
    try {
        _conf = conf;
    } catch (json::exception& e) {
        logger->error(
            "Configuration from DeaDBeeF is not valid. Falling back "
            "to default configuration."
        );
        return _update_conf_with_default();
    }
    return true;
}

bool Configuration::_update_conf_with_default() {
    auto default_conf =
        Gio::Resource::lookup_data_global("/ddb_ows/default_config.json");
    auto size = default_conf->get_size();
    _update_conf(static_cast<const char*>(default_conf->get_data(size)));
    return true;
}

bool Configuration::write_conf() {
    std::string conf_str = json(_conf).dump();
    ddb->conf_set_str(DDB_OWS_CONFIG_MAIN, conf_str.c_str());
    return true;
}

}  // namespace ddb_ows
