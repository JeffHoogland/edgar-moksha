ACLOCAL_AMFLAGS = -I m4
MAINTAINERCLEANFILES = Makefile.in aclocal.m4 config.guess config.h.in \
                       config.sub configure depcomp install-sh ltmain.sh \
                       missing module.desktop config.rpath mkinstalldirs

SUBDIRS = src python

EDJE_FLAGS = -v \
             -id $(top_srcdir)

filesdir =  $(datadir)
files_DATA = module.desktop e-module-edgar.edj

EXTRA_DIST = module.desktop.in \
             module_icon.png \
             e-module-edgar.edc

%.edj:  %.edc
	$(EDJE_CC) $(EDJE_FLAGS) $< $@

clean-local:
	rm -rf e-module-edgar.edj module.desktop *~

uninstall:
	rm -rf $(DESTDIR)$(datadir)
