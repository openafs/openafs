AFSCLIENTROOT = q:\afs\client

OBJS = \
	afscreds.res \
	advtab.obj \
	afswiz.obj \
	checklist.obj \
	creds.obj \
	credstab.obj \
	drivemap.obj \
	main.obj \
	misc.obj \
	mounttab.obj \
	msprintf.obj \
	settings.obj \
	shortcut.obj \
	subclass.obj \
	trayicon.obj \
	window.obj \
	wizard.obj

LIBRARIES = \
	user32.lib \
	shell32.lib \
	comctl32.lib \
	kernel32.lib \
	advapi32.lib \
	uuid.lib \
	ole32.lib \
	gdi32.lib \
	wsock32.lib \
	mpr.lib \
	$(AFSCLIENTROOT)\obj\afsd\libafstokens.lib \
	$(AFSCLIENTROOT)\obj\afsd\libafsconf.lib

TARGET = .\afscreds.exe

APPVER = 4.0
TARGETOS = BOTH
!include $(MSTOOLS)\include\win32.mak

cflags = $(cflags) -I$(AFSCLIENTROOT)\src
cflags = $(cflags) -I$(AFSCLIENTROOT)\src\rxkad
cflags = $(cflags) -I$(AFSCLIENTROOT)\src\afsd
cflags = $(cflags) -GX
cflags = $(cflags) -DSTRICT

guilflags = $(guilflags) -MACHINE:$(CPU)

cflags = $(cflags) -Zi -Od -DDEBUG -D_DEBUG -DDBG
lflags = $(lflags) -debug:full -debugtype:both

.rc.res :
	$(rc) $(rcflags) $(rcvars) $*.rc

.cpp.obj :
	$(cc) $(cflags) $(cvars) $*.cpp


all : $(TARGET)

clean :
	@if exist *.obj del *.obj
	@if exist *.exe del *.exe
	@if exist *.aps del *.aps
	@if exist *.res del *.res
	@if exist *.lib del *.lib
	@if exist *.pdb del *.pdb

$(TARGET) : $(OBJS)
	$(link) $(guilflags) $(libcmt) $(OBJS) $(LIBRARIES)

