#-------------------------------------------------
#
# Project created by QtCreator 2014-08-23T14:55:40
#
#-------------------------------------------------

QT       -= gui

TARGET = tiBackupLib
TEMPLATE = lib

DEFINES += TIBACKUPLIB_LIBRARY

SOURCES += tibackuplib.cpp \
    obj/devicedisk.cpp \
    tibackupdiskobserver.cpp \
    ticonf.cpp \
    obj/tibackupjob.cpp \
    tibackupservice.cpp \
    tibackupapi.cpp \
    workers/tibackupjobworker.cpp \
    obj/exitcodes.cpp

HEADERS += tibackuplib.h\
        tibackuplib_global.h \
    obj/devicedisk.h \
    tibackupdiskobserver.h \
    ticonf.h \
    config.h \
    obj/tibackupjob.h \
    tibackupservice.h \
    tibackupapi.h \
    workers/tibackupjobworker.h \
    obj/exitcodes.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}

unix:!macx:!symbian: LIBS += -lPocoFoundation -lPocoNet -ludev -lblkid

#INCLUDEPATH += /usr/local/include
QMAKE_CXXFLAGS_DEBUG += -pipe
QMAKE_CXXFLAGS_RELEASE += -pipe -O2

RESOURCES += \
    resdata.qrc

DISTFILES += \
    debian/changelog \
    tibackuplib-dev/debian/changelog
