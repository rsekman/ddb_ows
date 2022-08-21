#include "textbufferlogger.hpp"
#include "log.hpp"

TextBufferLogger::TextBufferLogger(Glib::RefPtr<Gtk::TextBuffer> _buffer, Gtk::TextView* _view) :
    buffer(_buffer),
    view(_view),
    sig_log(),
    sig_err(),
    m(),
    q_log(), q_err()
{
    err_tag = buffer->create_tag("error");
    sig_log.connect(sigc::mem_fun(*this, &TextBufferLogger::_log));
    sig_err.connect(sigc::mem_fun(*this, &TextBufferLogger::_err));
} ;

bool TextBufferLogger::log(std::string message) {
    {
        std::lock_guard <std::mutex> lock(m);
        q_log.push(message);
    }
    sig_log();
    return true;
}

bool TextBufferLogger::err(std::string message) {
    {
        std::lock_guard <std::mutex> lock(m);
        q_err.push(message);
    }
    sig_err();
    return true;
}

void TextBufferLogger::_log() {
    std::string message = "";
    std::lock_guard<std::mutex> lock(m);
    if (q_log.empty()) {
        return;
    } else {
        message = q_log.front();
        q_log.pop();
    }
    DDB_OWS_DEBUG << message << std::endl;
    auto end = buffer->get_mark("END");
    if(!end->get_deleted()){
        buffer->insert(end->get_iter(), message + "\n");
        view->scroll_to(end);
    }
}

void TextBufferLogger::_err() {
    std::string message = "";
    std::lock_guard<std::mutex> lock(m);
    if (q_err.empty()) {
        return;
    } else {
        message = q_err.front();
        q_err.pop();
    }
    DDB_OWS_ERR << message << std::endl;
    auto end = buffer->get_mark("END");
    if(!end->get_deleted()){
        buffer->insert_with_tag(end->get_iter(), message + "\n", err_tag);
        view->scroll_to(end);
    }
}

void TextBufferLogger::clear() {
    buffer->set_text("");
}
