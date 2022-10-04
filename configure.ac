## Process this file with autoconf to produce configure script

## Copyright (C) 2010 University of Helsinki
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


# autoconf requirements
AC_PREREQ([2.62])
AC_INIT([hfstospell],[0.5.3],[hfst-bugs@helsinki.fi],[hfstospell],[http://hfst.github.io])

LT_PREREQ([2.2.6])

# init
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([1.11 -Wall -Werror foreign check-news color-tests silent-rules])
AM_SILENT_RULES([yes])
AC_REVISION([$Revision$])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([ospell.cc])
AC_CONFIG_HEADERS([config.h])

# Information on package
HFSTOSPELL_NAME=hfstospell
HFSTOSPELL_MAJOR=0
HFSTOSPELL_MINOR=5
HFSTOSPELL_EXTENSION=.3
HFSTOSPELL_VERSION=$HFSTOSPELL_MAJOR.$HFSTOSPELL_MINOR$HFSTOSPELL_EXTENSION
AC_SUBST(HFSTOSPELL_MAJOR)
AC_SUBST(HFSTOSPELL_MINOR)
AC_SUBST(HFSTOSPELL_VERSION)
AC_SUBST(HFSTOSPELL_NAME)

PKG_PROG_PKG_CONFIG
AC_PATH_PROG([DOXYGEN], [doxygen], [false])
AM_CONDITIONAL([CAN_DOXYGEN], [test "x$DOXYGEN" != xfalse])

# Settings
AC_ARG_ENABLE([extra_demos],
              [AS_HELP_STRING([--enable-extra-demos],
                              [build conference demos for science reproduction @<:@default=no@:>@])],
              [enable_extra_demos=$enableval], [enable_extra_demos=no])
AM_CONDITIONAL([EXTRA_DEMOS], [test x$enable_extra_demos != xno])
AC_ARG_ENABLE([hfst_ospell_office],
              [AS_HELP_STRING([--enable-hfst-ospell-office],
                              [build hfst-ospell-office @<:@default=yes@:>@])],
              [enable_hfst_ospell_ofiice=$enableval], [enable_hfst_ospell_office=yes])
AM_CONDITIONAL([HFST_OSPELL_OFFICE], [test x$enable_hfst_ospell_office != xno])
AC_ARG_ENABLE([hfst_ospell_predict],
              [AS_HELP_STRING([--enable-hfst-ospell-predict],
                              [build hfst-ospell-predict @<:@default=yes@:>@])],
              [enable_hfst_ospell_ofiice=$enableval], [enable_hfst_ospell_predict=yes])
AM_CONDITIONAL([HFST_OSPELL_PREDICT], [test x$enable_hfst_ospell_predict != xno])
AC_ARG_ENABLE([zhfst],
              [AS_HELP_STRING([--enable-zhfst],
                              [support zipped complex automaton sets @<:@default=check@:>@])],
              [enable_zhfst=$enableval], [enable_zhfst=check])
AC_ARG_WITH([libxmlpp],
              [AS_HELP_STRING([--with-libxmlpp],
                              [support xml metadata for zipped automaton sets with libxml++-2.6 @<:@default=yes@:>@])],
              [with_libxmlpp=$withval], [with_libxmlpp=yes])
AC_ARG_WITH([tinyxml2],
              [AS_HELP_STRING([--with-tinyxml2],
                              [support xml metadata for zipped automaton sets with tinyxml2 @<:@default=no@:>@])],
              [with_tinyxml2=$withval], [with_tinyxml2=no])

AC_ARG_WITH([extract],
            [AS_HELP_STRING([--with-extract=TARGET],
                           [extract zhfst archives to tmpdir or mem @<:@default=mem@:>@])],
            [with_extract=$withval], [with_extract=mem])
AS_IF([test "x$with_extract" = xmem], [AC_DEFINE([ZHFST_EXTRACT_TO_MEM], [1],
                        [Define to extract zhfst archives to char buffer first])],
          [AS_IF([test "x$with_extract" = xtmpdir],
                 [AC_DEFINE([ZHFST_EXTRACT_TO_MEM], [0],
                            [Define to extract zhfst to tmp dir first])],
          [AC_MSG_ERROR([Use with-extract to mem or tmpdir])])])

# Checks for programs
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_CC
AC_PROG_CXX

LT_INIT
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Checks for libraries
AS_IF([test x$enable_zhfst != xno],
      [PKG_CHECK_MODULES([LIBARCHIVE], [libarchive > 3],
                  [AC_DEFINE([HAVE_LIBARCHIVE], [1], [Use archives])
                   enable_zhfst=yes],
                  [PKG_CHECK_MODULES([LIBARCHIVE], [libarchive > 2],
                              [AC_DEFINE([HAVE_LIBARCHIVE], [1], [Use archives])
	                       AC_DEFINE([USE_LIBARCHIVE_2], [1], [Use libarchive2])
                               enable_zhfst=yes],
                              [AS_IF([test x$enable_zhfst != xcheck],
                                     [AC_MSG_ERROR([zhfst support requires either libarchive or libarchive2])
                                      enable_zhfst=no],
                                     [enable_zhfst=no])])])])

AM_CONDITIONAL([WANT_ARCHIVE], [test x$enable_zhfst != xno])
AS_IF([test x$with_libxmlpp != xno],
      [PKG_CHECK_MODULES([LIBXMLPP], [libxml++-2.6 >= 2.10.0],
                         [AC_DEFINE([HAVE_LIBXML], [1], [Use libxml++])
                          enable_xml=libxmlpp],
                         [AC_MSG_ERROR([libxml++ failed])
                          enable_xml=no])])
AM_CONDITIONAL([WANT_LIBXMLPP], [test x$enable_xml = xlibxmlpp])
AS_IF([test x$with_tinyxml2 != xno -a x$with_libxmlpp = xno],
      [PKG_CHECK_MODULES([TINYXML2], [tinyxml2 >= 1.0.8 tinyxml2 < 3],
                         [AC_DEFINE([HAVE_TINYXML2], [1], [Use tinyxml])
                          enable_xml=tinyxml2],
                         [AC_MSG_ERROR([tinyxml missing])
                          enable_xml=no])])
AM_CONDITIONAL([WANT_TINYXML2], [test x$enable_xml = xtinyxml2])

# Find ICU in the new and old way
PKG_CHECK_MODULES(ICU, [icu-uc >= 4], [], [
 AC_PATH_PROG([ICU_CONFIG], [icu-config], [false])
 AS_IF([test x$ICU_CONFIG != xfalse], [
  ICU_LIBS=$($ICU_CONFIG --ldflags)
 ])
])
LIBS="$LIBS $ICU_LIBS"

# Checks for header files
AC_CHECK_HEADERS([getopt.h error.h])

# Checks for types
AC_TYPE_SIZE_T

# Checks for structures

# Checks for compiler characteristics
AC_C_BIGENDIAN

# Checks for library functions
AC_CHECK_FUNCS([strndup error])
# Checks for system services

# Require highest supported C++ standard
AC_LANG(C++)
AX_CHECK_COMPILE_FLAG([-std=c++20], [CXXFLAGS="$CXXFLAGS -std=c++20"], [
 AX_CHECK_COMPILE_FLAG([-std=c++2a], [CXXFLAGS="$CXXFLAGS -std=c++2a"], [
  AX_CHECK_COMPILE_FLAG([-std=c++17], [CXXFLAGS="$CXXFLAGS -std=c++17"], [
   AX_CHECK_COMPILE_FLAG([-std=c++1z], [CXXFLAGS="$CXXFLAGS -std=c++1z"], [
    AX_CHECK_COMPILE_FLAG([-std=c++14], [CXXFLAGS="$CXXFLAGS -std=c++14"], [
     AX_CHECK_COMPILE_FLAG([-std=c++1y], [CXXFLAGS="$CXXFLAGS -std=c++1y"], [
      AC_MSG_ERROR([Could not enable at least C++1y (C++14) - upgrade your compiler])
     ])
    ])
   ])
  ])
 ])
])

# config files
AC_CONFIG_FILES([Makefile hfstospell.pc])

# output
AC_OUTPUT

cat <<EOF
-- Building $PACKAGE_STRING
    * zhfst support: $enable_zhfst
    * extracting to: $with_extract
    * xml support: $enable_xml
    * hfst-ospell-office: $enable_hfst_ospell_office
    * conference demos: $enable_extra_demos
EOF
AS_IF([test x$with_libxmlpp != xno -a x$with_tinyxml2 != xno],
      [AC_MSG_ERROR([You can only have one xml library (e.g., --with-tinyxml2 --without-libxmlpp)])])
