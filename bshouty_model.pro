TEMPLATE = app
TARGET =
DEPENDPATH += .

QT += sql script

UI_DIR   = build/ui
MOC_DIR  = build/moc
OBJECTS_DIR = build/obj

SRC_DIR = src

INCLUDEPATH += $${SRC_DIR}

win32:LIBS += -lshell32

# Visual Studio compiler flags so we get debugging symbols files in release mode
win32:QMAKE_CXXFLAGS_RELEASE += /Zi
win32:QMAKE_LFLAGS_RELEASE += /DEBUG

# RESOURCES += images.qrc
# RC_FILE = model.rc

include($${SRC_DIR}/src.pri)
