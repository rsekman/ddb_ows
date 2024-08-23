#include "logger.hpp"

#include "log.hpp"

namespace ddb_ows {

bool StdioLogger::verbose(std::string message) {
    DDB_OWS_DEBUG << message << std::endl;
    return true;
};

bool StdioLogger::log(std::string message) {
    DDB_OWS_DEBUG << message << std::endl;
    return true;
};

bool StdioLogger::warn(std::string message) {
    DDB_OWS_WARN << message << std::endl;
    return true;
};
bool StdioLogger::err(std::string message) {
    DDB_OWS_ERR << message << std::endl;
    return true;
};

}  // namespace ddb_ows
