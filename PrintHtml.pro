# -------------------------------------------------
# QMake project for the PrintHtml program
# -------------------------------------------------
TARGET = PrintHtml
TEMPLATE = app
CONFIG += precompile_header
INCLUDEPATH += ../includes ../common
DEPENDPATH += ../includes ../common
PRECOMPILED_HEADER = stable.h
precompile_header:!isEmpty(PRECOMPILED_HEADER):DEFINES += USING_PCH
QT += network webkit testlib # Added testlib

HEADERS = stable.h \
    globals.h \
    printhtml.h \
    restserver.h \
    scanimage.h   # Added scanimage.h
SOURCES = main.cpp \
    printhtml.cpp \
    restserver.cpp \
    scanimage.cpp   # Added scanimage.cpp
FORMS =
RESOURCES =

DISTFILES += \
    LICENSE \
    README.md \
    .gitignore

# Test target configuration
tests {
    TARGET = testprinthtml
    CONFIG += testcase console
    CONFIG -= app_bundle // Ensure it's a console app for tests
    SOURCES += testscanimage.cpp // Add other test files if any
    # Dependencies for ScanImage class (already in main app SOURCES/HEADERS)
    # scanimage.cpp
    # scanimage.h
    # No need to add them again if they are part of the main application build
    # that the test target links against or includes source from.
    # However, if tests are built somewhat separately or if specific includes are needed:
    # INCLUDEPATH += . # Assuming testscanimage.cpp is in the root with PrintHtml.pro
    # If ScanImage relies on globals.h or other project-specific headers not in standard paths for tests:
    # INCLUDEPATH += ../includes ../common (if tests are in a subdir) or ensure they are found.

    # Ensure test can find ScanImage and other necessary classes.
    # If ScanImage and its dependencies are part of the main TARGET (PrintHtml),
    # the test executable might need to link against PrintHtml object files or include its sources.
    # For simplicity, qmake often handles this if sources are listed.
    # If `scanimage.cpp` and `scanimage.h` are part of the main `SOURCES` and `HEADERS` lists,
    # they will be compiled as part of the main application.
    # For the test target, you need to ensure these compiled objects are linked
    # or the sources are also compiled for the test target.
    # One common way is to define a variable for common sources/headers.

    # For this setup, we assume testscanimage.cpp includes "../scanimage.h"
    # and scanimage.cpp, scanimage.h are already in the main SOURCES/HEADERS list,
    # so they get compiled. The test target will then link these.
    # If ScanImage uses PCH, ensure test target also configured for it or disable for test if simpler.
    # PRECOMPILED_HEADER = stable.h (if test uses it too)

    # If main.cpp from the main application defines QCoreApplication or QApplication,
    # the test executable should not link main.cpp from the main app, as QTEST_MAIN provides its own main.
    # This is usually handled by qmake's testcase template.
}
# To build tests: qmake && make tests (or similar, depending on platform/generator)
# To run: ./testprinthtml
