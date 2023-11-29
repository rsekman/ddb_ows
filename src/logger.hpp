#ifndef DDB_OWS_LOGGER_HPP
#define DDB_OWS_LOGGER_HPP

#include <string>

namespace ddb_ows{

enum loglevel_e {
    DDB_OWS_TBL_VERBOSE,
    DDB_OWS_TBL_LOG,
    DDB_OWS_TBL_WARN,
    DDB_OWS_TBL_ERR,
};

class Logger {
    public:
        virtual ~Logger() {};
        virtual bool verbose(std::string message) = 0;
        virtual bool log(std::string message) = 0;
        virtual bool warn(std::string message) = 0;
        virtual bool err(std::string message) = 0;
        virtual void clear() = 0;
};

class StdioLogger : public Logger {
    public:
        ~StdioLogger() { };
        bool verbose(std::string message);
        bool log(std::string message);
        bool warn(std::string message);
        bool err(std::string message);
        void clear() {};
};

}

#endif
