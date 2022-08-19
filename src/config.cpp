#include "config.hpp"

extern char DDB_OWS_CONFIG_DEFAULT;

namespace ddb_ows{

Configuration::Configuration() {
    default_conf = json::parse(&DDB_OWS_CONFIG_DEFAULT);
    conf = {};
    // ensures proper copying
    conf.merge_patch(default_conf);
}

void Configuration::set_api(DB_functions_t* api) {
    ddb_api = api;
}

bool Configuration::update_conf() {
    ddb_api->conf_lock();
    const char* buf;
    buf = ddb_api->conf_get_str_fast( DDB_OWS_CONFIG_MAIN, "{}" );
    json upd;
    try {
        upd = json::parse(buf);
    } catch (json::exception& e) {
        DDB_OWS_ERR << "Configuration contains malformed JSON: " << e.what() << std::endl;
    } catch (std::exception& e) {
        DDB_OWS_ERR << "Error reading configuration: " << e.what() << std::endl;
    }
    ddb_api->conf_unlock();
    if (!upd.is_object()) {
        DDB_OWS_ERR << "Configuration is not a JSON object. Falling back to default configuration\n.";
        upd = default_conf;
    }
    conf.merge_patch(upd);
    return true;
}

bool Configuration::write_conf() {
    std::string conf_str = conf.dump();
    ddb_api->conf_set_str( DDB_OWS_CONFIG_MAIN, conf_str.c_str() );
    return true;
}

}
