BINARY = mtd64-ng
OBJECTS = main.o pool.o server.o dns.o query.o dnsclient.o
HEADERS = pool.h dns.h server.h query.h

CXX = clang++
CXXFLAGS = -std=c++11 -O3 -Wall -Wdeprecated -pedantic -g
LDFLAGS = -g -lpthread

CONFIG = mtd64-ng.conf
CONFIGDIR = /etc
PREFIX = /usr

.PHONY: all install clean

all: $(BINARY)

install: all
	install -m 0755 $(BINARY) $(PREFIX)/sbin
	install -m 0644 $(CONFIG) $(CONFIGDIR)

clean:
	rm -f $(BINARY) $(OBJECTS)

$(BINARY): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@
