#include "gui/textbufferlogger.hpp"

namespace ddb_ows {

TextBufferLogger::TextBufferLogger(
    Glib::RefPtr<Gtk::TextBuffer> _buffer, Gtk::TextView* _view
) :
    buffer(_buffer), view(_view), m() {
    auto style_ctx = view->get_style_context();
    for (auto& i : loglevels) {
        auto& l = i.second;
        style_ctx->lookup_color(l.color_name, l.color);
        l.tag = buffer->create_tag(l.name);
        l.tag->property_foreground_rgba().set_value(l.color);
    }

    sig_clear.connect(sigc::mem_fun(*this, &TextBufferLogger::_clear));
    sig_flush.connect(sigc::mem_fun(*this, &TextBufferLogger::flush));
};

#define DDB_OWS_TBL_METHOD(l, e)                    \
    bool TextBufferLogger::l(std::string message) { \
        return enqueue(message, DDB_OWS_TBL_##e);   \
    }

DDB_OWS_TBL_METHOD(verbose, VERBOSE);
DDB_OWS_TBL_METHOD(log, LOG);
DDB_OWS_TBL_METHOD(warn, WARN);
DDB_OWS_TBL_METHOD(err, ERR);

void TextBufferLogger::set_level(loglevel_e level) {
    for (auto& l : loglevels) {
        l.second.tag->property_invisible().set_value(true);
    }
#define DDB_OWS_TBL_LL_SET(l) \
    case DDB_OWS_TBL_##l:     \
        loglevels[DDB_OWS_TBL_##l].tag->property_invisible().set_value(false);

    switch (level) {
        DDB_OWS_TBL_LL_SET(VERBOSE)
        DDB_OWS_TBL_LL_SET(LOG)
        DDB_OWS_TBL_LL_SET(WARN)
        DDB_OWS_TBL_LL_SET(ERR)
    }
}

const std::map<loglevel_e, loglevel_info_t>& TextBufferLogger::get_levels() {
    return loglevels;
}

bool TextBufferLogger::enqueue(std::string message, loglevel_e level) {
    std::lock_guard<std::mutex> lock(m);
    bool was_empty = q.empty();
    q.push({message, loglevels[level]});
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
        if (!end->get_deleted()) {
            buffer->insert_with_tag(
                end->get_iter(), message.message + "\n", message.level.tag
            );
        }
        logged = true;
    }
    if (logged && !end->get_deleted()) {
        view->scroll_to(end);
    }
}

void TextBufferLogger::clear() { sig_clear(); }

void TextBufferLogger::_clear() { buffer->set_text(""); }

}  // namespace ddb_ows
