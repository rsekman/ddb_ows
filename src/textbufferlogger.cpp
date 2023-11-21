#include "textbufferlogger.hpp"

TextBufferLogger::TextBufferLogger(Glib::RefPtr<Gtk::TextBuffer> _buffer, Gtk::TextView* _view) :
    buffer(_buffer),
    view(_view),
    log_stream( {} ),
    err_stream( {} ),
    m()
{
    auto err_tag = buffer->create_tag("error");
    err_stream.tag = err_tag;
    log_stream.sig.connect(sigc::mem_fun(*this, &TextBufferLogger::_log));
    err_stream.sig.connect(sigc::mem_fun(*this, &TextBufferLogger::_err));
    sig_clear.connect(sigc::mem_fun(*this, &TextBufferLogger::_clear));
} ;

bool TextBufferLogger::log(std::string message) {
    return enqueue(message, log_stream);
}

bool TextBufferLogger::err(std::string message) {
    return enqueue(message, err_stream);
}

bool TextBufferLogger::enqueue(std::string message, TextBufferLoggerStream& stream) {
    std::lock_guard <std::mutex> lock(m);
    bool was_empty = stream.q.empty();
    stream.q.push(message);
    if (was_empty) {
        stream.sig();
    }
    return true;
}

void TextBufferLogger::_log() {
    flush(log_stream);
}
void TextBufferLogger::_err() {
    flush(err_stream);
}

void TextBufferLogger::flush(TextBufferLoggerStream& stream) {
    std::string message = "";
    std::lock_guard<std::mutex> lock(m);
    auto end = buffer->get_mark("END");
    bool logged = false;
    while (!stream.q.empty()) {
        message = stream.q.front();
        stream.q.pop();
        if(!end->get_deleted()){
            buffer->insert(end->get_iter(), message + "\n");
        }
        logged = true;
    }
    if(logged && !end->get_deleted()){
        view->scroll_to(end);
    }
}

void TextBufferLogger::clear() {
    sig_clear();
}

void TextBufferLogger::_clear() {
    buffer->set_text("");
}
