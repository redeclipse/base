* Wget-1.11.4 for Windows *
===========================
System
------
- Win32, i.e. MS-Windows 95 / 98 / ME / NT / 2000 / XP / 2003 / Vista / 2008 with msvcrt.dll
- if msvcrt.dll is not in your Windows/System folder, get it from
  Microsoft <http://support.microsoft.com/kb/259403>
  or by installing Internet Explorer 4.0 or higher
  <http://www.microsoft.com/windows/ie> 
- libintl-2 <http://gnuwin32.sourceforge.net/packages/libintl.htm> 
- libiconv-2 <http://gnuwin32.sourceforge.net/packages/libiconv.htm> 
- openssl <http://gnuwin32.sourceforge.net/packages/openssl.htm> 

Notes
-----
- Bugs and questions on this MS-Windows port: gnuwin32@users.sourceforge.net

Package and Source Availability
-------------------------------
- in: http://gnuwin32.sourceforge.net

Compilation
-----------
The package has been compiled with GNU auto-tools, GNU make, and Mingw
(GCC for MS-Windows). Any differences from the original sources are given
in wget-1.11.4-1-GnuWin32.diffs in wget-1.11.4-1-src.zip. Libraries needed
for compilation can be found at the lines starting with 'LIBS = ' in the
Makefiles. Usually, these are standard libraries provided with Mingw, or
libraries from the package itself; 'gw32c' refers to the libgw32c package,
which provides MS-Windows substitutes or stubs for functions normally found in
Unix. For more information, see: http://gnuwin32.sourceforge.net/compile.html
and http://gnuwin32.sourceforge.net/packages/libgw32c.htm.

GNU Wget
========
                  Current Web home: http://www.gnu.org/software/wget/

GNU Wget is a free utility for non-interactive download of files from
the Web.  It supports HTTP, HTTPS, and FTP protocols, as well as
retrieval through HTTP proxies.

It can follow links in HTML pages and create local versions of remote
web sites, fully recreating the directory structure of the original
site.  This is sometimes referred to as "recursive downloading."
While doing that, Wget respects the Robot Exclusion Standard
(/robots.txt).  Wget can be instructed to convert the links in
downloaded HTML files to the local files for offline viewing.

Recursive downloading also works with FTP, where Wget can retrieves a
hierarchy of directories and files.

With both HTTP and FTP, Wget can check whether a remote file has
changed on the server since the previous run, and only download the
newer files.

Wget has been designed for robustness over slow or unstable network
connections; if a download fails due to a network problem, it will
keep retrying until the whole file has been retrieved.  If the server
supports regetting, it will instruct the server to continue the
download from where it left off.

If you are behind a firewall that requires the use of a socks style
gateway, you can get the socks library and compile wget with support
for socks.

Most of the features are configurable, either through command-line
options, or via initialization file .wgetrc.  Wget allows you to
install a global startup file (/usr/local/etc/wgetrc by default) for
site settings.

Wget works under almost all Unix variants in use today and, unlike
many of its historical predecessors, is written entirely in C, thus
requiring no additional software, such as Perl.  The external software
it does work with, such as OpenSSL, is optional.  As Wget uses the GNU
Autoconf, it is easily built on and ported to new Unix-like systems.

As with other GNU software, the latest version of Wget can be found at
the master GNU archive site ftp.gnu.org, and its mirrors.  Wget
resides at <ftp://ftp.gnu.org/pub/gnu/wget/>.

Please report bugs in Wget to <bug-wget@gnu.org>.
Wget's home page is at <http://www.gnu.org/software/wget/>.

MAINTAINER: Micah Cowan <micah@cowan.name>

Wget was originally written and mainained by Hrvoje Niksic.  Please see
the file authors.txt for a list of major contributors, and the ChangeLogs
for a detailed listing of all contributions.


Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004
2005, 2006, 2007, 2008 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301
USA.

Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by linking or
combining it with the OpenSSL project's OpenSSL library (or a
modified version of that library), containing parts covered by the
terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
grants you additional permission to convey the resulting work.
Corresponding Source for a non-source form of such a combination
shall include the source code for the parts of OpenSSL used as well
as that of the covered work.

Authors of GNU Wget.

[ Note that this file does not attempt to list all the contributors to
  Wget; look at the ChangeLogs for that.  This is a list of people who
  contributed sizeable amounts of code and assigned the copyright to
  the FSF. ]

Hrvoje Niksic.  Designed and implemented Wget.

Gordon Matzigkeit.  Wrote netrc.c and netrc.h.

Darko Budor.  Wrote initial support for Windows, wrote wsstartup.c,
wsstartup.h and windecl.h.  (The files were later renamed, but his
code and ideas remained present.)

Junio Hamano.  Added support for FTP Opie and HTTP digest
authentication.

Dan Harkless.  Added --backup-converted, --follow-tags, --html-extension,
--ignore-tags, and --page-requisites; improved documentation; etc. Was
the principle maintainer of GNU Wget for some time.

Christian Fraenkel.  Initially implemented SSL support.

Thomas Lussnig.  Initially implemented IPv6 support.

Ian Abbott.  Contributed bugfixes, Windows-related fixes, provided a
prototype implementation of the new recursive code, and more.
Co-maintained Wget during the 1.8 release cycle.

Gisle Vanem.  Contributed Windows and MS-DOS improvements, including a
port of run_with_timeout to Windows, additions to Makefiles, and many
bug reports and fixes.

Mauro Tortonesi.  Improved IPv6 support, adding support for dual
family systems.  Refactored and enhanced FTP IPv6 code. Maintained GNU
Wget from 2004-2007.

Nicolas Schodet.  Contributed to cookie code and documentation.

Daniel Stenberg.  NTLM authentication in http-ntlm.c and http-ntlm.h
originally written for curl donated for use in GNU Wget.

Micah Cowan.  Current Wget maintainer, from mid-2007.
