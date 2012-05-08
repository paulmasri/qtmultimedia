TEMPLATE = lib

TARGET = qgsttools_p
QPRO_PWD = $$PWD
QT = core multimedia-private gui-private

!static:DEFINES += QT_MAKEDLL

unix:!maemo*:contains(QT_CONFIG, alsa) {
DEFINES += HAVE_ALSA
LIBS += \
    -lasound
}

CONFIG += link_pkgconfig

PKGCONFIG += \
    gstreamer-0.10 \
    gstreamer-base-0.10 \
    gstreamer-interfaces-0.10 \
    gstreamer-audio-0.10 \
    gstreamer-video-0.10 \
    gstreamer-pbutils-0.10

maemo*:PKGCONFIG +=gstreamer-plugins-bad-0.10

contains(config_test_resourcepolicy, yes) {
    DEFINES += HAVE_RESOURCE_POLICY
    PKGCONFIG += libresourceqt1
}

# Header files must go inside source directory of a module
# to be installed by syncqt.
INCLUDEPATH += ../multimedia/gsttools_headers/
DEPENDPATH += ../multimedia/gsttools_headers/

PRIVATE_HEADERS += \
    qgstbufferpoolinterface_p.h \
    qgstreamerbushelper_p.h \
    qgstreamermessage_p.h \
    qgstutils_p.h \
    qgstvideobuffer_p.h \
    qvideosurfacegstsink_p.h \
    qgstreamervideorendererinterface_p.h \
    qgstreameraudioinputendpointselector_p.h \
    qgstreamervideorenderer_p.h \
    qgstreamervideoinputdevicecontrol_p.h \
    gstvideoconnector_p.h \
    qgstcodecsinfo_p.h \
    qgstreamervideoprobecontrol_p.h \
    qgstreameraudioprobecontrol_p.h \

SOURCES += \
    qgstbufferpoolinterface.cpp \
    qgstreamerbushelper.cpp \
    qgstreamermessage.cpp \
    qgstutils.cpp \
    qgstvideobuffer.cpp \
    qvideosurfacegstsink.cpp \
    qgstreamervideorendererinterface.cpp \
    qgstreameraudioinputendpointselector.cpp \
    qgstreamervideorenderer.cpp \
    qgstreamervideoinputdevicecontrol.cpp \
    qgstcodecsinfo.cpp \
    gstvideoconnector.c \
    qgstreamervideoprobecontrol.cpp \
    qgstreameraudioprobecontrol.cpp \

contains(config_test_xvideo, yes) {
    DEFINES += HAVE_XVIDEO

    LIBS += -lXv -lX11 -lXext

    PRIVATE_HEADERS += \
        qgstxvimagebuffer_p.h \

    SOURCES += \
        qgstxvimagebuffer.cpp \

    !isEmpty(QT.widgets.name) {
        QT += multimediawidgets

        PRIVATE_HEADERS += \
        qgstreamervideooverlay_p.h \
        qgstreamervideowindow_p.h \
        qgstreamervideowidget_p.h \
        qx11videosurface_p.h \

        SOURCES += \
        qgstreamervideooverlay.cpp \
        qgstreamervideowindow.cpp \
        qgstreamervideowidget.cpp \
        qx11videosurface.cpp \
    }
}

maemo6 {
    PKGCONFIG += qmsystem2

    contains(QT_CONFIG, opengles2):!isEmpty(QT.widgets.name) {
        PRIVATE_HEADERS += qgstreamergltexturerenderer_p.h
        SOURCES += qgstreamergltexturerenderer.cpp
        QT += opengl
        LIBS += -lEGL -lgstmeegointerfaces-0.10
    }
}

contains(config_test_gstreamer_appsrc, yes) {
    PKGCONFIG += gstreamer-app-0.10
    PRIVATE_HEADERS += qgstappsrc_p.h
    SOURCES += qgstappsrc.cpp

    DEFINES += HAVE_GST_APPSRC

    LIBS += -lgstapp-0.10
}

HEADERS += $$PRIVATE_HEADERS

DESTDIR = $$QT.multimedia.libs
target.path = $$[QT_INSTALL_LIBS]

INSTALLS += target
