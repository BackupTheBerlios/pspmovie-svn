template = app

CONFIG += qt debug

FORMS += transcode.ui mainwin.ui xferwin.ui

SOURCES += pspmovie.cpp \
	avutils.cpp \
	ffmpeg_patched.c \
	transcode.cpp \
	xferwin.cpp \
	mainwin.cpp

SOURCES += pspdetect_linux.cpp

HEADERS += avutils.h pspdetect.h \
	transcode.h mainwin.h xferwin.h


RESOURCES	= pspmovie.qrc

unix:LIBS	+= -lhal -lhal-storage ffmpeg/libavformat/libavformat.a ffmpeg/libavcodec/libavcodec.a ffmpeg/libavutil/libavutil.a -lmp4v2 -lfaad -lfaac

unix:INCLUDEPATH	+= . /usr/include/hal /usr/include/dbus-1.0/ /usr/lib/dbus-1.0/include/
