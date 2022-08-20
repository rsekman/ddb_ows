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
    success = buffer->insert(buffer->end(), message + "\n");
    view->scroll_to(buffer->get_mark("END"));
    c.notify_one();
    return success;
}

bool TextBufferLogger::err(std::string message) {
    bool success;
    DDB_OWS_ERR << message << std::endl;
    std::unique_lock<std::mutex> lock(m);
    success = buffer->insert_with_tag(buffer->end(), message + "\n", err_tag);
    view->scroll_to(buffer->get_mark("END"));
    c.notify_one();
    return success;
}

void TextBufferLogger::clear() {
    buffer->set_text("");
}
