MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I. -Wall \
           -I$(top_srcdir) \
           -I$(includedir) \
           @EO_CFLAGS@ @E_CFLAGS@ @PYTHON_CPPFLAGS@

pkgdir = $(datadir)/$(MODULE_ARCH)
pkg_LTLIBRARIES = module.la
module_la_SOURCES = e_mod_main.h \
                    e_mod_main.c \
                    e_mod_edgar.c \
                    e_mod_edgar.h

module_la_CPPFLAGS = -DEFL_BETA_API_SUPPORT -DEFL_EO_API_SUPPORT
module_la_LIBADD = @EO_LIBS@ @E_LIBS@ @PYTHON_LDFLAGS@ \
                   @PYTHON_EXTRA_LIBS@ @PYTHON_EXTRA_LDFLAGS@
module_la_LDFLAGS = -module -avoid-version
module_la_DEPENDENCIES = $(top_builddir)/config.h

clean-local:
	rm -rf *~
