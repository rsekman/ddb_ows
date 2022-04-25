.PHONY: clean install

PROJECT=ddb_ows
BUILDDIR=build
SRCDIR=src

OUT=$(addprefix $(BUILDDIR)/,$(PROJECT).so $(PROJECT)_gtk2.so $(PROJECT)_gtk3.so)

CXX?=g++
CFLAGS+=-Wall -g -O2 -fPIC -D_GNU_SOURCE
LDFLAGS+=-shared

GTK2_DEPS_CFLAGS = -I/usr/include/gtk-2.0 -I/usr/lib/gtk-2.0/include -I/usr/include/pango-1.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/sysprof-4 -I/usr/include/harfbuzz -I/usr/include/freetype2 -I/usr/include/libpng16 -I/usr/include/libmount -I/usr/include/blkid -I/usr/include/fribidi -I/usr/include/cairo -I/usr/include/lzo -I/usr/include/pixman-1 -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/atk-1.0 -pthread
GTK2_DEPS_LIBS = -lgtk-x11-2.0 -lgdk-x11-2.0 -lpangocairo-1.0 -latk-1.0 -lcairo -lgdk_pixbuf-2.0 -lgio-2.0 -lpangoft2-1.0 -lpango-1.0 -lgobject-2.0 -lharfbuzz -lfontconfig -lfreetype -lgthread-2.0 -pthread -lglib-2.0 -lglib-2.0
GTK3_DEPS_CFLAGS = -I/usr/include/gtk-3.0 -I/usr/include/pango-1.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/sysprof-4 -I/usr/include/harfbuzz -I/usr/include/freetype2 -I/usr/include/libpng16 -I/usr/include/libmount -I/usr/include/blkid -I/usr/include/fribidi -I/usr/include/cairo -I/usr/include/lzo -I/usr/include/pixman-1 -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/gio-unix-2.0 -I/usr/include/cloudproviders -I/usr/include/atk-1.0 -I/usr/include/at-spi2-atk/2.0 -I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include -I/usr/include/at-spi-2.0 -pthread
GTK3_DEPS_LIBS = -lgtk-3 -lgdk-3 -lz -lpangocairo-1.0 -lpango-1.0 -lharfbuzz -latk-1.0 -lcairo-gobject -lcairo -lgdk_pixbuf-2.0 -lgthread-2.0 -pthread -lglib-2.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0


COMMONSOURCES=$(addprefix $(SRCDIR)/, plugin.cpp plugin.hpp)
SOURCES?=$(addprefix $(SRCDIR)/,ddb_ows.cpp ddb_ows.hpp) $(COMMONSOURCES)
UISOURCES?=$(addprefix $(SRCDIR)/, ddb_ows_gui.cpp ddb_ows_gui.hpp) $(COMMONSOURCES)
OBJ?=$(patsubst $(SRCDIR)/%.cpp, $(BUILDDIR)/%.o, $(filter $(SRCDIR)/%.cpp,$(SOURCES)))
UIOBJ?=$(patsubst $(SRCDIR)/%.cpp, $(BUILDDIR)/%.o, $(filter $(SRCDIR)/%.cpp,$(UISOURCES)))

all: $(OUT)

install: $(OUT)
	cp -t ~/.local/lib/deadbeef $(OUT)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp $(SRCDIR)/%.hpp
	$(CXX) $(CFLAGS) $< -c -o $@

$(BUILDDIR)/$(PROJECT).so: $(OBJ)
	$(CXX) $(CFLAGS$) $(LDFLAGS) $(OBJ) -o $@

$(BUILDDIR)/$(PROJECT)_gtk2.so: $(OBJ)
	$(CXX) $(CFLAGS$) $(GTK2_DEPS_LIBS) $(GTK2_DEPS_CFLAGS) $(LDFLAGS) $(OBJ) -o $@

$(BUILDDIR)/$(PROJECT)_gtk3.so: $(OBJ)
	$(CXX) $(CFLAGS$) $(GTK3_DEPS_LIBS) $(GTK3_DEPS_CFLAGS) $(LDFLAGS) $(OBJ) -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)
