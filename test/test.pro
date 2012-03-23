TEMPLATE = app
TARGET =
DEPENDPATH += .
INCLUDEPATH += .

# No GUI
QT -= gui
QT += sql

# Input
HEADERS += fct.h \
	../model/model.h

SOURCES += tests.cpp \
	../model/model.cpp

