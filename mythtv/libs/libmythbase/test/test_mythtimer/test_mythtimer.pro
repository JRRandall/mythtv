include ( ../../../../settings.pro )

QT += testlib

TEMPLATE = app
TARGET = test_mythtimer
DEPENDPATH += . ../..
INCLUDEPATH += . ../..

contains(QMAKE_CXX, "g++") {
  QMAKE_CXXFLAGS += -O0 -fprofile-arcs -ftest-coverage 
  QMAKE_LFLAGS += -fprofile-arcs 
}

QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../..

# Input
HEADERS += test_mythtimer.h
SOURCES += test_mythtimer.cpp

HEADERS += ../../mythtimer.h
SOURCES += ../../mythtimer.cpp

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
