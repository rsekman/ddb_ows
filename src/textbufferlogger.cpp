#include "textbufferlogger.hpp"
#include "log.hpp"

#include <mutex>
TextBufferLogger::TextBufferLogger(Glib::RefPtr<Gtk::TextBuffer> _buffer, Gtk::TextView* _view) :
    buffer(_buffer),
    view(_view),
    m(),
    c()
{
    err_tag = buffer->create_tag("error");

} ;
bool TextBufferLogger::log(std::string message) {
    bool success;
    DDB_OWS_DEBUG << message << std::endl;
    std::unique_lock<std::mutex> lock(m);
    auto end = buffer->get_mark("END");
    if(!end->get_deleted()){
        success = buffer->insert(end->get_iter(), message + "\n");
        view->scroll_to(end);
    } else {
        success = false;
    }
    c.notify_one();
    return success;
}

bool TextBufferLogger::err(std::string message) {
    bool success;
    DDB_OWS_ERR << message << std::endl;
    std::unique_lock<std::mutex> lock(m);
    auto end = buffer->get_mark("END");
    if(!end->get_deleted()){
        success = buffer->insert_with_tag(end->get_iter(), message + "\n", err_tag);
        view->scroll_to(end);
    } else {
        success = false;
    }
    c.notify_one();
    return success;
}

void TextBufferLogger::clear() {
    buffer->set_text("");
}
