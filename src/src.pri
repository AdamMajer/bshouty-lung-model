SOURCES += \
	$${SRC_DIR}/about.cpp \
	$${SRC_DIR}/calculationresultdlg.cpp \
	$${SRC_DIR}/calibratedlg.cpp \
	$${SRC_DIR}/capillary.cpp \
	$${SRC_DIR}/capillaryview.cpp \
	$${SRC_DIR}/common.cpp \
	$${SRC_DIR}/dbsettings.cpp \
	$${SRC_DIR}/diseaselistdlg.cpp \
	$${SRC_DIR}/diseasemodel.cpp \
	$${SRC_DIR}/diseaseparamdelegate.cpp \
	$${SRC_DIR}/init.cpp \
	$${SRC_DIR}/lungview.cpp \
	$${SRC_DIR}/modelscene.cpp \
	$${SRC_DIR}/mainwindow.cpp \
	$${SRC_DIR}/multimodeloutput.cpp \
	$${SRC_DIR}/opencldlg.cpp \
	$${SRC_DIR}/opencl.cpp \
	$${SRC_DIR}/overlaymapwidget.cpp \
	$${SRC_DIR}/overlaysettingsdlg.cpp \
	$${SRC_DIR}/rangelineedit.cpp \
	$${SRC_DIR}/rangestepdlg.cpp \
	$${SRC_DIR}/scripteditdlg.cpp \
	$${SRC_DIR}/scripthighlighter.cpp \
	$${SRC_DIR}/specialgeometricflowwidget.cpp \
	$${SRC_DIR}/transducerview.cpp \
	$${SRC_DIR}/vesseldlg.cpp \
	$${SRC_DIR}/vesselview.cpp \
	$${SRC_DIR}/vesselwidget.cpp \
	$${SRC_DIR}/vesselconnectionview.cpp

HEADERS += \
	$${SRC_DIR}/about.h \
	$${SRC_DIR}/calculationresultdlg.h \
	$${SRC_DIR}/calibratedlg.h \
	$${SRC_DIR}/capillary.h \
	$${SRC_DIR}/capillaryview.h \
	$${SRC_DIR}/common.h \
	$${SRC_DIR}/dbsettings.h \
	$${SRC_DIR}/diseaselistdlg.h \
	$${SRC_DIR}/diseasemodel.h \
	$${SRC_DIR}/diseaseparamdelegate.h \
	$${SRC_DIR}/lungview.h \
	$${SRC_DIR}/mainwindow.h \
	$${SRC_DIR}/modelscene.h \
	$${SRC_DIR}/multimodeloutput.h \
	$${SRC_DIR}/opencldlg.h \
	$${SRC_DIR}/opencl.h \
	$${SRC_DIR}/overlaymapwidget.h \
	$${SRC_DIR}/overlaysettingsdlg.h \
	$${SRC_DIR}/rangelineedit.h \
	$${SRC_DIR}/rangestepdlg.h \
	$${SRC_DIR}/scripteditdlg.h \
	$${SRC_DIR}/scripthighlighter.h \
	$${SRC_DIR}/specialgeometricflowwidget.h \
	$${SRC_DIR}/transducerview.h \
	$${SRC_DIR}/vesseldlg.h \
	$${SRC_DIR}/vesselview.h \
	$${SRC_DIR}/vesselwidget.h \
	$${SRC_DIR}/vesselconnectionview.h

FORMS += \
	$${SRC_DIR}/about.ui \
	$${SRC_DIR}/calculationresultdlg.ui \
	$${SRC_DIR}/calibratedlg.ui \
	$${SRC_DIR}/capillary.ui \
	$${SRC_DIR}/diseaselistdlg.ui \
	$${SRC_DIR}/mainwindow.ui \
	$${SRC_DIR}/multimodeloutput.ui \
	$${SRC_DIR}/opencldlg.ui \
	$${SRC_DIR}/overlaysettingsdlg.ui \
	$${SRC_DIR}/rangestepdlg.ui \
	$${SRC_DIR}/scripteditdlg.ui \
	$${SRC_DIR}/vesseldlg.ui

RESOURCES += $${SRC_DIR}/images.qrc
RC_FILE = $${SRC_DIR}/model.rc

include(model/model.pri)

