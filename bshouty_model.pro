TEMPLATE = app
TARGET = bshouty_model
DEPENDPATH += .

QT += sql script

greaterThan(QT_MAJOR_VERSION, 4) {
	QT += widgets concurrent
}

UI_DIR   = build/ui
MOC_DIR  = build/moc
OBJECTS_DIR = build/obj

SRC_DIR = src

INCLUDEPATH += $${SRC_DIR}

win32 {
	LIBS += -lshell32
	INCLUDEPATH += "C:/Program Files (x86)/AMD APP/include"

	# Visual Studio compiler flags so we get debugging symbols files in release mode
	contains(QMAKE_COMPILER_DEFINES, _MSC_VER) {
		QMAKE_CXXFLAGS_RELEASE += /Zi
		QMAKE_LFLAGS_RELEASE += /DEBUG
	}
}

# RESOURCES += images.qrc
# RC_FILE = model.rc

include($${SRC_DIR}/src.pri)
