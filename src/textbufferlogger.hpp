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

class TextBufferLogger : public Logger {
    public:
        TextBufferLogger(Glib::RefPtr<Gtk::TextBuffer> _buffer, Gtk::TextView* view) ;
        ~TextBufferLogger() {};
        bool log(std::string message);
        bool err(std::string message);
        void clear();
    private :
        Glib::RefPtr<Gtk::TextBuffer> buffer;
        Glib::RefPtr<Gtk::TextBuffer::Tag> err_tag;
        Gtk::TextView* view;
        Glib::Dispatcher sig_log;
        Glib::Dispatcher sig_err;
        std::string _message;
        void _log();
        void _err();
        std::mutex m;
        std::queue<std::string> q_log;
        std::queue<std::string> q_err;
};

#endif
