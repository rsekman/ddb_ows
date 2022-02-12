.PHONY: clean install

CXX?=g++
CFLAGS+=-Wall -g -O2 -fPIC -D_GNU_SOURCE
LDFLAGS+=-shared

BUILDDIR=build
SRCDIR=src

SOCK=/tmp/ddb_socket

OUT=$(BUILDDIR)/ddb_ows.so

SOURCES?=$(wildcard $(SRCDIR)/*.cpp)
COMMON=$(SRCDIR)/defs.hpp
OBJ?=$(patsubst $(SRCDIR)/%.cpp, $(BUILDDIR)/%.o, $(SOURCES))

all: $(OUT)

install: $(OUT)
	cp -t ~/.local/lib/deadbeef $(OUT)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp $(SRCDIR)/%.hpp $(COMMON)
	$(CXX) $(CFLAGS) $< -c -o $@

$(OUT): $(OBJ) $(BUILDDIR)
	$(CXX) $(CFLAGS$) $(LDFLAGS) $(OBJ) -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)
	rm -f $(SOCK)
