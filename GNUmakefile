# GNU Make-specific makefile for GNU Make.

# Copyright (C) 1990, 1991, 1992, 1993, 1994 Free Software Foundation, Inc.
# This file is part of GNU Make.
# 
# GNU Make is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# GNU Make is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with GNU Make; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

.PHONY: default
default:

# Set `ARCH' to a string for the type of machine.
ifndef ARCH
ifdef machine
ARCH = $(machine)
endif # machine
endif # not ARCH

override srcdir := .
override CC := $(CC)

ifeq ($(ARCH),hp300)
#customs=yes
endif
ifdef customs
override REMOTE := cstms
else
override REMOTE := stub
endif

# Get most of the information from the Unix-compatible makefile.
include compatMakefile

MAKE = $(MAKE_COMMAND) $(MAKEOVERRIDES)

# Remove autoconf magic.
extras := $(filter-out getloadavg.o @%@,$(extras)) getloadavg.o
LOADLIBES := $(filter-out @%@,$(LOADLIBES))
ALLOCA := $(filter-out @%@,$(ALLOCA))

# Tell autoconf/autoheader to use m4 files from the master source.
ifndef AC_MACRODIR
AC_MACRODIR = /home/gd/gnu/autoconf
endif
ACFLAGS := -m $(AC_MACRODIR)
configure config.h.in: $(patsubst %,$(AC_MACRODIR)/%.m4,acspecific acgeneral)
config.h.in: $(AC_MACRODIR)/acconfig.h
configure: configure.in; autoconf $(ACFLAGS)
config.h.in: configure.in; autoheader $(ACFLAGS)

ifdef customs
defines := $(defines) -Ipmake/customs -Ipmake/lib/include
LOADLIBES := $(addprefix pmake/customs/,customslib.o rpc.o xlog.o) \
	     pmake/lib/sprite/libsprite.a
endif

ifdef ARCH

ifndef no_libc
libc_dir = /home/gd2/gnu/libc/$(ARCH)
ifneq ($(wildcard $(libc_dir)),)
ifneq ($(wildcard $(libc_dir)/works-for-make),)
#CPPFLAGS := -I$(libc_dir)/include
#LDFLAGS := -nostdlib $(libc_dir)/lib/start.o
#LOADLIBES := $(LOADLIBES) \
#	     $(libc_dir)/lib/mcheck-init.o \
#	     $(libc_dir)/lib/libc.a \
#	     -lgcc \
#	     $(libc_dir)/lib/libc.a
CC := $(CC) -b glibc

# getopt is in libc.
GETOPT =
#GETOPT_SRC = Don't clear this or dist will break.

# glob is in libc too.
GLOB = 

else

CPPFLAGS := $(CPPFLAGS) -Iglob

endif	 # works-for-make
endif	 # $(libc_dir)
endif	 # !no_libc

# We know the type of machine, so put the binaries in subdirectories.
$(ARCH)/%.o: %.c
	$(COMPILE.c) -Iglob $< $(OUTPUT_OPTION)
$(ARCH)/glob/libglob.a: FORCE
	$(MAKE) -C $(@D) $(@F) \
		CC='$(CC)' CFLAGS='$(CFLAGS) -I..' \
		CPPFLAGS='$(CPPFLAGS) -DHAVE_CONFIG_H'
FORCE:
objs := $(addprefix $(ARCH)/,$(objs))
prog := $(ARCH)/make

archpfx = $(ARCH)/

$(archpfx)load.o: load.c
	$(COMPILE.c) $(LOAD_AVG) $< -o $@
$(archpfx)load.dep: load.c
	$(mkdep) $(LOAD_AVG) $< | sed 's,$*\.o,& $@,' > $@

CPPFLAGS := -I$(ARCH) $(CPPFLAGS) -DHAVE_CONFIG_H $(filter-out @%@,$(defines))

ifneq "$(wildcard $(ARCH)/makefile)" ""
include $(ARCH)/makefile
endif
objs := $(objs) $(addprefix $(ARCH)/,$(ALLOCA) $(extras))

else # Not ARCH
prog := make
endif

ifneq	"$(findstring gcc,$(CC))" ""
CFLAGS = -g -W -Wunused -Wpointer-arith -Wreturn-type -Wswitch
else
CFLAGS = -g
endif
LDFLAGS = -g

# Define the command to make dependencies.
ifneq	"$(findstring gcc,$(CC))" ""
# Don't include system directories.
mkdep-nolib = $(CC) -MM $(CPPFLAGS)
else
mkdep-nolib = $(mkdep)
endif
mkdep = $(CC) -M $(CPPFLAGS)

depfiles = $(patsubst %.o,%.dep,$(filter %.o,$(objs)))


.PHONY: default
default: $(prog)

$(prog): $(objs) $(globdep) #$(addprefix $(ARCH)/,gmalloc.o mcheck.o)
	$(CC) $(LDFLAGS) $^ $(globlib) $(LOADLIBES) -o $@.new
	mv -f $@.new $@

globfiles = $(addprefix glob/,COPYING.LIB Makefile.in \
			glob.c fnmatch.c glob.h fnmatch.h)
$(globfiles): stamp-glob ;
stamp-glob: /home/gd/gnu/libc/posix/glob.tar
	-rm -rf glob
	tar xvf $< glob
	touch $@
/home/gd/gnu/libc/posix/glob.tar: force
	$(MAKE) -C $(@D) $(@F) no_deps=t
.PHONY: force
force:

# Make the Unix-compatible Makefile to be distributed by appending
# the automatically-generated dependencies to compatMakefile.
ifeq ($(mkdep),$(mkdep-nolib))
nolib-deps = $(depfiles)
else
%.dep: %.c
	$(mkdep-nolib) $< | sed -e 's,$*\.o,$(@:.dep=.o) $@,' > $(@:.dep=.dtm)
	mv -f $(@:.dep=.dtm) $@
nolib-deps = $(patsubst $(archpfx)%,%,$(depfiles))
endif
# The distributed Makefile.in should contain deps for remote-stub only.
Makefile.in: compatMakefile $(nolib-deps:remote-%.dep=remote-stub.dep)
	(sed 's/^MAKE[	 ]*=.*$$/@SET_MAKE@/' $<; \
	 echo '# Automatically generated dependencies.'; \
	 sed -e 's/ [^ ]*\.dep//' -e 's=$(archpfx)==' $(filter-out $<,$^) \
	) > $@

.SUFFIXES: .dep
# Maintain the automatically-generated dependencies.
ifndef	   no_deps
include $(depfiles)
endif
$(archpfx)%.dep: %.c
	$(mkdep) $< | sed 's,$*\.o,$(@:.dep=.o) $@,' > $@

ETAGS = etags -T # for v19 etags

# Run the tests.
.PHONY: tests
testdir := $(shell ls -d1 make-test-?.? | sort -n +0.10 -0.11 +0.12 | tail -1l)
tests:# $(testdir)/run_make_tests.pl $(prog)
#	cd $(<D); MAKELEVEL=0 perl $(<F)

build.sh.in: build.template compatMakefile
	sed -e 's@%objs%@$(filter-out remote-% $(GLOB) $(ALLOCA) $(extras),\
	       $(patsubst $(archpfx)%,%,$(objs)))\
	       $(patsubst %.c,%.o,$(filter %.c,$(globfiles)))@' \
	    $< > $@.new
	mv -f $@.new $@

# Make the distribution tar files.

.PHONY: dist
# Figure out the version number from the source of `version.c'.
version := \
  $(strip $(shell sed -e '/=/!d' -e 's/^.*"\(.*\)";$$/\1/' < version.c))
tarfiles := make # make-doc
tarfiles := $(addsuffix -$(version).tar,$(tarfiles))
tarfiles := $(tarfiles:%=%.gz) # no more compress $(tarfiles:%=%.Z)
# Depend on default and doc so we don't ship anything that won't compile.
dist: rcs-mark default info dvi tests tarfiles
.PHONY: tarfiles
tarfiles: $(tarfiles)

ifndef dist-flavor
dist-flavor = alpha
endif
.PHONY: rcs-mark rcs-mark-alpha rcs-mark-beta
rcs-mark: rcs-mark-$(dist-flavor)
rcs-mark-alpha:;rcs -sAlpha -Nmake-$(version-): RCS/[!=]*,v
rcs-mark-beta:;rcs -sBeta -Nmake-$(version-): RCS/[!=]*,v
version- = $(subst .,-,$(version))

dist: local-inst
.PHONY: local-inst
local-inst: $(prog)
	install -c -g kmem -o $(USER) -m 2755 $< /usr/local/gnubin/make

# Put the alpha distribution files in the anonymous FTP directory.
alpha-files = $(tarfiles) GNUmakefile compatMakefile $(testdir).tar.Z
dist: alpha
.PHONY: alpha
alpha-dir := ~ftp/gnu
alpha-files := $(addprefix $(alpha-dir)/,$(alpha-files))
alpha: $(alpha-dir) $(alpha-files)
$(alpha-dir)/%: %
	@rm -f $@
	cp $< $@

# Implicit rule to make README and README-doc.
%: %.template version.c
	rm -f $@
	sed 's/VERSION/$(version)/' < $< > $@
# Make sure I don't edit it by accident.
	chmod a-w $@

define make-tar
@rm -fr make-$(version)
ln -s . make-$(version)
tar cvhof $@ $(addprefix make-$(version)/,$^)
rm -f make-$(version)
endef

%.Z: %; compress -c $< > $@
%.gz: %; gzip -9 -c -v $< > $@

make-doc-$(version).tar: README-doc COPYING make.dvi make.info make.info*
	$(make-tar)
make-$(version).tar: README INSTALL COPYING ChangeLog NEWS \
          configure Makefile.in configure.in build.sh.in mkinstalldirs \
	  acconfig.h $(srcs) remote-*.c $(globfiles) \
	  make.texinfo make-stds.texi \
	  make.?? make.??s make.toc make.aux make.man texinfo.tex TAGS tags \
	  install.sh \
	  make.info make.info*
	$(make-tar)

ifneq (,)
tests := $(filter-out %~,$(wildcard tests/*))
make-tests-$(version).tar.Z: $(tests)
	@rm -fr make-tests-$(version)
	ln -s tests make-tests-$(version)
	tar cvhf $(@:.Z=) $(patsubst tests/%,make-tests-$(version)/%,$^)
	rm -f make-tests-$(version)
	compress -f $(@:.Z=)
endif

$(archpfx)loadtest: $(archpfx)load.o
