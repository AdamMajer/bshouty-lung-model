TEMPLATE = app
TARGET =
DEPENDPATH += .
INCLUDEPATH += .

QT += sql script

UI_DIR   = build/ui
MOC_DIR  = build/moc
OBJECTS_DIR = build/obj


win32:LIBS += -lshell32

# Visual Studio compiler flags so we get debugging symbols files in release mode
win32:QMAKE_CXXFLAGS_RELEASE += /Zi
win32:QMAKE_LFLAGS_RELEASE += /DEBUG

# QMAKE_CXXFLAGS += -std=c++0x

# Input
SOURCES += \
	about.cpp \
	capillary.cpp \
	capillaryview.cpp \
	clickablelineedit.cpp \
	common.cpp \
	dbsettings.cpp \
	diseaselistdlg.cpp \
	diseasemodel.cpp \
	diseaseparamdelegate.cpp \
	init.cpp \
	lungview.cpp \
	modelscene.cpp \
	mainwindow.cpp \
	multimodeloutput.cpp \
	opencldlg.cpp \
	opencl.cpp \
	rangestepdlg.cpp \
	scripteditdlg.cpp \
	scripthighlighter.cpp \
	transducerview.cpp \
	vesseldlg.cpp \
	vesselview.cpp \
	vesselconnectionview.cpp \
	wizard.cpp

HEADERS += \
	about.h \
	capillary.h \
	capillaryview.h \
	clickablelineedit.h \
	common.h \
	dbsettings.h \
	diseaselistdlg.h \
	diseasemodel.h \
	diseaseparamdelegate.h \
	lungview.h \
	mainwindow.h \
	modelscene.h \
	multimodeloutput.h \
	opencldlg.h \
	opencl.h \
	rangestepdlg.h \
	scripteditdlg.h \
	scripthighlighter.h \
	transducerview.h \
	vesseldlg.h \
	vesselview.h \
	vesselconnectionview.h \
	wizard.h

FORMS += \
	about.ui \
	capillary.ui \
	diseaselistdlg.ui \
	mainwindow.ui \
	multimodeloutput.ui \
	opencldlg.ui \
	rangestepdlg.ui \
	scripteditdlg.ui \
	vesseldlg.ui

RESOURCES += images.qrc
RC_FILE = model.rc

include(wizard/wizard.pri)
include(model/model.pri)

