#ifndef DDB_OWS_TEXTBUFFERLOGGER_HPP
#define DDB_OWS_TEXTBUFFERLOGGER_HPP

#include <condition_variable>

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
        std::mutex m;
        std::condition_variable c;
};

#endif
