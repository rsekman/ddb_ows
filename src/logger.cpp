#include "logger.hpp"

#include "log.hpp"

namespace ddb_ows {

bool StdioLogger::verbose(std::string message) {
    DDB_OWS_DEBUG("{}", message);
    return true;
};

bool StdioLogger::log(std::string message) {
    DDB_OWS_DEBUG("{}", message);
    return true;
};

bool StdioLogger::warn(std::string message) {
    DDB_OWS_WARN("{}", message);
    return true;
};
bool StdioLogger::err(std::string message) {
    DDB_OWS_ERR("{}", message);
    return true;
};

}  // namespace ddb_ows
