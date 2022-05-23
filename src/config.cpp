#include "ddb_ows.hpp"
#include "config.hpp"

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace ddb_ows{

json read_config_json(DB_functions_t* ddb_api) {
    ddb_api->conf_lock();
    const char* buf;
    buf = ddb_api->conf_get_str_fast( DDB_OWS_CONFIG_MAIN, "{}" );
    ddb_api->conf_unlock();
    json out;
    try {
        out = json(buf);
        if (!out.is_object()) {
            throw std::invalid_argument("Configuration is not a JSON object.");
        }
        return out;
    } catch (json::exception& e) {
        DDB_OWS_ERR << "Configuration contains malformed JSON: " << e.what() << std::endl;
        return json {};
    } catch (std::exception& e) {
        DDB_OWS_ERR << "Error reading configuration: " << e.what() << std::endl;
        return json {};
    }
}

bool write_config_json(DB_functions_t* ddb_api, json config) {
    std::string conf_str(config);
    ddb_api->conf_set_str( DDB_OWS_CONFIG_MAIN, conf_str.c_str() );
    return true;
}

template <typename T>
T get_with_default(json conf, std::string key, T def) {
    if (conf.contains(key) ) {
        return conf[key];
    } else {
        return def;
    }
}

void Configuration::set_api(DB_functions_t* ddb_api){
    this->ddb_api = ddb_api;
}

// root
std::string Configuration::get_root() {
    json conf = read_config_json(this->ddb_api);
    return get_with_default(conf, DDB_OWS_CONFIG_KEY_ROOT, std::string(""));
}

bool Configuration::set_root(std::string root) {
    json conf = read_config_json(this->ddb_api);
    conf[DDB_OWS_CONFIG_KEY_ROOT] = root;
    return write_config_json(this->ddb_api, conf);
}

// fn_formats
std::vector<std::string> Configuration::get_fn_formats() {
    json conf = read_config_json(this->ddb_api);
    std::vector<std::string> def = {};
    return get_with_default(conf, DDB_OWS_CONFIG_KEY_FN_FORMATS, def);
}

bool Configuration::set_fn_formats(std::vector<std::string> formats) {
    json conf = read_config_json(this->ddb_api);
    conf[DDB_OWS_CONFIG_KEY_FN_FORMATS] = formats;
    return write_config_json(this->ddb_api, conf);
}

bool Configuration::get_cover_sync() {
    json conf = read_config_json(this->ddb_api);
    return get_with_default(conf, DDB_OWS_CONFIG_KEY_COVER_SYNC, true);
}

bool Configuration::set_cover_sync(bool sync) {
    json conf = read_config_json(this->ddb_api);
    conf[DDB_OWS_CONFIG_KEY_COVER_SYNC] = sync;
    return write_config_json(this->ddb_api, conf);
}

std::string Configuration::get_cover_fname() {
    json conf = read_config_json(this->ddb_api);
    return get_with_default<std::string>(
        conf,
        DDB_OWS_CONFIG_KEY_COVER_FNAME,
        DDB_OWS_CONFIG_KEY_COVER_FNAME_DEFAULT
   );
}

bool Configuration::set_cover_fname(std::string fname) {
    json conf = read_config_json(this->ddb_api);
    conf[DDB_OWS_CONFIG_KEY_COVER_FNAME] = fname;
    return write_config_json(this->ddb_api, conf);
}

// ft_selection
std::map<std::string, bool> Configuration::get_ft_selection() {
    json conf = read_config_json(this->ddb_api);
    std::map<std::string, bool> def = {};
    return get_with_default(conf, DDB_OWS_CONFIG_KEY_FT_SEL, def);
}

bool Configuration::set_ft_selection(std::map<std::string, bool> sel) {
    json conf = read_config_json(this->ddb_api);
    conf[DDB_OWS_CONFIG_KEY_FT_SEL] = sel;
    return write_config_json(this->ddb_api, conf);
}

// conv_presets
std::string Configuration::get_preset() {
    json conf = read_config_json(this->ddb_api);
    return get_with_default(conf, DDB_OWS_CONFIG_KEY_CONV_PRESET, std::string(""));
}

bool Configuration::set_preset(std::string preset) {
    json conf = read_config_json(this->ddb_api);
    conf[DDB_OWS_CONFIG_KEY_CONV_PRESET] = preset;
    return write_config_json(this->ddb_api, conf);
}

// worker_threads
int Configuration::get_worker_threads() {
    json conf = read_config_json(this->ddb_api);
    return get_with_default(conf, DDB_OWS_CONFIG_KEY_WT, -1);
}

bool Configuration::set_worker_threads(int worker_threads) {
    json conf = read_config_json(this->ddb_api);
    conf[DDB_OWS_CONFIG_KEY_WT] = worker_threads;
    return write_config_json(this->ddb_api, conf);
}

}
