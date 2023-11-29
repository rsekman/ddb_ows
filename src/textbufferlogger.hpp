#ifndef DDB_OWS_TBL_HPP
#define DDB_OWS_TBL_HPP

#include <mutex>
#include <map>
#include <queue>

#include <glibmm/dispatcher.h>
#include <glibmm/refptr.h>
#include <gtkmm/textbuffer.h>
#include <gtkmm/textview.h>
#include <gtkmm/texttag.h>

#include "logger.hpp"

namespace ddb_ows {

using namespace Gtk;
using RGBA = Gdk::RGBA;
template<typename T>
using RefPtr = Glib::RefPtr<T>;

typedef struct {
    std::string name;
    std::string color_name;
    RGBA color;
    RefPtr<TextBuffer::Tag> tag;
} loglevel_info_t;

typedef struct {
    std::string message;
    loglevel_info_t& level;
} message_t;


typedef std::queue<message_t> TextBufferLoggerStream;


/*
class TextBufferLoggerStream {
    public:
        TextBufferLoggerStream(Glib::RefPtr<TextBuffer::Tag> _tag) :
            q(), sig(), tag(_tag)
        {} ;
        std::queue<std::string> q;
        Glib::Dispatcher sig;
        Glib::RefPtr<TextBuffer::Tag> tag;
};
*/

class TextBufferLogger : public Logger {
    public:
        TextBufferLogger(Glib::RefPtr<TextBuffer> _buffer, TextView* view) ;
        ~TextBufferLogger() {};
        bool verbose(std::string message);
        bool log(std::string message);
        bool warn(std::string message);
        bool err(std::string message);
        void clear();

    private :
        RefPtr<TextBuffer> buffer;
        TextView* view;
        std::mutex m;
        std::queue<message_t> q;

        Glib::Dispatcher sig_clear;
        void _clear();

        Glib::Dispatcher sig_flush;
        void flush();

        bool enqueue(std::string message, loglevel_e level);

        std::map<loglevel_e, loglevel_info_t> loglevels {
            {
                DDB_OWS_TBL_VERBOSE,
                {"Verbose", "insensitive_fg_color", RGBA("rgb( 94,  94,  94)") }
            },
            {
                DDB_OWS_TBL_LOG,
                {"Log",     "success_color",        RGBA("rgb(  0, 202,   0)") }
            },
            {
                DDB_OWS_TBL_WARN,
                {"Warning", "warning_color",        RGBA("rgb(202, 202,   0)") }
            },
            {
                DDB_OWS_TBL_ERR,
                {"Error",   "error_color",          RGBA("rgb(202,   0,   0)") },
            }
        };
};

}

#endif
