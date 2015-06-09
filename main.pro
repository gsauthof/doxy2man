TEMPLATE = app
TARGET = doxy2man
QT += xml
QT += xmlpatterns
CONFIG += debug
CONFIG += warn_off

QMAKE_CXXFLAGS_DEBUG = -g -Wall -Wno-unused-parameter -Wno-switch

HEADERS += main.h
SOURCES += main.cc 

doc.target = doxy2man.8
doc.commands = asciidoc.py -v -d manpage -b docbook doxy2man.8.txt && xsltproc --nonet -o doxy2man.8 /usr/share/asciidoc/docbook-xsl/manpage.xsl doxy2man.8.xml
QMAKE_EXTRA_TARGETS += doc
