QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    GelAnalyzer.cpp \
    main.cpp \
    LaneGraphicsView.cpp \
    ProfileDialog.cpp \
    ProfilePlot.cpp \
    mainwindow.cpp

HEADERS += \
    GelAnalyzer.h \
    LaneGraphicsView.h \
    ProfileDialog.h \
    ProfilePlot.h \
    mainwindow.h


INCLUDEPATH += C:/opencv/build/include

CONFIG(debug, debug|release) {
    LIBS += -LC:/opencv/build/x64/vc16/lib \
            -lopencv_world490d
} else {
    LIBS += -LC:/opencv/build/x64/vc16/lib \
            -lopencv_world490
}

RC_ICONS = app.ico

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
