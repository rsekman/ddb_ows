#ifndef DDB_OWS_TEXTBUFFERLOGGER_HPP
#define DDB_OWS_TEXTBUFFERLOGGER_HPP

#include <mutex>
#include <queue>

#include <glibmm/dispatcher.h>
#include <glibmm/refptr.h>
#include <gtkmm/textbuffer.h>
#include <gtkmm/textview.h>
#include <gtkmm/texttag.h>

#include "logger.hpp"

class TextBufferLoggerStream {
    public:
        TextBufferLoggerStream(Glib::RefPtr<Gtk::TextBuffer::Tag> _tag) :
            q(), sig(), tag(_tag)
        {} ;
        std::queue<std::string> q;
        Glib::Dispatcher sig;
        Glib::RefPtr<Gtk::TextBuffer::Tag> tag;
};

class TextBufferLogger : public Logger {
    public:
        TextBufferLogger(Glib::RefPtr<Gtk::TextBuffer> _buffer, Gtk::TextView* view) ;
        ~TextBufferLogger() {};
        bool log(std::string message);
        bool err(std::string message);
        void clear();
    private :
        Glib::RefPtr<Gtk::TextBuffer> buffer;
        Gtk::TextView* view;
        TextBufferLoggerStream log_stream;
        TextBufferLoggerStream err_stream;
        Glib::Dispatcher sig_clear;
        bool enqueue(std::string message, TextBufferLoggerStream& stream);
        void flush(TextBufferLoggerStream& stream);
        void _log();
        void _err();
        void _clear();
        std::mutex m;
};

#endif
