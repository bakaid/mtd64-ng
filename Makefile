BINARY_MTD64NG = mtd64-ng
BINARY_FAKEDNS = fakedns
OBJECTS_COMMON = pool.o dns.o
OBJECTS_MTD64NG = main.o server.o query.o dnsclient.o
OBJECTS_FAKEDNS = main.o
HEADERS_COMMON = pool.h dns.h
HEADERS_MTD64NG = server.h query.h dnsclient.h dnssource.h
HEADERS_FAKEDNS =

OBJECTS_MTD64NG := $(patsubst %.o,$(BINARY_MTD64NG)_%.o,$(OBJECTS_MTD64NG))
OBJECTS_FAKEDNS := $(patsubst %.o,$(BINARY_FAKEDNS)_%.o,$(OBJECTS_FAKEDNS))

CXX = clang++
CXXFLAGS = -std=c++11 -O3 -Wall -Wdeprecated -pedantic -g
LDFLAGS = -g -lpthread

CONFIG = conf/mtd64-ng.conf
CONFIGDIR = /etc
PREFIX = /usr

SRCDIR = src
MTD64NGDIR = mtd64-ng
FAKEDNSDIR = fakedns

.PHONY: all install clean

all: $(BINARY_MTD64NG) $(BINARY_FAKEDNS)

install: all
	install -m 0755 $(BINARY_MTD64NG) $(PREFIX)/sbin
	install -m 0755 $(BINARY_FAKEDNS) $(PREFIX)/sbin
	install -m 0644 $(CONFIG) $(CONFIGDIR)

clean:
	rm -f $(BINARY_MTD64NG) $(BINARY_FAKEDNS) $(OBJECTS_COMMON) $(OBJECTS_MTD64NG) $(OBJECTS_FAKEDNS)

$(BINARY_MTD64NG): $(OBJECTS_COMMON) $(OBJECTS_MTD64NG)
	$(CXX) $(LDFLAGS) $(OBJECTS_COMMON) $(OBJECTS_MTD64NG) -o $@

$(BINARY_FAKEDNS): $(OBJECTS_COMMON) $(OBJECTS_FAKEDNS)
	$(CXX) $(LDFLAGS) $(OBJECTS_COMMON) $(OBJECTS_FAKEDNS) -o $@

%.o: $(SRCDIR)/%.cpp $(patsubst %.h,$(SRCDIR)/%.h,$(HEADERS_COMMON))
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BINARY_MTD64NG)_%.o: $(SRCDIR)/$(MTD64NGDIR)/%.cpp $(patsubst %.h,$(SRCDIR)/$(MTD64NGDIR)/%.h,$(HEADERS_MTD64NG))
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BINARY_FAKEDNS)_%.o: $(SRCDIR)/$(FAKEDNSDIR)/%.cpp $(patsubst %.h,$(SRCDIR)/$(FAKEDNSDIR)/%.h,$(HEADERS_FAKEDNS))
	$(CXX) $(CXXFLAGS) -c $< -o $@
