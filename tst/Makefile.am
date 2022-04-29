check_PROGRAMS = shelldown_test

shelldown_test_source = main.c config.c
shelldown_test_header = mtest.h

shelldown_test_SOURCES = $(shelldown_test_source) $(shelldown_test_header)
shelldown_test_CFLAGS = -I$(top_srcdir)/inc \
	-I$(top_srcdir)/src \
	-I$(top_srcdir) \
	$(COVERAGE_CFLAGS)

shelldown_test_LDFLAGS = $(COVERAGE_LDFLAGS) -static
shelldown_test_LDADD = $(top_builddir)/src/libshelldown.la

TESTS = $(check_PROGRAMS)
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/tap-driver.sh
CLEANFILES = shelldown.log
# static code analyzer

if ENABLE_ANALYZER

analyze_plists = $(shelldown_test_source:%.c=%.plist)
MOSTLYCLEANFILES = $(analyze_plists)

$(analyze_plists): %.plist: %.c
	clang --analyze -DHAVE_CONFIG_H -I../inc -I../src -I.. -I. $< -o $@

analyze: $(analyze_plists)

endif
