#include "log.hpp"
#include "logger.hpp"

bool StdioLogger::log(std::string message) {
    DDB_OWS_DEBUG << message << std::endl;
    return true;
};

bool StdioLogger::err(std::string message) {
    DDB_OWS_ERR << message << std::endl;
    return true;
};
