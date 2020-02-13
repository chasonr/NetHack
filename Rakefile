# Rakefile for NetHack, supporting Windows, Mac OS and Linux

require 'set'

# User-configurable settings go here; the user should edit config.rb only:
CONFIG = {}
require './config.rb'

# Detect the running operating system
if Rake::Win32.windows? then
    PLATFORM = :windows
elsif RUBY_PLATFORM == 'i386-msys' then
    PLATFORM = :windows
elsif RUBY_PLATFORM =~ /^[^-]*-darwin[0-9.]+/ then
    PLATFORM = :mac
else
    PLATFORM = :unix # Assumed
end

# Look for a compiler if one is not specified
if not CONFIG[:compiler] then
    case PLATFORM
    when :windows then
        compilers = [ :visualc, :gcc, :clang ]
    when :mac then
        compilers = [ :clang, :gcc ]
    when :unix then
        compilers = [ :gcc, :clang ]
    else
        raise RuntimeError, %Q[Unknown platform "#{PLATFORM}"]
    end
    compilers.each do |c|
        case c
        when :gcc then
            cmd = 'gcc --version'
        when :clang then
            cmd = 'clang --version'
        when :visualc then
            cmd = 'cl -nologo'
        end
        begin
            banner = `#{cmd}`
            CONFIG[:compiler] = c
            break
        rescue Errno::ENOENT
            ;
        end
    end
end
CONFIG[:compiler] ||= :cc   # Last resort

# Set the compiler names
case CONFIG[:compiler]
when :gcc then
	CONFIG[:CC] ||= 'gcc'
	CONFIG[:CXX] ||= 'g++'
    CONFIG[:rc] ||= 'windres'
when :clang then
    CONFIG[:CC] ||= 'clang'
    CONFIG[:CXX] ||= 'clang++'
    CONFIG[:rc] ||= 'windres'
when :visualc then
    CONFIG[:CC] ||= 'cl'
    CONFIG[:CXX] ||= 'cl'
    CONFIG[:rc] ||= 'rc'
when :cc then
    CONFIG[:CC] ||= 'cc'
    CONFIG[:CXX] ||= 'c++'
    CONFIG[:rc] ||= 'windres'
else
    raise RuntimeError, %Q[Unknown compiler "#{CONFIG[:compiler]}"]
end

# Certain combinations of platform and interface are unsupported
if PLATFORM == :windows then
    CONFIG[:Curses_graphics] ||= CONFIG[:TTY_graphics]
    CONFIG[:TTY_graphics] = false
    CONFIG[:X11_graphics] = false
end

# If Qt, look for the MOC
if CONFIG[:Qt_graphics] then
    case PLATFORM
    when :windows then
        QTDIR = CONFIG[:Qt]
    when :mac then
        QTDIR = File.join(`brew --prefix`.chomp, 'opt/qt')
    when :unix then
        QTDIR = nil
    end
    if not CONFIG[:moc] then
        mocs = %w[moc-qt5 moc-qt4 moc]
        if QTDIR then
            mocs += mocs.map {|x| File.join(QTDIR, 'bin', x)}
        end
        mocs.each do |moc|
            begin
                text = `#{moc} -v 2>&1`
                if text =~ /moc 5\.[0-9]+\.[0-9]+/ then
                    CONFIG[:moc] = moc
                    QT5 = true
                    break
                elsif text =~ /Qt Meta Object Compiler version [0-9]+ \(Qt 4\.[0-9]+\.[0-9]+\)/ then
                    CONFIG[:moc] = moc
                    QT5 = false
                    break
                end
            rescue Errno::ENOENT
                ;
            end
        end
    end
    if not CONFIG[:moc] then
        CONFIG[:Qt_graphics] = false
        STDERR.puts 'Warning: moc not found; disabling Qt'
    end
end

# Define a default window system if none is specified
if not CONFIG[:default_graphics] then
    if PLATFORM == :windows then
        CONFIG[:default_graphics] = 'mswin'
    elsif CONFIG[:SDL2_graphics] then
        CONFIG[:default_graphics] = 'sdl2'
    elsif CONFIG[:Qt_graphics] then
        CONFIG[:default_graphics] = 'Qt'
    elsif CONFIG[:X11_graphics] then
        CONFIG[:default_graphics] = 'X11'
    elsif CONFIG[:Curses_graphics] then
        CONFIG[:default_graphics] = 'curses'
    else
        CONFIG[:default_graphics] = 'tty'
    end
end

# File extensions and path separators
def slash(name)
    name
end
if PLATFORM == :windows then
    def exe(name)
        name + '.exe'
    end
else
    def exe(name)
        name
    end
end
if CONFIG[:compiler] == :visualc then
    if PLATFORM == :windows then
        def slash(name)
            name.gsub('/', "\\");
        end
    end
    def obj(name)
        name + '.obj'
    end
    def res(name)
        name + '.res'
    end
else
    def obj(name)
        name + '.o'
    end
    def res(name)
        name + '.o'
    end
end

# Install path
install_path = CONFIG[:install_dir]
case PLATFORM
when :windows then
    # Path is absolute if it has a drive letter, or if it begins with a slash.
    if install_path =~ %r[^[[:alpha]]:] then
        # Drive letter is present; consider as relative to the root of the
        # drive
        install_path = $& + "\\" + $'
    elsif install_path.start_with?("\\", "/") then
        # No drive letter, but slash is present; install to drive C
        install_path = "C:" + install_path
    else
        # Consider as relative to the home directory
        install_path = File.join(ENV['USERPROFILE'], install_path)
    end
    install_path.gsub!("\\", "/")
when :unix, :mac then
    # Path is absolute if it begins with a slash
    if not install_path.start_with?("/") then
        install_path = File.join(ENV['HOME'], install_path)
    end
end
HACKDIR = install_path.gsub(%r[///+], '/')

# Compiler flags
cflags = ""
cxxflags = ""
if CONFIG[:debug] then
    case CONFIG[:compiler]
    when :gcc, :clang then
        min_flags = "-Wall -g -fPIC"
    when :cc then
        min_flags = "-g"
    when :visualc then
        min_flags = "-Wall"
    end
else
    case CONFIG[:compiler]
    when :gcc, :clang then
        min_flags = "-Wall -O2 -fPIC"
    when :cc then
        min_flags = "-O1"
    when :visualc then
        min_flags = "-Wall -Ox"
    end
end
if CONFIG[:compiler] == :visualc then
    # TODO: 4131, old-style function
    # TODO: 4242 and 4244, loss of data when converting numeric types
    # TODO: 4255, no function prototype given
    # TODO: 4706, assignment within conditional expression
    min_flags += " -nologo -wd4820 -wd4711 -wd4131 -wd4710 -wd4242 -wd4244 -wd5045 -wd4255 -wd4668 -wd4514 -wd4706 -wd4996"
    cxxflags += " -EHsc -wd4623 -wd4625 -wd4626 -wd5026 -wd5027"
end
base_flags = min_flags
base_flags += ' -DDLB -DTIMED_DELAY'
base_flags += " -Iinclude"
base_flags += " -DNOTTYGRAPHICS" unless CONFIG[:TTY_graphics]
base_flags += " -DCURSES_GRAPHICS" if CONFIG[:Curses_graphics]
base_flags += " -DQT_GRAPHICS" if CONFIG[:Qt_graphics]
base_flags += " -DSDL2_GRAPHICS" if CONFIG[:SDL2_graphics]
base_flags += " -DX11_GRAPHICS -DUSE_XPM= -DHAVE_XPM" if CONFIG[:X11_graphics]
base_flags += %Q[ -DDEFAULT_WINDOW_SYS=\\"#{CONFIG[:default_graphics]}\\"]
base_flags += " -DSYSCF= -DDUMPLOG= -DSTATUES_LOOK_LIKE_MONSTERS= -DPOSITIONBAR"
if PLATFORM == :windows then
    base_flags += " -DMSWIN_GRAPHICS -DSAFEPROCS"
end
if CONFIG[:zlib_compress] then
    base_flags += %Q[ -DZLIB_COMP -DCOMPRESS_EXTENSION=\\".gz\\"]
end
if CONFIG[:png_tiles] then
    base_flags += %Q[ -DHAVE_PNG]
end
case PLATFORM
when :windows then
    if CONFIG[:png_tiles] then
        base_flags += %Q[ -I#{CONFIG[:libpng]}]
    end
    if CONFIG[:png_tiles] or CONFIG[:zlib_compress] then
        base_flags += %Q[ -I#{CONFIG[:zlib]}]
    end
when :unix, :mac then
    if CONFIG[:png_tiles] then
        base_flags += " " + `pkg-config --cflags libpng`.chomp
    end
    if CONFIG[:zlib_compress] then
        base_flags += " " + `pkg-config --cflags zlib`.chomp
    else
        base_flags += %q[ -DCOMPRESS=\\"/bin/gzip\\"]
        base_flags += %q[ -DCOMPRESS_EXTENSION=\\".gz\\"]
    end
    base_flags += %Q[ -DSYSCF_FILE=\\"#{HACKDIR}/sysconf\\"]
    base_flags += %q[ -DSECURE]
    base_flags += %Q[ -DHACKDIR=\\"#{HACKDIR}\\"]
    base_flags += %q[ -DCONFIG_ERROR_SECURE=FALSE]
end

cflags += " " + (CONFIG[:CFLAGS] || "")
cxxflags += " " + (CONFIG[:CXXFLAGS] || "")
case CONFIG[:compiler]
when :gcc, :clang then
    cxxflags += " -std=gnu++11"
end
CFLAGS = (base_flags + " " + cflags).strip
CXXFLAGS = (base_flags + " " + cxxflags).strip
case CONFIG[:compiler]
when :visualc then
    def objflag(name)
        "-Fo" + name
    end
    def exeflag(name)
        "-Fe" + name
    end
else
    def objflag(name)
        "-o " + name
    end
    def exeflag(name)
        "-o " + name
    end
end

# The final build products
targets = Set.new([
    exe('binary/nethack'),
    'binary/nhdat',
    'binary/license',
    'binary/logfile',
    'binary/record',
    'binary/save',
    'binary/xlogfile'
])
if PLATFORM == :unix or PLATFORM == :mac then
    targets.merge(%w[
        binary/perm
        binary/recover
        binary/symbols
        binary/sysconf
    ])
else
    targets.merge(%w[
        binary/symbols.template
        binary/sysconf.template
        binary/.nethackrc.template
        binary/Guidebook.txt
        binary/opthelp
    ])
end
if CONFIG[:X11_graphics] then
    targets.merge(%w[
        binary/NetHack.ad
        binary/pet_mark.xbm
        binary/pilemark.xbm
        binary/rip.xpm
        binary/x11tiles
    ])
end
if CONFIG[:Qt_graphics] then
    targets.merge(%w[
        binary/nhsplash.xpm
        binary/nhtiles.bmp
        binary/pet_mark.xbm
        binary/pilemark.xbm
        binary/rip.xpm
    ])
    if PLATFORM == :windows then
        if QT5 then
            targets.merge(%w[
                binary/Qt5Gui.dll
                binary/Qt5Widgets.dll
                binary/Qt5Multimedia.dll
                binary/Qt5Core.dll
            ])
        else
            targets.merge(%w[
                binary/QtGui4.dll
                binary/QtCore4.dll
            ])
        end
    end
end
if CONFIG[:SDL2_graphics] then
    targets << 'binary/nhtiles.bmp'
    if PLATFORM == :windows and CONFIG[:compiler] == :visualc then
        targets << "binary/SDL2.dll"
    end
end
if PLATFORM == :windows then
    targets << 'binary/nhtiles.bmp'
end

task :default => targets.to_a do
end

# Some useful functions
def make_dir(dir)
    mkdir_p(dir) unless File.directory?(dir)
end

def in_dir(dir)
    old_dir = Dir.getwd
    begin
        Dir.chdir(dir)
        yield
    ensure
        Dir.chdir(old_dir)
    end
end

# Build rules:
def compile_rule(src, flags)
    # TODO set up autodepends
    target = obj("build/#{src}")
    deps = [src]
    if File.file?(src) then
        text = File.read(src)
        if /^\s*#\s*include\s*"hack\.h"/ =~ text then
            deps << 'include/pm.h'
            deps << 'include/onames.h'
        end
        if /^\s*#\s*include\s*"date\.h"/ =~ text then
            deps << 'include/date.h'
        end
    end
    if src.end_with?('.cpp') then
        file target => deps do |x|
            make_dir File.dirname(x.name)
            sh slash("#{CONFIG[:CXX]} #{CXXFLAGS} #{flags.chomp} -c #{src} #{objflag(target)}")
        end
    else
        file target => deps do |x|
            make_dir File.dirname(x.name)
            sh slash("#{CONFIG[:CC]} #{CFLAGS} #{flags.chomp} -c #{src} #{objflag(target)}")
        end
    end
end

def link_rule(ofiles, exe, libs)
    cpp = false
    cpp_ext = obj('.cpp')
    ofiles.each do |o|
        if o.end_with?(cpp_ext) then
            cpp = true
            break
        end
    end
    if cpp then
        file exe => ofiles do |x|
            make_dir File.dirname(x.name)
            sh slash("#{CONFIG[:link] || CONFIG[:CXX]} #{ofiles.join(' ')} #{exeflag(exe)} #{libs.join(' ')}")
        end
    else
        file exe => ofiles do |x|
            make_dir File.dirname(x.name)
            sh slash("#{CONFIG[:link] || CONFIG[:CC]} #{ofiles.join(' ')} #{exeflag(exe)} #{libs.join(' ')}")
        end
    end
end

# Directory src
src_dir = %w[
allmain  alloc    apply    artifact attrib   ball     bones    botl
cmd      dbridge  decl     detect   dig      display  dlb      do
dog      dogmove  dokick   do_name  dothrow  do_wear  drawing  dungeon
eat      end      engrave  exper    explode  extralev files    fountain
hack     hacklib  invent   isaac64  light    lock     mail     makemon
mapglyph mcastu   mhitm    mhitu    minion   mklev    mkmap    mkmaze
mkobj    mkroom   mon      mondata  monmove  monst    mplayer  mthrowu
muse     music    objects  objnam   o_init   options  pager    pickup
pline    polyself potion   pray     priest   quest    questpgr read
rect     region   restore  rip      rnd      role     rumors   save
shk      shknam   sit      sounds   spell    sp_lev   steal    steed
sys      teleport timeout  topten   track    trap     uhitm    u_init
vault    version  vision   weapon   were     wield    windows  wizard
worm     worn     write    zap
].map {|x| "src/#{x}.c"}
src_dir.each do |src|
    compile_rule(src, '')
end

# Directory util
util_dir = %w[
    dgn_main dlb_main lev_main makedefs panic recover
].map {|x| "util/#{x}.c"}
util_dir.each do |src|
    compile_rule(src, '')
end

# Directory sys/share
sysshare_dir = %w[
dgn_lex.c  dgn_yacc.c ioctl.c    lev_lex.c  lev_yacc.c nhlan.c    pcmain.c
pcsys.c    pctty.c    pcunix.c   random.c   tclib.c    unixtty.c  uudecode.c
pmatchregex.c posixregex.c cppregex.cpp
].map {|x| "sys/share/#{x}"}
sysshare_dir.each do |src|
    compile_rule(src, '')
end

# Directory sys/unix
sysunix_dir = %w[
    unixmain unixres unixunix
].map {|x| "sys/unix/#{x}.c"}
sysunix_dir.each do |src|
    compile_rule(src, '')
end

# Directory win/share
winshare_dir = %w[
    tilemap tiletext
    tileset bmptiles giftiles pngtiles xpmtiles
    safeproc
].map {|x| "win/share/#{x}.c"}
winshare_dir.each do |src|
    compile_rule(src, '')
end

# Directory win/tty
if CONFIG[:TTY_graphics] then
    wintty_dir = %w[
        getline termcap topl wintty
    ].map {|x| "win/tty/#{x}.c"}
    case PLATFORM
    when :windows then
        tty_flags = ''
    when :mac then
        tty_flags = "-DNOTPARMDECL"
    when :unix then
        tty_flags = `pkg-config --cflags ncursesw`.chomp + " -DNOTPARMDECL"
    end
    wintty_dir.each do |src|
        compile_rule(src, tty_flags)
    end
end

# Directory win/curses
if CONFIG[:Curses_graphics] then
    wincurses_dir = %w[
        cursdial cursinit cursinvt cursmain cursmesg cursmisc cursstat curswins
    ].map {|x| "win/curses/#{x}.c"}
    case PLATFORM
    when :windows then
        curses_flags = "-I#{CONFIG[:PDCurses]} -DPDC_WIDE"
    when :mac then
        curses_flags = ''
    when :unix then
        curses_flags = `pkg-config --cflags ncursesw`
    end
    wincurses_dir.each do |src|
        compile_rule(src, curses_flags)
    end
end

# Directory win/sdl2
if CONFIG[:SDL2_graphics] then
    winsdl2_dir = %w[
        sdl2getlin sdl2map sdl2menu sdl2message sdl2plsel sdl2posbar sdl2status
        sdl2text sdl2unicode sdl2window
    ].map {|x| "win/sdl2/#{x}.c"}
    case PLATFORM
    when :windows then
        winsdl2_dir << 'win/sdl2/sdl2font_windows.cpp'
        case CONFIG[:compiler]
        when :visualc then
            sdl2_flags = %Q[-I#{CONFIG[:SDL2]}/include -DPIXMAPDIR=\\".\\"]
        else
            sdl2_flags = %Q[-I#{CONFIG[:SDL2]}/include/SDL2 -DPIXMAPDIR=\\".\\"]
        end
    when :mac then
        winsdl2_dir << 'win/sdl2/sdl2font_mac.c'
        sdl2_flags = `sdl2-config --cflags`
    when :unix then
        winsdl2_dir << 'win/sdl2/sdl2font_pango.c'
        sdl2_flags = `pkg-config --cflags sdl2 pangocairo`
    end
    winsdl2_dir.each do |src|
        compile_rule(src, sdl2_flags)
    end
end

# Directory win/X11
if CONFIG[:X11_graphics] then
    winX11_dir = %w[
        dialogs  Window   winmap   winmenu  winmesg  winmisc  winstat
        wintext  winval   winX
    ].map {|x| "win/X11/#{x}.c"}
    if PLATFORM == :windows then
        x11_flags = ''
    else
        x11_flags = '-Iwin/share ' + `pkg-config --cflags xaw7 xpm xt`
    end
    winX11_dir.each do |src|
        compile_rule(src, x11_flags)
    end
end

# Directory win/Qt4
if CONFIG[:Qt_graphics] then
    winQt4_dir = %w[
        qt4bind  qt4click qt4clust qt4delay qt4glyph qt4icon  qt4inv   qt4key
        qt4line  qt4main  qt4map   qt4menu  qt4msg   qt4plsel qt4rip   qt4set
        qt4stat  qt4str   qt4streq qt4svsel qt4win   qt4xcmd  qt4yndlg
    ].map {|x| "win/Qt4/#{x}.cpp"}
    case PLATFORM
    when :windows then
        qt4_flags = %Q[-I#{QTDIR}/include -DPIXMAPDIR=\\".\\"]
    when :unix then
        if QT5 then
            qt4_flags = `pkg-config --cflags Qt5Gui Qt5Widgets Qt5Multimedia`
        else
            qt4_flags = `pkg-config --cflags QtGui`
        end
    when :mac then
        qt4_flags = `PKG_CONFIG_PATH=#{QTDIR}/lib/pkgconfig pkg-config --cflags Qt5Gui Qt5Widgets Qt5Multimedia`
    end
    qt4_flags.chomp!
    qt4_flags += ' -Ibuild/win/Qt4'
    winQt4_dir.each do |src|
        text = File.read(src)
        target = obj("build/#{src}")
        deps = [src]
        if /^\s*#\s*include\s*"hack\.h"/ =~ text then
            deps << 'include/pm.h'
            deps << 'include/onames.h'
        end
        if /^\s*#\s*include\s*"date\.h"/ =~ text then
            deps << 'include/date.h'
        end
        text2 = text
        while /^\s*#\s*include\s*"([^"]*)\.moc"/ =~ text2 do
            header = "win/Qt4/#{$1}.h"
            mocfile = "build/win/Qt4/#{$1}.moc"
            deps << mocfile
            file mocfile => header do |x|
                make_dir File.dirname(x.name)
                sh "#{CONFIG[:moc]} #{x.sources[0]} -o #{x.name}"
            end
            text2 = $'
        end
        file target => deps do |x|
            make_dir File.dirname(x.name)
            sh slash("#{CONFIG[:CXX]} #{CXXFLAGS} #{qt4_flags} -c #{src} #{objflag(target)}")
        end
    end
end

# Directory win/win32
if PLATFORM == :windows then
    win32_flags = '-Isys/winnt'
    syswinnt_dir = %w[
        win10 windmain winnt ntsound
    ].map {|x| "sys/winnt/#{x}.c"}
    syswinnt_dir.each do |src|
        compile_rule(src, win32_flags)
    end
    compile_rule('sys/winnt/stubs.c', win32_flags + ' -DTTYSTUB')
    win32_dir = %w[
        mhaskyn  mhdlg    mhfont   mhinput  mhmain   mhmap    mhmenu
        mhmsgwnd mhrip    mhsplash mhstatus mhtext   mswproc  NetHackW
    ].map {|x| "win/win32/#{x}.c"}
    win32_dir.each do |src|
        compile_rule(src, win32_flags)
    end
end

# PDCurses sources
if PLATFORM == :windows and CONFIG[:Curses_graphics] then
    pdcurses_dir = %w[
        addch    addchstr addstr   attr     beep     bkgd     border
        clear    color    debug    delch    deleteln getch    getstr
        getyx    inch     inchstr  initscr  inopts   insch    insstr
        instr    kernel   keyname  mouse    move     outopts  overlay
        pad      panel    printw   refresh  scanw    scr_dump scroll
        slk      termattr touch    util     window
    ].map {|x| "pdcurses/#{x}.c"}
    pdcurses_dir.each do |src|
        file obj("build/pdcurses/#{src}") => "#{CONFIG[:PDCurses]}/#{src}" do |x|
            dir = File.dirname(x.name)
            make_dir File.dirname(x.name)
            sh slash("#{CONFIG[:CC]} -I#{CONFIG[:PDCurses]} -DPDC_WIDE #{min_flags} -c #{objflag(x.name)} #{x.source}")
        end
    end

    wingui_dir = %w[
        pdcclip pdcdisp pdcgetsc pdckbd pdcscrn pdcsetsc pdcutil
    ].map {|x| "wingui/#{x}.c"}
    wingui_dir.each do |src|
        file obj("build/pdcurses/#{src}") => "#{CONFIG[:PDCurses]}/#{src}" do |x|
            dir = File.dirname(x.name)
            make_dir File.dirname(x.name)
            sh slash("#{CONFIG[:CC]} -I#{CONFIG[:PDCurses]} -DPDC_WIDE #{min_flags} -c #{objflag(x.name)} #{x.source}")
        end
    end
end

# libpng sources
if PLATFORM == :windows and CONFIG[:png_tiles] then
    libpng_dir = %w[
        png.c      pngerror.c pngget.c   pngmem.c   pngpread.c pngread.c
        pngrio.c   pngrtran.c pngrutil.c pngset.c   pngtrans.c pngwio.c
        pngwrite.c pngwtran.c pngwutil.c
    ]
    libpng_dir.each do |src|
        file obj("build/libpng/#{src}") => "#{CONFIG[:libpng]}/#{src}" do |x|
            dir = File.dirname(x.name)
            make_dir File.dirname(x.name)
            sh slash("#{CONFIG[:CC]} -I#{CONFIG[:libpng]} -I#{CONFIG[:zlib]} #{min_flags} -c #{objflag(x.name)} #{x.source}")
        end
    end
end

# zlib sources
if PLATFORM == :windows and (CONFIG[:png_tiles] or CONFIG[:zlib_compress]) then
    zlib_dir = %w[
        adler32.c  compress.c crc32.c    deflate.c  gzclose.c  gzlib.c
        gzread.c   gzwrite.c  infback.c  inffast.c  inflate.c  inftrees.c
        trees.c    uncompr.c  zutil.c
    ]
    zlib_dir.each do |src|
        file obj("build/zlib/#{src}") => "#{CONFIG[:zlib]}/#{src}" do |x|
            dir = File.dirname(x.name)
            make_dir File.dirname(x.name)
            sh slash("#{CONFIG[:CC]} -I#{CONFIG[:zlib]} #{min_flags} -c #{objflag(x.name)} #{x.source}")
        end
    end
end

##############################################################################
#                             The NetHack binary                             #
##############################################################################

# TODO: for Unix and Mac, set up executable permissions
nethack_ofiles = Set.new(src_dir.map {|x| obj("build/#{x}")})
if PLATFORM == :windows then
    nethack_ofiles.merge(win32_dir.map {|x| obj("build/#{x}")})
    nethack_ofiles.merge(%w[
        win/share/safeproc.c
        sys/winnt/ntsound.c
        sys/winnt/stubs.c
        sys/winnt/win10.c
        sys/winnt/windmain.c
        sys/winnt/winnt.c
        sys/share/cppregex.cpp
    ].map {|x| obj("build/#{x}")})
    nethack_ofiles << res('build/win/win32/NetHackW.rc')
else
    nethack_ofiles.merge(%w[
        sys/share/ioctl.c
        sys/share/unixtty.c
        sys/share/posixregex.c
        sys/unix/unixmain.c
        sys/unix/unixunix.c
    ].map {|x| obj("build/#{x}")})
end
nethack_libs = []
case PLATFORM
when :windows then
    if CONFIG[:png_tiles] then
        nethack_ofiles.merge(
            libpng_dir.map {|x| obj("build/libpng/#{x}")})
    end
    if CONFIG[:png_tiles] or CONFIG[:zlib_compress] then
        nethack_ofiles.merge(
            zlib_dir.map {|x| obj("build/zlib/#{x}")})
    end
when :unix, :mac then
    if CONFIG[:png_tiles] then
        nethack_libs << `pkg-config --libs libpng`.chomp
    end
    if CONFIG[:zlib_compress] then
        nethack_libs << `pkg-config --libs zlib`.chomp
    end
end
if CONFIG[:SDL2_graphics] or CONFIG[:Qt_graphics] or CONFIG[:X11_graphics] \
    or PLATFORM == :windows then
    # At least one tiled configuration is being built
    nethack_ofiles.merge(%w[
        src/tile.c
    ].map {|x| obj("build/#{x}")})
    compile_rule('src/tile.c', '')
end
if CONFIG[:SDL2_graphics] or CONFIG[:X11_graphics] or PLATFORM == :windows then
    nethack_ofiles.merge(%w[
        win/share/tileset.c
        win/share/bmptiles.c
        win/share/giftiles.c
        win/share/pngtiles.c
        win/share/xpmtiles.c
    ].map {|x| obj("build/#{x}")})
end
if CONFIG[:TTY_graphics] then
    nethack_ofiles.merge(wintty_dir.map {|x| obj("build/#{x}")})
end
if CONFIG[:Curses_graphics] then
    nethack_ofiles.merge(wincurses_dir.map {|x| obj("build/#{x}")})
    if PLATFORM == :windows then
        nethack_ofiles.merge(
            (pdcurses_dir + wingui_dir).map {|x| obj("build/pdcurses/#{x}")})
    end
end
if CONFIG[:SDL2_graphics] then
    nethack_ofiles.merge(winsdl2_dir.map {|x| obj("build/#{x}")})
    case PLATFORM
    when :windows then
        case CONFIG[:compiler]
        when :visualc then
            nethack_libs << "#{CONFIG[:SDL2]}/lib/#{CONFIG[:architecture]}/SDL2.lib"
            nethack_libs << 'setupapi.lib winmm.lib imm32.lib oleaut32.lib version.lib'
        else
            nethack_libs << "#{CONFIG[:SDL2]}/lib/libSDL2.a"
            nethack_libs << '-lsetupapi -lwinmm -limm32 -loleaut32 -lversion'
        end
    when :mac then
        nethack_libs << `sdl2-config --libs`.chomp
        nethack_libs << '-Wl,-framework,Cocoa'
    when :unix then
        nethack_libs << `pkg-config --libs sdl2 pangocairo`.chomp
    end
end
if CONFIG[:X11_graphics] then
    nethack_ofiles.merge(winX11_dir.map {|x| obj("build/#{x}")})
    nethack_libs << `pkg-config --libs xaw7 xpm xt`.chomp
end
if CONFIG[:Qt_graphics] then
    nethack_ofiles.merge(winQt4_dir.map {|x| obj("build/#{x}")})
    case PLATFORM
    when :windows then
        if QT5 then
            qt_libs = %w[Qt5Gui Qt5Widgets Qt5Multimedia Qt5Core]
        else
            qt_libs = %w[QtGui4 QtCore4]
        end
        if CONFIG[:compiler] == :visualc then
            qt_libs.map! {|x| "#{x}.lib"}
        else
            qt_libs.map! {|x| "lib#{x}.a"}
        end
        nethack_libs << (qt_libs.map {|x| "#{QTDIR}/lib/#{x}"}).join(' ')
    when :unix then
        if QT5 then
            nethack_libs << `pkg-config --libs Qt5Gui Qt5Widgets Qt5Multimedia`.chomp
        else
            nethack_libs << `pkg-config --libs QtGui`.chomp
        end
    when :mac then
        nethack_libs << `PKG_CONFIG_PATH=#{QTDIR}/lib/pkgconfig pkg-config --libs Qt5Gui Qt5Widgets Qt5Multimedia`.chomp
    end
end
if CONFIG[:TTY_graphics] or CONFIG[:Curses_graphics] then
    # Curses library for Unix and Mac
    case PLATFORM
    when :mac then
        nethack_libs << '-lncurses'
    when :unix then
        nethack_libs << `pkg-config --libs ncursesw`.chomp
    end
end
if PLATFORM == :windows then
    case CONFIG[:compiler]
    when :visualc then
        nethack_libs << '-link -subsystem:windows'
        nethack_libs << 'kernel32.lib advapi32.lib gdi32.lib user32.lib comctl32.lib comdlg32.lib winspool.lib winmm.lib bcrypt.lib shell32.lib ole32.lib'
    else
        nethack_libs << '-lgdi32 -lcomctl32 -lcomdlg32 -lwinmm -lbcrypt -lshell32 -lole32'
    end
end
link_rule(nethack_ofiles.to_a, exe('binary/nethack'), nethack_libs)

##############################################################################
#                          nhdat and its components                          #
##############################################################################

dat_files = %w[
    cmdhelp
    help
    hh
    history
    keyhelp
    opthelp
    tribute
    wizhelp
    options
    bogusmon
    data
    engrave
    epitaph
    oracles
    quest.dat
    rumors
    dungeon
]

level_sources = %w[
    bigroom  castle   endgame  gehennom knox     medusa   mines    oracle
    sokoban  tower    yendor   Arch     Barb     Caveman  Healer   Knight
    Monk     Priest   Ranger   Rogue    Samurai  Tourist  Valkyrie Wizard
]

level_tags = level_sources.map {|x| File.join('build', "#{x}.tag")}

level_files = %w[
    asmodeus.lev
    baalz.lev
    bigrm-1.lev
    bigrm-2.lev
    bigrm-3.lev
    bigrm-4.lev
    bigrm-5.lev
    bigrm-6.lev
    bigrm-7.lev
    bigrm-8.lev
    bigrm-9.lev
    bigrm-10.lev
    castle.lev
    fakewiz1.lev
    fakewiz2.lev
    juiblex.lev
    knox.lev
    medusa-1.lev
    medusa-2.lev
    medusa-3.lev
    medusa-4.lev
    minend-1.lev
    minend-2.lev
    minend-3.lev
    minefill.lev
    minetn-1.lev
    minetn-2.lev
    minetn-3.lev
    minetn-4.lev
    minetn-5.lev
    minetn-6.lev
    minetn-7.lev
    oracle.lev
    orcus.lev
    sanctum.lev
    soko1-1.lev
    soko1-2.lev
    soko2-1.lev
    soko2-2.lev
    soko3-1.lev
    soko3-2.lev
    soko4-1.lev
    soko4-2.lev
    tower1.lev
    tower2.lev
    tower3.lev
    valley.lev
    wizard1.lev
    wizard2.lev
    wizard3.lev
    astral.lev
    air.lev
    earth.lev
    fire.lev
    water.lev
    Arc-fila.lev
    Arc-filb.lev
    Arc-goal.lev
    Arc-loca.lev
    Arc-strt.lev
    Bar-fila.lev
    Bar-filb.lev
    Bar-goal.lev
    Bar-loca.lev
    Bar-strt.lev
    Cav-fila.lev
    Cav-filb.lev
    Cav-goal.lev
    Cav-loca.lev
    Cav-strt.lev
    Hea-fila.lev
    Hea-filb.lev
    Hea-goal.lev
    Hea-loca.lev
    Hea-strt.lev
    Kni-fila.lev
    Kni-filb.lev
    Kni-goal.lev
    Kni-loca.lev
    Kni-strt.lev
    Mon-fila.lev
    Mon-filb.lev
    Mon-goal.lev
    Mon-loca.lev
    Mon-strt.lev
    Pri-fila.lev
    Pri-filb.lev
    Pri-goal.lev
    Pri-loca.lev
    Pri-strt.lev
    Ran-fila.lev
    Ran-filb.lev
    Ran-goal.lev
    Ran-loca.lev
    Ran-strt.lev
    Rog-fila.lev
    Rog-filb.lev
    Rog-goal.lev
    Rog-loca.lev
    Rog-strt.lev
    Sam-fila.lev
    Sam-filb.lev
    Sam-goal.lev
    Sam-loca.lev
    Sam-strt.lev
    Tou-fila.lev
    Tou-filb.lev
    Tou-goal.lev
    Tou-loca.lev
    Tou-strt.lev
    Val-fila.lev
    Val-filb.lev
    Val-goal.lev
    Val-loca.lev
    Val-strt.lev
    Wiz-fila.lev
    Wiz-filb.lev
    Wiz-goal.lev
    Wiz-loca.lev
    Wiz-strt.lev
]

file 'binary/nhdat' => ([
    exe('build/dlb')
] + dat_files.map {|x| "dat/#{x}"}) + level_tags do |x|
    make_dir File.dirname(x.name)
    sh slash("build/dlb Ccf dat ../binary/nhdat #{dat_files.join(' ')} #{level_files.join(' ')}")
end

##############################################################################
#                             The recover program                            #
##############################################################################

recover_ofiles = %w[
    util/recover.c
].map {|x| obj('build/'+x)}
recover_exe = exe('binary/recover')

link_rule(recover_ofiles, recover_exe, [])

##############################################################################
#                               Other targets                                #
##############################################################################

copy_targets = {
    'dat/license' => 'binary/license',
    'dat/symbols' => 'binary/symbols',
    'dat/opthelp' => 'binary/opthelp',
    'doc/Guidebook.txt' => 'binary/Guidebook.txt',
    'win/X11/NetHack.ad' => 'binary/NetHack.ad',
    'win/Qt/nhsplash.xpm' => 'binary/nhsplash.xpm',
    'win/X11/pet_mark.xbm' => 'binary/pet_mark.xbm',
    'win/X11/pilemark.xbm' => 'binary/pilemark.xbm',
    'win/X11/rip.xpm' => 'binary/rip.xpm',
    'sys/unix/sysconf' => 'binary/sysconf',
    'sys/winnt/sysconf.template' => 'binary/sysconf.template',
    'sys/winnt/.nethackrc.template' => 'binary/.nethackrc.template',
    'binary/nhtiles.bmp' => 'build/tiles.bmp',
    File.join(CONFIG[:SDL2], 'lib', CONFIG[:architecture], 'SDL2.dll') => 'binary/SDL2.dll'
}
if PLATFORM == :windows then
    copy_targets['dat/symbols'] = 'binary/symbols.template'
end
if defined?(QTDIR) and QTDIR then
    %w[QtCore4 QtGui4 Qt5Gui Qt5Widgets Qt5Multimedia Qt5Core].each do |name|
        copy_targets[File.join(QTDIR, 'bin', "#{name}.dll")] = "binary/#{name}.dll"
    end
end

copy_targets.each do |from, to|
    file to => from do |x|
        make_dir File.dirname(x.name)
        cp(x.source, x.name)
    end
end

touch_targets = [
    'binary/logfile',
    'binary/perm',
    'binary/record',
    'binary/xlogfile'
]

touch_targets.each do |name|
    file name do |x|
        make_dir File.dirname(x.name)
        touch x.name unless File.file?(x.name)
    end
end

file 'binary/save' do |x|
    make_dir x.name
end

##############################################################################
#                          tilemap and its product                           #
##############################################################################

tilemap_ofiles = %w[
    win/share/tilemap.c
].map {|x| obj('build/'+x)}
tilemap_exe = exe('build/tilemap')

link_rule(tilemap_ofiles, tilemap_exe, [])

file 'src/tile.c' => tilemap_exe do |x|
    in_dir 'src' do
        sh '../build/tilemap'
    end
end

##############################################################################
#                                  x11tiles                                  #
##############################################################################

# Some dependencies are shared between tile2x11 and tile2bmp
tile2x11_ofiles = %w[
    win/X11/tile2x11.c
    win/share/tiletext.c
    win/share/tiletxt2.c
    src/decl.c
    src/drawing.c
    src/monst.c
    src/objects.c
].map {|x| obj('build/'+x)}
tile2x11_exe = exe('build/tile2x11')

compile_rule('win/X11/tile2x11.c', x11_flags)

file obj('build/win/share/tiletxt2.c') => %w[
    win/share/tilemap.c include/pm.h include/onames.h
] do |x|
    sh slash("#{CONFIG[:CC]} #{CFLAGS} -DTILETEXT -c win/share/tilemap.c #{objflag(obj('build/win/share/tiletxt2.c'))}")
end

link_rule(tile2x11_ofiles, tile2x11_exe, [])

file 'binary/x11tiles' => [
    tile2x11_exe,
    'win/share/monsters.txt', 'win/share/objects.txt', 'win/share/other.txt'
] do |x|
    in_dir 'binary' do
        sh '../build/tile2x11 ../win/share/monsters.txt ../win/share/objects.txt ' \
           '../win/share/other.txt -grayscale ../win/share/monsters.txt'
    end
end

##############################################################################
#                                nhtiles.bmp                                 #
##############################################################################

tile2bmp_ofiles = %w[
    win/share/tile2bmp.c
    win/share/tiletext.c
    win/share/tiletxt2.c
    src/decl.c
    src/drawing.c
    src/monst.c
    src/objects.c
].map {|x| obj('build/'+x)}
tile2bmp_exe = exe('build/tile2bmp')

tile2bmp_flags = '-Isys/winnt'
case CONFIG[:compiler]
when :gcc, :clang then
    tile2bmp_flags += ' -mno-ms-bitfields'
end
compile_rule('win/share/tile2bmp.c', tile2bmp_flags)

file obj('build/win/share/tiletxt2.c') => %w[
    win/share/tilemap.c include/pm.h include/onames.h
] do |x|
    sh slash("#{CONFIG[:CC]} #{CFLAGS} -DTILETEXT -c win/share/tilemap.c #{objflag(obj('build/win/share/tiletxt2.c'))}")
end

link_rule(tile2bmp_ofiles, tile2bmp_exe, [])

file 'binary/nhtiles.bmp' => [
    tile2bmp_exe,
    'win/share/monsters.txt', 'win/share/objects.txt', 'win/share/other.txt'
] do |x|
    make_dir File.dirname(x.name)
    in_dir 'build' do
        sh "./tile2bmp #{File.join('..', x.name)}"
    end
end

##############################################################################
#                    The dungeon compiler and its product                    #
##############################################################################

dgn_comp_exe = exe('build/dgn_comp')
dgn_comp_ofiles = %w[
    util/dgn_main.c
    util/panic.c
    src/alloc.c
].map {|x| obj('build/'+x)}

if CONFIG[:lex] and CONFIG[:yacc] then
    dgn_comp_ofiles += %w[
        build/util/dgn_lex.c
        build/util/dgn_yacc.c
    ].map {|x| obj(x)}

    file 'build/util/dgn_lex.c.o' => %w[
        build/util/dgn_lex.c
        build/util/dgn_comp.h
    ] do |x|
        sh slash("#{CONFIG[:CC]} #{CFLAGS} -c #{x.source} #{objflag(x.name)}")
    end

    file 'build/util/dgn_yacc.c.o' => 'build/util/dgn_yacc.c' do |x|
        sh slash("#{CONFIG[:CC]} #{CFLAGS} -c #{x.source} #{objflag(x.name)}")
    end

    file 'build/util/dgn_lex.c' => 'util/dgn_comp.l' do |x|
        make_dir File.dirname(x.name)
        sh "#{CONFIG[:lex]} -o build/util/dgn_lex.c util/dgn_comp.l"
    end

    file 'build/util/dgn_yacc.c' => 'util/dgn_comp.y' do |x|
        make_dir File.dirname(x.name)
        sh "#{CONFIG[:yacc]} -d -o build/util/dgn_yacc.c util/dgn_comp.y"
        mv "build/util/dgn_yacc.h", "build/util/dgn_comp.h"
    end

    file 'build/util/dgn_comp.h' => 'util/dgn_comp.y' do |x|
        make_dir File.dirname(x.name)
        sh "#{CONFIG[:yacc]} -d -o build/util/dgn_yacc.c util/dgn_comp.y"
        mv "build/util/dgn_yacc.h", "build/util/dgn_comp.h"
    end
else
    dgn_comp_ofiles += %w[
        sys/share/dgn_yacc.c
        sys/share/dgn_lex.c
    ].map {|x| obj('build/'+x)}
end

link_rule(dgn_comp_ofiles, dgn_comp_exe, [])

file 'dat/dungeon' => [ dgn_comp_exe, 'dat/dungeon.pdf' ] do |x|
    sh slash("build/dgn_comp dat/dungeon.pdf")
end

##############################################################################
#                    The level compiler and its products                     #
##############################################################################

lev_comp_exe = exe('build/lev_comp')
lev_comp_ofiles = %w[
    util/lev_main.c
    util/panic.c
    src/alloc.c
    src/decl.c
    src/drawing.c
    src/monst.c
    src/objects.c
].map {|x| obj('build/'+x)}

if CONFIG[:lex] and CONFIG[:yacc] then
    lev_comp_ofiles += %w[
        build/util/lev_lex.c
        build/util/lev_yacc.c
    ].map {|x| obj(x)}

    file 'build/util/lev_lex.c.o' => %w[
        build/util/lev_lex.c
        build/util/lev_comp.h
        include/pm.h
        include/onames.h
    ] do |x|
        sh slash("#{CONFIG[:CC]} #{CFLAGS} -c #{x.source} #{objflag(x.name)}")
    end

    file 'build/util/lev_yacc.c.o' => %w[
        build/util/lev_yacc.c
        include/pm.h
        include/onames.h
    ] do |x|
        sh slash("#{CONFIG[:CC]} #{CFLAGS} -c #{x.source} #{objflag(x.name)}")
    end

    file 'build/util/lev_lex.c' => 'util/lev_comp.l' do |x|
        make_dir File.dirname(x.name)
        sh "#{CONFIG[:lex]} -o build/util/lev_lex.c util/lev_comp.l"
    end

    file 'build/util/lev_yacc.c' => 'util/lev_comp.y' do |x|
        make_dir File.dirname(x.name)
        sh "#{CONFIG[:yacc]} -d -o build/util/lev_yacc.c util/lev_comp.y"
        mv "build/util/lev_yacc.h", "build/util/lev_comp.h"
    end

    file 'build/util/lev_comp.h' => 'util/lev_comp.y' do |x|
        make_dir File.dirname(x.name)
        sh "#{CONFIG[:yacc]} -d -o build/util/lev_yacc.c util/lev_comp.y"
        mv "build/util/lev_yacc.h", "build/util/lev_comp.h"
    end
else
    lev_comp_ofiles += %w[
        sys/share/lev_yacc.c
        sys/share/lev_lex.c
    ].map {|x| obj('build/'+x)}
end

link_rule(lev_comp_ofiles, lev_comp_exe, [])

level_sources.each do |src|
    file "build/#{src}.tag" => [ lev_comp_exe, "dat/#{src}.des" ] do |x|
        in_dir 'dat' do
            sh slash("../build/lev_comp #{src}.des")
        end
        touch x.name
    end
end

##############################################################################
#                         makedefs and its products                          #
##############################################################################

makedefs_exe = exe('build/makedefs')
makedefs_ofiles = %w[
    util/makedefs.c
    src/monst.c
    src/objects.c
].map {|x| obj('build/'+x)}

link_rule(makedefs_ofiles, makedefs_exe, [])

def makedefs(flag)
    in_dir 'build' do
        sh "./makedefs #{flag}"
    end
end

file 'include/pm.h' => makedefs_exe do
    makedefs('-p')
end

file 'include/onames.h' => makedefs_exe do
    makedefs('-o')
end

file 'include/date.h' => makedefs_exe do
    makedefs('-v')
end

file 'dat/options' => makedefs_exe do
    makedefs('-v')
end

file 'dat/bogusmon' => [ makedefs_exe, 'dat/bogusmon.txt' ] do
    makedefs('-s')
end

file 'dat/engrave' =>[ makedefs_exe, 'dat/engrave.txt' ] do
    makedefs('-s')
end

file 'dat/epitaph' =>[ makedefs_exe, 'dat/epitaph.txt' ] do
    makedefs('-s')
end

file 'dat/data' =>[ makedefs_exe, 'dat/data.base' ] do
    makedefs('-d')
end

file 'dat/dungeon.pdf' =>[ makedefs_exe, 'dat/dungeon.def' ] do
    makedefs('-e')
end

file 'dat/oracles' =>[ makedefs_exe, 'dat/oracles.txt' ] do
    makedefs('-h')
end

file 'dat/quest.dat' =>[ makedefs_exe, 'dat/quest.txt' ] do
    makedefs('-q')
end

file 'dat/rumors' =>[ makedefs_exe, 'dat/rumors.fal', 'dat/rumors.tru' ] do
    makedefs('-r')
end


##############################################################################
#                             The data librarian                             #
##############################################################################

dlb_exe = exe('build/dlb')
dlb_ofiles = %w[
    util/dlb_main.c
    util/panic.c
    src/alloc.c
    src/dlb.c
].map {|x| obj('build/'+x)}

link_rule(dlb_ofiles, dlb_exe, [])

##############################################################################
#                              Win32 resources                               #
##############################################################################

file res('build/win/win32/NetHackW.rc') => [
    'win/win32/NetHackW.rc',
    'build/nethack.ico',
    'build/tiles.bmp',
    'build/mnsel.bmp',
    'build/mnunsel.bmp',
    'build/petmark.bmp',
    'build/pilemark.bmp',
    'build/mnselcnt.bmp',
    'build/rip.bmp',
    'build/splash.bmp'
] do |x|
    sh slash("#{CONFIG[:rc]} -Ibuild -Iwin/win32 #{objflag(x.name)} win/win32/NetHackW.rc")
end

##############################################################################
#                         uudecode and its products                          #
##############################################################################

uudecode_ofiles = %w[
    sys/share/uudecode.c
].map {|x| obj('build/'+x)}
uudecode_exe = exe('build/uudecode')

link_rule(uudecode_ofiles, uudecode_exe, [])

file 'build/nethack.ico' => [ uudecode_exe, 'sys/winnt/nhico.uu' ] do |x|
    make_dir File.dirname(x.name)
    in_dir 'build' do
        sh "./uudecode ../sys/winnt/nhico.uu"
    end
end

file 'build/nethack.ico' => [ uudecode_exe, 'sys/winnt/nhico.uu' ] do |x|
    make_dir File.dirname(x.name)
    in_dir 'build' do
        sh "./uudecode ../sys/winnt/nhico.uu"
    end
end

%w[mnsel mnunsel petmark pilemark mnselcnt rip splash].each do |name|
    file "build/#{name}.bmp" => [ uudecode_exe, "win/win32/#{name}.uu" ] do |x|
        make_dir File.dirname(x.name)
        in_dir 'build' do
            sh "./uudecode ../win/win32/#{name}.uu"
        end
    end
end

##############################################################################
#                                Install task                                #
##############################################################################

task :install do
    make_dir HACKDIR
    targets.each do |target|
        path = File.join(HACKDIR, File.basename(target))
        case target
        when 'binary/logfile', \
             'binary/perm', \
             'binary/record', \
             'binary/xlogfile' then
            touch path unless File.file?(path)
        when 'binary/save' then
            make_dir path
        else
            cp target, path
        end
    end
end

##############################################################################
#                                  Cleanup                                   #
##############################################################################

task :clean do
    rm_rf 'build'
    rm_f 'dat/options'
    rm_f 'dat/bogusmon'
    rm_f 'dat/dungeon'
    rm_f 'dat/dungeon.pdf'
    rm_f 'dat/engrave'
    rm_f 'dat/epitaph'
    rm_f 'dat/data'
    rm_f 'dat/oracles'
    rm_f 'dat/quest.dat'
    rm_f 'dat/rumors'
    rm_f 'include/date.h'
    rm_f 'include/onames.h'
    rm_f 'include/pm.h'
    rm_f 'src/tile.c'
    lev = Dir.glob('dat/*.lev')
    rm_f lev unless lev.empty?
end
