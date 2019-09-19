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
# CONFIG[:default_graphics] = 'win32'

# Choose a compiler; if no entry, let the Rakefile try to detect one
# CONFIG[:compiler] = :gcc      # Default on Linux; fallback on Windows
# CONFIG[:compiler] = :clang    # Default on Mac; fallback on Linux
# CONFIG[:compiler] = :visualc  # Default on Windows
# CONFIG[:compiler] = :watcom   # Available on Windows
# CONFIG[:compiler] = :cc       # Last resort

# Override the names of the compilers, e.g., to use "clang-6.0" instead
# of "clang"
# CONFIG[:CC] = "clang"
# CONFIG[:CXX] = "clang++"
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
CONFIG[:PDCurses] = "#{ENV['USERPROFILE']}/PDCurses"

# This is the root of the SDL2 tree
CONFIG[:SDL2] = "#{ENV['USERPROFILE']}/SDL2"

# This is the root of the Qt tree
CONFIG[:Qt] = "C:/qt/Qt5.13.0/5.13.0/mingw73_32"

# To get the Curses interface in NetHackW, enable this and use the Bill Gray
# fork of PDCurses; see
# https://www.projectpluto.com/win32a.htm
# https://github.com/Bill-Gray/PDCurses/
#CONFIG[:win_curses] = true
