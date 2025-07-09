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
    cover_timeout_ms,
    sync_pls,
    rm_unref,
    conv_fts,
    conv_preset,
    conv_ext,
    conv_wts
)

Configuration::Configuration(DB_functions_t* api) : ddb(api) {
    auto res =
        Gio::Resource::lookup_data_global("/ddb_ows/default_config.json");
    auto size = res->get_size();
    auto default_buf = static_cast<const char*>(res->get_data(size));
    // This is safe because we fully control the input and can fix it at
    // compile-time if it's invalid
    _conf = json::parse(default_buf);
}

bool Configuration::load_conf() {
    ddb->conf_lock();
    const char* buf;
    buf = ddb->conf_get_str_fast(DDB_OWS_CONFIG_MAIN, "{}");
    load_conf_from_buffer(buf);
    ddb->conf_unlock();
    return true;
}

bool Configuration::load_conf_from_buffer(const char* buf) {
    auto logger = spdlog::get(DDB_OWS_PROJECT_ID);

    json upd;
    try {
        upd = json::parse(buf);
    } catch (json::exception& e) {
        logger->error("Configuration contains malformed JSON: {}", e.what());
        return false;
    } catch (std::exception& e) {
        logger->error("Error reading configuration: {}", e.what());
        return false;
    }
    if (!upd.is_object()) {
        logger->error(
            "Configuration is not a JSON object. Falling back to default."
        );
        return false;
    }
    // ensures proper copying of strings
    json conf = _conf;
    conf.merge_patch(upd);
    try {
        _conf = conf;
    } catch (json::exception& e) {
        logger->error(
            "Configuration is not valid: {}. Falling back to default.", e.what()
        );
        return false;
    }
    return true;
}

bool Configuration::write_conf() {
    std::string conf_str = json(_conf).dump();
    ddb->conf_set_str(DDB_OWS_CONFIG_MAIN, conf_str.c_str());
    return true;
}

}  // namespace ddb_ows
