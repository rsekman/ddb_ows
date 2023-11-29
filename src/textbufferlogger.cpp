#include "textbufferlogger.hpp"

namespace ddb_ows {

TextBufferLogger::TextBufferLogger(Glib::RefPtr<Gtk::TextBuffer> _buffer, Gtk::TextView* _view) :
    buffer(_buffer),
    view(_view),
    m()
{

    auto style_ctx = view->get_style_context();
    for (auto &i : loglevels ) {
        auto &l = i.second;
        style_ctx->lookup_color(l.color_name, l.color);
        l.tag = buffer->create_tag(l.name);
        l.tag->property_foreground_rgba().set_value(l.color);
    }

    sig_clear.connect(sigc::mem_fun(*this, &TextBufferLogger::_clear));
    sig_flush.connect(sigc::mem_fun(*this, &TextBufferLogger::flush));
} ;

bool TextBufferLogger::verbose(std::string message) {
    return enqueue(message, DDB_OWS_TBL_VERBOSE);
}

bool TextBufferLogger::log(std::string message) {
    return enqueue(message, DDB_OWS_TBL_LOG);
}

bool TextBufferLogger::warn(std::string message) {
    return enqueue(message, DDB_OWS_TBL_WARN);
}

bool TextBufferLogger::err(std::string message) {
    return enqueue(message, DDB_OWS_TBL_ERR);
}

bool TextBufferLogger::enqueue(std::string message, loglevel_e level) {
    std::lock_guard <std::mutex> lock(m);
    bool was_empty = q.empty();
    q.push( {message, loglevels[level]} );
    if (was_empty) {
        sig_flush();
    }
    return true;
}

void TextBufferLogger::flush() {
    std::lock_guard<std::mutex> lock(m);
    auto end = buffer->get_mark("END");
    bool logged = false;
    while (!q.empty()) {
        auto message = q.front();
        q.pop();
        if(!end->get_deleted()){
            buffer->insert_with_tag(
                end->get_iter(),
                message.message + "\n",
                message.level.tag
            );
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

}
