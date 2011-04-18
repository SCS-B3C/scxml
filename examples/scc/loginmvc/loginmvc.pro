TEMPLATE = app
CONFIG += console scc
SOURCES = main.cpp
FORMS += frame.ui
QT += network
STATECHARTS += controller.scxml
OTHER_FILES += controller.scxml
CONFIG -= app_bundle
