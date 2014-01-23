#
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://mozilla.org/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and limitations
# under the License.
#
# Copyright 2006 (C) Stephen Deasey <sdeasey@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms
# of the GNU General Public License (the "GPL"), in which case the
# provisions of GPL are applicable instead of those above.  If you wish
# to allow use of your version of this file only under the terms of the
# GPL and not to allow others to use your version of this file under the
# License, indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by the GPL.
# If you do not delete the provisions above, a recipient may use your
# version of this file under either the License or the GPL.
#
#

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
