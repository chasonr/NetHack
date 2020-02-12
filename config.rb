# Configuration for NetHack Rakefile

##############################################################################
#           This section applies to all systems supported by rake            #
##############################################################################

# Enable or disable lines as desired
CONFIG[:TTY_graphics   ] = true
# CONFIG[:Curses_graphics] = true
# CONFIG[:Qt_graphics    ] = true
# CONFIG[:SDL2_graphics  ] = true
# CONFIG[:X11_graphics   ] = true

# Define a default window system
# CONFIG[:default_graphics] = 'tty'
# CONFIG[:default_graphics] = 'curses'
# CONFIG[:default_graphics] = 'Qt'
# CONFIG[:default_graphics] = 'SDL2'
# CONFIG[:default_graphics] = 'X11'
# CONFIG[:default_graphics] = 'mswin'

# Optional features having additional dependencies
# CONFIG[:png_tiles] = true # PNG support on SDL2, X11 and Win32
# CONFIG[:zlib_compress] = true # ZLIB-based file compression

# Define the installation directory. If this is a relative path, it is relative
# to your home directory: %USERPROFILE% on Windows, or $HOME on Linux and Mac.
CONFIG[:install_dir] = 'games/nethackdir'

# Choose a compiler; if no entry, let the Rakefile try to detect one
# CONFIG[:compiler] = :gcc      # Default on Linux; fallback on Windows
# CONFIG[:compiler] = :clang    # Default on Mac; fallback on Linux
# CONFIG[:compiler] = :visualc  # Default on Windows
# CONFIG[:compiler] = :cc       # Last resort

# Override the names of the compilers, e.g., to use "clang-6.0" instead
# of "clang"
# CONFIG[:CC] = "clang"
# CONFIG[:CXX] = "clang++"
# CONFIG[:rc] = "rc"
# CONFIG[:link] = "clang++"
# CONFIG[:moc] = "moc"

# Enable to include debug tables
# CONFIG[:debug] = true

# Provide extra compiler flags
# CONFIG[:CFLAGS] = ""
# CONFIG[:CXXFLAGS] = ""

# If you have the lex and yacc programs, enable them here
# CONFIG[:lex] = "lex"
# CONFIG[:yacc] = "yacc"
# CONFIG[:lex] = "flex"
# CONFIG[:yacc] = "bison -y"

##############################################################################
#                 This section applies to Microsoft Windows                  #
##############################################################################

# This is the root of the PDCurses tree
# This should be the Bill Gray fork of PDCurses; see
# https://www.projectpluto.com/win32a.htm
# https://github.com/Bill-Gray/PDCurses/
CONFIG[:PDCurses] = "#{ENV['USERPROFILE']}/PDCurses-4.1.0"

# This is the root of the SDL2 tree
#CONFIG[:SDL2] = "#{ENV['USERPROFILE']}/SDL2-2.0.10/i686-w64-mingw32"
CONFIG[:SDL2] = "#{ENV['USERPROFILE']}/SDL2-2.0.10-MSC"
# For Visual Studio: we need the directory where the libraries are found
CONFIG[:architecture] = 'x86'
# CONFIG[:architecture] = 'x64'

# This is the root of the Qt tree
#CONFIG[:Qt] = "C:/qt/5.14.1/mingw73_32"
CONFIG[:Qt] = "C:/qt/5.14.1/msvc2017"

# This is the root of the libpng tree
CONFIG[:libpng] = "#{ENV['USERPROFILE']}/#{CONFIG[:architecture]}libs/libpng-1.6.37"

# This is the root of the zlib tree
CONFIG[:zlib] = "#{ENV['USERPROFILE']}/#{CONFIG[:architecture]}libs/zlib-1.2.11"
