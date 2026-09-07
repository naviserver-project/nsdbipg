#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
#
# Copyright 2006 (C) Stephen Deasey <sdeasey@gmail.com>

VERSION     = 0.3

ifndef NAVISERVER
    NAVISERVER  = /usr/local/ns
endif

ifndef PGINCLUDE
    PGINCLUDE	= /usr/include
endif

ifndef PGLIB
    PGLIB	= /usr/lib
endif

NSD		= $(NAVISERVER)/bin/nsd
CFLAGS      +=	-I$(PGINCLUDE)
MODNAME		= nsdbipg

MOD		= $(MODNAME).so
MODOBJS		= $(MODNAME).o
MODLIBS		= -lnsdbi -L$(PGLIB) -lpq


include $(NAVISERVER)/include/Makefile.module


#
# The Postgres database to use for testing.
#

export DBIPG_USER=dbipg
export DBIPG_PASSWORD=dbipg
export DBIPG_DBNAME=dbipg


NS_TEST_CFG	= -c -d -t tests/config.tcl
NS_TEST_ALL	= tests/all.tcl $(TCLTESTARGS)
LD_LIBRARY_PATH	= LD_LIBRARY_PATH="./::$$LD_LIBRARY_PATH"

test: all
	export $(LD_LIBRARY_PATH); $(NSD) $(NS_TEST_CFG) $(NS_TEST_ALL)

runtest: all
	export $(LD_LIBRARY_PATH); $(NSD) $(NS_TEST_CFG)

gdbtest: all
	@echo set args $(NS_TEST_CFG) $(NS_TEST_ALL) > gdb.run
	export $(LD_LIBRARY_PATH); gdb -x gdb.run $(NSD)
	rm gdb.run

gdbruntest: all
	@echo set args $(NS_TEST_CFG) $(NS_TEST_ALL) > gdb.run
	export $(LD_LIBRARY_PATH); gdb -x gdb.run $(NSD)
	rm gdb.run

memcheck: all
	export $(LD_LIBRARY_PATH); valgrind --tool=memcheck $(NSD) $(NS_TEST_CFG) $(NS_TEST_ALL)



SRCS = nsdbipg.c
EXTRA = README sample-config.tcl Makefile tests

dist: all
	rm -rf $(MODNAME)-$(VERSION)
	mkdir $(MODNAME)-$(VERSION)
	$(CP) $(SRCS) $(EXTRA) $(MODNAME)-$(VERSION)
	hg log > $(MODNAME)-$(VERSION)/ChangeLog
	tar czf $(MODNAME)-$(VERSION).tgz $(MODNAME)-$(VERSION)
