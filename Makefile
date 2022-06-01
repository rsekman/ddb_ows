.PHONY: clean install

PROJECT=ddb_ows
BUILDDIR=build
SRCDIR=src

UISRC = $(PROJECT).ui
TARGETS=$(PROJECT).so $(PROJECT)_gtk2.so $(PROJECT)_gtk3.so $(UISRC)
OUT=$(addprefix $(BUILDDIR)/,$(TARGETS))
LIBDIR=$(HOME)/.local/lib
INSTALLDIR=$(LIBDIR)/deadbeef

CXX?=clang++
CFLAGS+=-Wall --std=c++17 -g -fPIC -DLIBDIR="\"$(LIBDIR)\"" -Wno-deprecated-declarations
LDFLAGS=-shared -rdynamic


GTK2_CFLAGS=$(shell pkg-config --cflags-only-I gtkmm-2.4)
GTK2_CFLAGS+=-DDDB_OWS_LIB_FILE="\"$(INSTALLDIR)/$(PROJECT)_gtk2.so\""
GTK2_LDFLAGS=$(shell pkg-config --libs gtkmm-2.4)

GTK3_CFLAGS=$(shell pkg-config --cflags-only-I gtkmm-3.0)
GTK3_CFLAGS+=-DDDB_OWS_LIB_FILE="\"$(INSTALLDIR)/$(PROJECT)_gtk3.so\""
GTK3_LDFLAGS=$(shell pkg-config --libs gtkmm-3.0)

SOURCES?=$(addprefix $(SRCDIR)/,ddb_ows.cpp config.cpp)
UISOURCES?=$(addprefix $(SRCDIR)/, ddb_ows_gui.cpp)
OBJ?=$(patsubst $(SRCDIR)/%.cpp, $(BUILDDIR)/%.o, $(filter $(SRCDIR)/%.cpp,$(SOURCES)))
GTK2OBJ?=$(patsubst $(SRCDIR)/%.cpp, $(BUILDDIR)/%_gtk2.o, $(filter $(SRCDIR)/%.cpp,$(UISOURCES)))
GTK2OBJ+=$(BUILDDIR)/config.o
GTK3OBJ?=$(patsubst $(SRCDIR)/%.cpp, $(BUILDDIR)/%_gtk3.o, $(filter $(SRCDIR)/%.cpp,$(UISOURCES)))


all: $(OUT)

install: $(addprefix install-,main gtk2 gtk3)

install-main: $(INSTALLDIR)/ddb_ows.so

install-gtk2: $(INSTALLDIR)/$(PROJECT)_gtk2.so $(INSTALLDIR)/$(UISRC)

install-gtk3: $(INSTALLDIR)/$(PROJECT)_gtk3.so $(INSTALLDIR)/$(UISRC)


$(INSTALLDIR)/%: $(BUILDDIR)/%
	cp -t $(INSTALLDIR) $<

$(BUILDDIR)/$(PROJECT).so: $(OBJ)
	$(CXX) $(CFLAGS) $(LDFLAGS) $(OBJ) -o $@

$(BUILDDIR)/$(PROJECT)_gtk2.so: $(GTK2OBJ)
	$(CXX) $(CFLAGS) $(LDFLAGS) $(GTK2_CFLAGS) $(GTK2_LDFLAGS) $^ -o $@

$(BUILDDIR)/$(PROJECT)_gtk3.so: $(GTK3OBJ)
	$(CXX) $(CFLAGS) $(LDFLAGS) $(GTK3_CFLAGS) $(GTK3_LDFLAGS) $^ -o $@

$(BUILDDIR)/%_gtk2.o: $(SRCDIR)/%.cpp $(SRCDIR)/%.hpp
	$(CXX) $(CFLAGS) $(GTK2_CFLAGS) $< -c -o $@

$(BUILDDIR)/%_gtk3.o: $(SRCDIR)/%.cpp $(SRCDIR)/%.hpp
	$(CXX) $(CFLAGS) $(GTK3_CFLAGS) $< -c -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp $(SRCDIR)/%.hpp
	$(CXX) $(CFLAGS) $< -c -o $@


$(BUILDDIR)/$(UISRC): $(SRCDIR)/$(UISRC)
	cp $< $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)
