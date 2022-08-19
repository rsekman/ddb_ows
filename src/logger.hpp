#ifndef DDB_OWS_LOGGER_HPP
#define DDB_OWS_LOGGER_HPP

#include <string>

class Logger {
    public:
        virtual ~Logger() {};
        virtual bool log(std::string message) = 0;
        virtual bool err(std::string message) = 0;
        virtual void clear() = 0;
};

class StdioLogger : public Logger {
    public:
        ~StdioLogger() { };
        bool log(std::string message);
        bool err(std::string message);
        void clear() {};
};

#endif
