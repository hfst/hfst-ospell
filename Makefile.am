## Process this file with automake to produce Makefile.in

# Copyright 2010 University of Helsinki
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

# to silence:
# libtoolize: Consider adding `-I m4' to ACLOCAL_AMFLAGS in Makefile.am.
ACLOCAL_AMFLAGS=-I m4

# targets
if EXTRA_DEMOS
CONFERENCE_DEMOS=hfst-ospell-norvig hfst-ospell-fsmnlp-2012 hfst-ospell-cicling\
				 hfst-ospell-survey hfst-ospell-lrec2013 hfst-ispell
endif # EXTRA_DEMOS

if HFST_OSPELL_OFFICE
MAYBE_HFST_OSPELL_OFFICE=hfst-ospell-office
endif # HFST_OSPELL_OFFICE

bin_PROGRAMS=hfst-ospell $(MAYBE_HFST_OSPELL_OFFICE) $(CONFERENCE_DEMOS)
lib_LTLIBRARIES=libhfstospell.la
man1_MANS=hfst-ospell.1 hfst-ospell-office.1

PKG_LIBS=
PKG_CXXFLAGS=

if WANT_ARCHIVE
PKG_LIBS+=$(LIBARCHIVE_LIBS)
PKG_CXXFLAGS+=$(LIBARCHIVE_CFLAGS)
endif

if WANT_PUGIXML
PKG_LIBS+=$(PUGIXML_LIBS)
PKG_CXXFLAGS+=$(PUGIXML_CFLAGS)
endif

# library parts
libhfstospell_la_SOURCES=hfst-ol.cc ospell.cc \
						 ZHfstOspeller.cc ZHfstOspellerXmlMetadata.cc
libhfstospell_la_CXXFLAGS=$(AM_CXXFLAGS) $(CXXFLAGS) $(PKG_CXXFLAGS)
libhfstospell_la_LDFLAGS=-no-undefined -version-info 11:0:0 \
						 $(PKG_LIBS)

# link sample program against library here
hfst_ospell_SOURCES=main.cc
hfst_ospell_LDADD=libhfstospell.la
hfst_ospell_CXXFLAGS=$(AM_CXXFLAGS) $(CXXFLAGS) \
					 $(PKG_CXXFLAGS)

if HFST_OSPELL_OFFICE

hfst_ospell_office_SOURCES=office.cc
hfst_ospell_office_LDADD=libhfstospell.la
hfst_ospell_office_CXXFLAGS=$(AM_CXXFLAGS) $(CXXFLAGS) $(PKG_CXXFLAGS)

endif # HFST_OSPELL_OFFICE

if EXTRA_DEMOS

hfst_ospell_norvig_SOURCES=main-norvig.cc
hfst_ospell_norvig_LDADD=libhfstospell.la
hfst_ospell_norvig_CXXFLAGS=$(AM_CXXFLAGS) $(CXXFLAGS) \
							$(PKG_CXXFLAGS)

hfst_ospell_cicling_SOURCES=main-cicling.cc
hfst_ospell_cicling_LDADD=libhfstospell.la
hfst_ospell_cicling_CXXFLAGS=$(AM_CXXFLAGS) $(CXXFLAGS) \
							 $(PKG_CXXFLAGS)

hfst_ospell_lrec2013_SOURCES=main-lrec2013.cc
hfst_ospell_lrec2013_LDADD=libhfstospell.la
hfst_ospell_lrec2013_CXXFLAGS=$(AM_CXXFLAGS) $(CXXFLAGS) \
							$(PKG_CXXFLAGS)

hfst_ospell_survey_SOURCES=main-survey.cc
hfst_ospell_survey_LDADD=libhfstospell.la
hfst_ospell_survey_CXXFLAGS=$(AM_CXXFLAGS) $(CXXFLAGS) \
							$(PKG_CXXFLAGS)

hfst_ospell_fsmnlp_2012_SOURCES=main-fsmnlp-2012.cc
hfst_ospell_fsmnlp_2012_LDADD=libhfstospell.la
hfst_ospell_fsmnlp_2012_CXXFLAGS=$(AM_CXXFLAGS) $(CXXFLAGS) \
								 $(PKG_CXXFLAGS)
endif # EXTRA_DEMOS

if EXTRA_DEMOS

hfst_ispell_SOURCES=main-ispell.cc
hfst_ispell_LDADD=libhfstospell.la
hfst_ispell_CXXFLAGS=$(AM_CXXFLAGS) $(CXXFLAGS) \
							$(PKG_CXXFLAGS)

endif # EXTRA_DEMOS

# install headers for library in hfst's includedir
include_HEADERS=hfst-ol.h ospell.h ol-exceptions.h \
				ZHfstOspeller.h ZHfstOspellerXmlMetadata.h \
				hfstol-stdafx.h

# pkgconfig
pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=hfstospell.pc

# tests
TESTS=tests/basic-zhfst.sh tests/basic-edit1.sh \
	  tests/empty-descriptions.sh tests/empty-titles.sh tests/empty-locale.sh \
	  tests/trailing-spaces.sh tests/bad-errormodel.sh tests/empty-zhfst.sh \
	  tests/analyse-spell.sh tests/no-errormodel.sh
if WANT_ARCHIVE
XFAIL_TESTS=tests/empty-descriptions.sh tests/empty-titles.sh tests/empty-locale.sh tests/empty-zhfst.sh
else
XFAIL_TESTS=tests/empty-descriptions.sh tests/empty-titles.sh tests/empty-locale.sh tests/empty-zhfst.sh \
	tests/basic-zhfst.sh tests/basic-edit1.sh tests/trailing-spaces.sh tests/bad-errormodel.sh \
	tests/analyse-spell.sh tests/no-errormodel.sh
endif

if CAN_DOXYGEN
doxygen:
	$(DOXYGEN)
endif

EXTRA_DIST=hfst-ospell.1 hfst-ospell-office.1 tests/basic-zhfst.sh tests/basic-edit1.sh \
	  tests/empty-descriptions.sh tests/empty-titles.sh tests/empty-locale.sh \
	  tests/trailing-spaces.sh tests/bad-errormodel.sh tests/empty-zhfst.sh \
	  tests/analyse-spell.sh tests/no-errormodel.sh \
	  tests/empty-descriptions.sh tests/empty-titles.sh tests/empty-locale.sh tests/empty-zhfst.sh \
	  tests/acceptor.basic.txt tests/analyser.default.txt tests/errmodel.basic.txt tests/errmodel.edit1.txt tests/errmodel.extrachars.txt \
	  tests/test.strings \
	  tests/bad_errormodel.zhfst tests/empty_descriptions.zhfst tests/empty_locale.zhfst tests/empty_titles.zhfst tests/no_errormodel.zhfst \
	  tests/speller_analyser.zhfst tests/speller_basic.zhfst tests/speller_edit1.zhfst tests/trailing_spaces.zhfst \
	  tests/basic_test.xml tests/empty_descriptions.xml tests/empty_locale.xml tests/empty_titles.xml tests/no_errmodel.xml tests/trailing_spaces.xml
