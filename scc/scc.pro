# -------------------------------------------------
# Project created by QtCreator 2009-08-02T10:07:01
# -------------------------------------------------
QT += xmlpatterns
QT -= gui
TARGET = scc
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app
SOURCES += main.cpp
RESOURCES += scc.qrc
target.path = $$[QT_INSTALL_PREFIX]/bin
prf.files = scc.prf
prf.path = $$[QMAKE_MKSPECS]/features
INSTALLS += target prf
OTHER_FILES += scc.prf
