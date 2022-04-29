if HAVE_GCOV

# Coverage targets
clean-gcda:
	@echo Removing old coverage results
	-find -name '*.gcda' -print | xargs -r rm
	-find -name '*.gcno' -print | xargs -r rm

coverage: clean-gcda
	-$(MAKE) $(AM_MAKEFLAGS) -k check
	$(MAKE) $(AM_MAKEFLAGS) generate-coverage-html

generate-coverage-html:
	@echo Collecting coverage data
	$(LCOV) --directory $(top_builddir) --capture --output-file coverage.info --no-checksum --compat-libtool
	LANG=C $(GENHTML) --prefix $(top_builddir) --output-directory coveragereport --title "Code Coverage" --legend --show-details coverage.info

clean-coverage-html: clean-gcda
	-$(LCOV) --directory $(top_builddir) -z
	-rm -rf coverage.info coveragereport

clean-local: clean-gcov
clean-gcov: clean-coverage-html
.PHONY: clean-gcda coverage generate-coverage-html clean-coverage-html

endif # HAVE_GCOV

ACLOCAL_AMFLAGS=-I m4
SUBDIRS = src tst

EXTRA_DIST = tap-driver.sh readme.md
analyze:
	make analyze -C src
	make analyze -C tst

.PHONY: analyze
