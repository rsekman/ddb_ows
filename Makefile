.PHONY: clean install

PROJECT=ddb_ows
BUILDDIR=build
SRCDIR=src

UISRC = $(PROJECT).ui
OUT=$(addprefix $(BUILDDIR)/,$(PROJECT).so $(PROJECT)_gtk2.so $(PROJECT)_gtk3.so $(UISRC))
LIBDIR=$(HOME)/.local/lib

CXX?=g++
CFLAGS+=-Wall -g -O2 -fPIC -D_GNU_SOURCE -DLIBDIR="\"$(LIBDIR)\"" -pthread
LDFLAGS=-shared -export-dynamic

GTK2_DEPS_LIBS = $(shell pkg-config --cflags-only-I gtkmm-2.4)
GTK3_DEPS_LIBS = $(shell pkg-config --cflags-only-I gtkmm-3.0)

GTK2_LDFLAGS=$(shell pkg-config --libs gtkmm-2.4)
GTK3_LDFLAGS=$(shell pkg-config --libs gtkmm-3.0)

COMMONSOURCES=$(addprefix $(SRCDIR)/)
SOURCES?=$(addprefix $(SRCDIR)/,ddb_ows.cpp ddb_ows.hpp) $(COMMONSOURCES)
UISOURCES?=$(addprefix $(SRCDIR)/, ddb_ows_gui.cpp ddb_ows_gui.hpp) $(COMMONSOURCES)
OBJ?=$(patsubst $(SRCDIR)/%.cpp, $(BUILDDIR)/%.o, $(filter $(SRCDIR)/%.cpp,$(SOURCES)))
UIOBJ?=$(patsubst $(SRCDIR)/%.cpp, $(BUILDDIR)/%.o, $(filter $(SRCDIR)/%.cpp,$(COMMONSOURCES)))

all: $(OUT)

install: $(OUT)
	cp -t $(LIBDIR)/deadbeef $(OUT)

$(BUILDDIR)/$(PROJECT).so: $(OBJ)
	$(CXX) $(CFLAGS) $(LDFLAGS) $(OBJ) -o $@

$(BUILDDIR)/$(PROJECT)_gtk2.so: $(UIOBJ) $(BUILDDIR)/$(PROJECT)_gtk2.o
	$(CXX) $(CFLAGS) $(LDFLAGS) $(GTK2_DEPS_LIBS) $(GTK2_LDFLAGS) $^ -o $@

$(BUILDDIR)/$(PROJECT)_gtk3.so: $(UIOBJ) $(BUILDDIR)/$(PROJECT)_gtk3.o
	$(CXX) $(CFLAGS) $(LDFLAGS) $(GTK3_DEPS_LIBS) $(GTK3_LDFLAGS) $^ -o $@

$(BUILDDIR)/$(PROJECT)_gtk2.o: $(SRCDIR)/ddb_ows_gui.cpp $(SRCDIR)/ddb_ows_gui.hpp
	$(CXX) $(CFLAGS) $(GTK2_DEPS_LIBS) $(GTK2_LDFLAGS) $< -c -o $@

$(BUILDDIR)/$(PROJECT)_gtk3.o: $(SRCDIR)/ddb_ows_gui.cpp $(SRCDIR)/ddb_ows_gui.hpp
	$(CXX) $(CFLAGS) $(GTK3_DEPS_LIBS) $(GTK3_LDFLAGS) $< -c -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp $(SRCDIR)/%.hpp
	$(CXX) $(CFLAGS) $< -c -o $@


$(BUILDDIR)/$(UISRC): $(SRCDIR)/$(UISRC)
	cp $< $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)
