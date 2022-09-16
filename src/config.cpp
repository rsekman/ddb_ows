#include "config.hpp"

#include <nlohmann/json.hpp>

using nlohmann::json;

extern char DDB_OWS_CONFIG_DEFAULT;

namespace ddb_ows{

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    ddb_ows_config,
    root,
    fn_formats,
    cover_sync,
    cover_fname,
    rm_unref,
    conv_fts,
    conv_preset,
    conv_ext,
    conv_wts
)

Configuration::Configuration() {
    _update_conf(&DDB_OWS_CONFIG_DEFAULT);
}

void Configuration::set_api(DB_functions_t* api) {
    ddb = api;
}

bool Configuration::update_conf() {
    ddb->conf_lock();
    const char* buf;
    buf = ddb->conf_get_str_fast( DDB_OWS_CONFIG_MAIN, "{}" );
    _update_conf(buf);
    ddb->conf_unlock();
    return true;
}

bool Configuration::_update_conf(const char* buf) {
    json upd;
    try {
        upd = json::parse(buf);
    } catch (json::exception& e) {
        DDB_OWS_ERR << "Configuration contains malformed JSON: " << e.what() << std::endl;
    } catch (std::exception& e) {
        DDB_OWS_ERR << "Error reading configuration: " << e.what() << std::endl;
    }
    if (!upd.is_object()) {
        DDB_OWS_ERR << "Configuration is not a JSON object. Falling back to default configuration.\n";
        return _update_conf(&DDB_OWS_CONFIG_DEFAULT);
    }
    // ensures proper copying of strings
    json conf = {};
    conf.merge_patch(upd);
    try {
        _conf = conf;
    } catch (json::exception& e) {
        DDB_OWS_ERR << "Configuration from DeaDBeeF is not valid. Falling back to default configuration.\n";
        return _update_conf(&DDB_OWS_CONFIG_DEFAULT);
    }
    return true;
}

bool Configuration::write_conf() {
    std::string conf_str = json(_conf).dump();
    ddb->conf_set_str( DDB_OWS_CONFIG_MAIN, conf_str.c_str() );
    return true;
}

}
