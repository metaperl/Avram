include_directories(${AVRAM_SOURCE_DIR}/avm)
link_directories(${AVRAM_SOURCE_DIR}/src)

set(SRCS
   apply.c error.c gsldiflib.c listfuns.c mwrap.c umf.c exf.c gslevu.c
   lists.c portals.c vglue.c bes.c exmodes.c gslintlib.c lpsolve.c
   ports.c vman.c branches.c farms.c harminv.c matcon.c profile.c
   xstrerror.c chrcodes.c fftw.c instruct.c mathlib.c rawio.c avram.c
   cmdline.c fnames.c jobs.c memmove.c remote.c compare.c formin.c
   kinsol.c minpack.c rewrite.c complexlib.c formout.c lapack.c mpfr.c
   rmathlib.c decons.c glpklib.c libfuns.c mtwist.c servlist.c)

add_executable(avram ${SRCS})
