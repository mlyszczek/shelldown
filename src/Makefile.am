shelldown_source = config.c id-map.c main.c
shelldown_headers = config.h macros.h id-map.h

bin_cflags = $(COVERAGE_CFLAGS) -I$(top_srcdir) -I$(top_srcdir)/inc
bin_ldflags = $(COVERAGE_LDFLAGS)

if ENABLE_STANDALONE

bin_PROGRAMS = shelldown
standalone_cflags = -DSHELLDOWN_STANDALONE=1

shelldown_SOURCES = $(shelldown_source) $(shelldown_headers)
shelldown_LDFLAGS = $(bin_ldflags)
shelldown_CFLAGS = $(bin_cflags) $(standalone_cflags)

endif # ENABLE_STANDALONE

if ENABLE_LIBRARY

lib_LTLIBRARIES = libshelldown.la
library_cflags = -DSHELLDOWN_LIBRARY=1

libshelldown_la_SOURCES = $(shelldown_source)
libshelldown_la_CFLAGS = $(bin_cflags) $(library_cflags)
libshelldown_la_LDFLAGS = $(bin_ldflags) -version-info 1:0:1

endif # ENABLE_LIBRARY
# static code analyzer

if ENABLE_ANALYZER

analyze_plists = $(shelldown_source:%.c=%.plist)
MOSTLYCLEANFILES = $(analyze_plists)

$(analyze_plists): %.plist: %.c
	clang --analyze -DHAVE_CONFIG_H -I../inc -I. -I.. $< -o -cconfig.file.shelldown

analyze: $(analyze_plists)

endif # ENABLE_ANALYZER
