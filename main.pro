TEMPLATE = app
TARGET = doxy2man
QT += xml
QT += xmlpatterns
CONFIG += debug
CONFIG += warn_off

QMAKE_CXXFLAGS_DEBUG = -g -Wall -Wno-unused-parameter -Wno-switch

HEADERS += main.h
SOURCES += main.cc 

