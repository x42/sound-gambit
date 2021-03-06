PREFIX ?= /usr/local
bindir = $(PREFIX)/bin
mandir = $(PREFIX)/share/man/man1

PKG_CONFIG ?= pkg-config
CXXFLAGS ?= -Wall -O3

VERSION=0.2

###############################################################################

ifeq ($(shell $(PKG_CONFIG) --exists sndfile || echo no), no)
  $(error "http://www.mega-nerd.com/libsndfile/ is required - install libsndfile1-dev")
endif

CPPFLAGS+=-DVERSION=\"$(VERSION)\"
CXXFLAGS+=`$(PKG_CONFIG) --cflags sndfile`
LOADLIBES=`$(PKG_CONFIG) --libs sndfile` -lm

all: sound-gambit

man: sound-gambit.1

sound-gambit: sound-gambit.cc peaklim.cc

sound-gambit.1: sound-gambit
	help2man -N -n 'Audio File Peak Limiter' -o sound-gambit.1 ./sound-gambit

clean:
	rm -f sound-gambit

install: install-bin install-man

uninstall: uninstall-bin uninstall-man

install-bin: sound-gambit
	install -d $(DESTDIR)$(bindir)
	install -m755 sound-gambit $(DESTDIR)$(bindir)

uninstall-bin:
	rm -f $(DESTDIR)$(bindir)/sound-gambit
	-rmdir $(DESTDIR)$(bindir)

install-man:
	install -d $(DESTDIR)$(mandir)
	install -m644 sound-gambit.1 $(DESTDIR)$(mandir)

uninstall-man:
	rm -f $(DESTDIR)$(mandir)/sound-gambit.1
	-rmdir $(DESTDIR)$(mandir)

.PHONY: all clean install uninstall man install-man install-bin uninstall-man uninstall-bin
