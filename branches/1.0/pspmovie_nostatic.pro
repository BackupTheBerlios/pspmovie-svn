TEMPLATE	= app
LANGUAGE	= C++

CONFIG	+= qt debug

unix:LIBS	+= -lhal -lhal-storage -lavformat -lavcodec -lmp4v2

unix:INCLUDEPATH	+= /usr/include/hal /usr/include/dbus-1.0/ /usr/lib/dbus-1.0/include/

HEADERS	+= pspmovie.h \
	pspdetect.h \
	avutils.h \
	ffmpeg_glue.h \
	ffmpeg_opt.h \
	ffmpeg_cmdutils.h

SOURCES	+= pspmovie.cpp \
	avutils.cpp \
	ffmpeg_cmdutils.c \
	ffmpeg_opt.c \
	ffmpeg_patched.c

FORMS	= mainwin.ui \
	newjobdialog.ui \
	xferwin.ui

IMAGES	= images/stock_calc-cancel.png \
	images/stock_insert-slide.png \
	images/stock_stop.png \
	images/stock_refresh.png \
	images/stock_tools-macro.png \
	images/stock_data-undo.png \
	images/stock_close.png \
	images/stock_view-details-16.png \
	images/stock_open.png \
	images/filenew \
	images/fileopen \
	images/filesave \
	images/print \
	images/undo \
	images/redo \
	images/editcut \
	images/editcopy \
	images/editpaste \
	images/searchfind \
	images/stock_down.png \
	images/stock_up.png \
	images/stock_left.png \
	images/stock_right.png \
	images/sonypsp.png

unix {
  UI_DIR = .ui
  MOC_DIR = .moc
  OBJECTS_DIR = .obj
  SOURCES += pspdetect_linux.cpp
  }

