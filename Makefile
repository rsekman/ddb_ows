.PHONY: clean install

PROJECT=ddb_ows
BUILDDIR=build
SRCDIR=src

UISRC = $(PROJECT).ui
TARGETS=$(PROJECT).so $(PROJECT)_gtk3.so $(UISRC)
OUT=$(addprefix $(BUILDDIR)/,$(TARGETS))
LIBDIR=$(HOME)/.local/lib
INSTALLDIR=$(LIBDIR)/deadbeef

CXX=clang++
CFLAGS+=-Wall --std=c++17 -g -fPIC -DLIBDIR="\"$(LIBDIR)\"" -Wno-deprecated-declarations -gdwarf-4
LDFLAGS=-shared -rdynamic -Wl,-E -lfmt -luuid

ifneq ($(DDB_OWS_LOGLEVEL),)
	CFLAGS+=-DDDB_OWS_LOGLEVEL=$(DDB_OWS_LOGLEVEL)
endif

PCHS=builder.h

GTK3_CFLAGS=$(shell pkg-config --cflags-only-I gtkmm-3.0)
GTK3_CFLAGS+=-DDDB_OWS_LIB_FILE='"$(INSTALLDIR)/$(PROJECT)_gtk3.so"'
GTK3_PCHS=$(patsubst %.h,$(BUILDDIR)/gtkmm-3.0/%.h.pch,$(PCHS))
GTK3_PCH_FLAGS=$(addprefix -include-pch ,$(GTK3_PCHS))
GTK3_LDFLAGS=$(shell pkg-config --libs gtkmm-3.0)

OBJ:=ddb_ows.o config.o default_config.o job.o jobsqueue.o logger.o database.o playlist_uuid.o
GUIOBJ:=textbufferlogger progressmonitor ddb_ows_gui
GTK3OBJ:=$(addprefix $(BUILDDIR)/, $(OBJ) $(addsuffix _gtk3.o, $(GUIOBJ)))
OBJ:=$(addprefix $(BUILDDIR)/, $(OBJ))


all: $(OUT)

install: $(addprefix install-,main gtk3)

install-main: $(INSTALLDIR)/ddb_ows.so

install-gtk3: $(INSTALLDIR)/$(PROJECT)_gtk3.so $(INSTALLDIR)/$(UISRC)


$(INSTALLDIR)/%: $(BUILDDIR)/%
	cp -t $(INSTALLDIR) $<

$(BUILDDIR)/$(PROJECT).so: $(OBJ)
	$(CXX) $^ $(CFLAGS) $(LDFLAGS) -o $@

$(BUILDDIR)/$(PROJECT)_gtk3.so: $(GTK3OBJ)
	$(CXX) $^ $(CFLAGS) $(LDFLAGS) $(GTK3_CFLAGS) $(GTK3_LDFLAGS) -o $@

$(BUILDDIR)/%_gtk3.o: $(SRCDIR)/%.cpp $(SRCDIR)/%.hpp
	$(CXX) $(CFLAGS) $(GTK3_CFLAGS) $< -c -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp $(SRCDIR)/%.hpp
	$(CXX) $(CFLAGS) $< -c -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.S $(SRCDIR)/%.json
	$(CC) '-DFNAME="$(SRCDIR)/$*.json"' $< -c -o $@

$(BUILDDIR)/gtkmm-3.0/%.h.pch: $(subst -I,,$(firstword $(GTK3_CFLAGS)))/gtkmm/%.h
	mkdir -p $(@D)
	$(CXX) -x c++-header $(CFLAGS) $(GTK3_CFLAGS) $^ -o $@

.PRECIOUS: $(GTK3_PCHS)

$(BUILDDIR)/$(UISRC): $(SRCDIR)/$(UISRC)
	cp $< $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)/gtkmm-2.4/
	rm $(BUILDDIR)/*.o $(BUILDDIR)/*.so
