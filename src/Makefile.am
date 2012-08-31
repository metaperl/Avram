EXTRA_DIST              = com.fun exfsrc.fun

bin_PROGRAMS            = avram

avram_CFLAGS            = $(AM_CFLAGS)

avram_LDADD             = -lm

avram_LDFLAGS           = -export-dynamic

MAINTAINERCLEANFILES	= Makefile.in

INCLUDES		= -I$(top_builddir) -I$(top_srcdir) $(LTDLINCL)

avram_SOURCES     = avram.c error.c xstrerror.c \
lists.c branches.c chrcodes.c rawio.c formin.c formout.c fnames.c \
cmdline.c compare.c decons.c ports.c portals.c profile.c instruct.c \
listfuns.c matcon.c apply.c exmodes.c vman.c memmove.c rewrite.c exf.c \
mathlib.c complexlib.c rmathlib.c mtwist.c \
gslintlib.c gsldiflib.c gslevu.c glpklib.c umf.c mpfr.c \
lapack.c fftw.c minpack.c kinsol.c libfuns.c mwrap.c bes.c lpsolve.c \
harminv.c remote.c servlist.c jobs.c farms.c vglue.c

