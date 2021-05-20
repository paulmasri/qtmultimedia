/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qcamerainfo.h>

#include "qgstreamercamera_p.h"
#include "qgstreamercameraimagecapture_p.h"
#include <private/qgstreamermediadevices_p.h>
#include <private/qgstreamerintegration_p.h>
#include <qmediacapturesession.h>

#if QT_CONFIG(linux_v4l)
#include <linux/videodev2.h>
#include <private/qcore_unix_p.h>
#endif

#include <QtCore/qdebug.h>

QGstreamerCamera::QGstreamerCamera(QCamera *camera)
    : QPlatformCamera(camera),
      gstCameraBin("camerabin")
{
    gstCamera = QGstElement("videotestsrc");
    gstDecode = QGstElement("identity");
    gstVideoConvert = QGstElement("videoconvert", "videoConvert");
    gstVideoScale = QGstElement("videoscale", "videoScale");
    gstCameraBin.add(gstCamera, gstDecode, gstVideoConvert, gstVideoScale);
    gstCamera.link(gstDecode, gstVideoConvert, gstVideoScale);

    gstCameraBin.addGhostPad(gstVideoScale, "src");
}

QGstreamerCamera::~QGstreamerCamera()
{
#if QT_CONFIG(linux_v4l)
    if (v4l2FileDescriptor >= 0)
        qt_safe_close(v4l2FileDescriptor);
    v4l2FileDescriptor = -1;
#endif
}

bool QGstreamerCamera::isActive() const
{
    return m_active;
}

void QGstreamerCamera::setActive(bool active)
{
    if (m_active == active)
        return;
    if (m_cameraInfo.isNull() && active)
        return;

    m_active = active;

    statusChanged(m_active ? QCamera::ActiveStatus : QCamera::InactiveStatus);
    emit activeChanged(active);
}

void QGstreamerCamera::setCamera(const QCameraInfo &camera)
{
    if (m_cameraInfo == camera)
        return;
//    qDebug() << "setCamera" << camera;

    m_cameraInfo = camera;

    bool havePipeline = !gstPipeline.isNull();

    auto state = havePipeline ? gstPipeline.state() : GST_STATE_NULL;
    if (havePipeline)
        gstPipeline.setStateSync(GST_STATE_PAUSED);

    Q_ASSERT(!gstCamera.isNull());

    gstCamera.setStateSync(GST_STATE_NULL);
    gstCameraBin.remove(gstCamera);

    if (camera.isNull()) {
        gstCamera = QGstElement("videotestsrc");
    } else {
        auto *devices = static_cast<QGstreamerMediaDevices *>(QGstreamerIntegration::instance()->devices());
        auto *device = devices->videoDevice(camera.id());
        gstCamera = gst_device_create_element(device, "camerasrc");
        QGstStructure properties = gst_device_get_properties(device);
        if (properties.name() == "v4l2deviceprovider")
            m_v4l2Device = QString::fromUtf8(properties["device.path"].toString());
    }

    gstCameraBin.add(gstCamera);
    // set the camera up with a decent format
    setCameraFormatInternal({});

    gstCamera.setStateSync(state == GST_STATE_PLAYING ? GST_STATE_PAUSED : state);

    if (havePipeline) {
        gstPipeline.dumpGraph("setCamera");
        gstPipeline.setStateSync(state);
    }

    updateCameraProperties();
}

void QGstreamerCamera::setCameraFormatInternal(const QCameraFormat &format)
{
    QCameraFormat f = format;
    if (f.isNull())
        f = findBestCameraFormat(m_cameraInfo);

    // add jpeg decoder where required
    gstDecode.setStateSync(GST_STATE_NULL);
    gstCameraBin.remove(gstDecode);

    if (f.pixelFormat() == QVideoFrameFormat::Format_Jpeg) {
//        qDebug() << "    enabling jpeg decoder";
        gstDecode = QGstElement("jpegdec");
    } else {
//        qDebug() << "    camera delivers raw video";
        gstDecode = QGstElement("identity");
    }
    gstCameraBin.add(gstDecode);
    gstDecode.link(gstVideoConvert);

    auto caps = QGstMutableCaps::fromCameraFormat(f);
    if (!caps.isNull()) {
        if (!gstCamera.linkFiltered(gstDecode, caps))
            qWarning() << "linking filtered camera to decoder failed" << gstCamera.name() << gstDecode.name() << caps.toString();
    } else {
        if (!gstCamera.link(gstDecode))
            qWarning() << "linking camera to decoder failed" << gstCamera.name() << gstDecode.name();
    }
}

bool QGstreamerCamera::setCameraFormat(const QCameraFormat &format)
{
    if (!m_cameraInfo.videoFormats().contains(format))
        return false;
    bool havePipeline = !gstPipeline.isNull();
    auto state = havePipeline ? gstPipeline.state() : GST_STATE_NULL;
    if (havePipeline)
        gstPipeline.setStateSync(GST_STATE_PAUSED);
    setCameraFormatInternal(format);
    if (havePipeline)
        gstPipeline.setStateSync(state);
    return true;
}

void QGstreamerCamera::setCaptureSession(QPlatformMediaCaptureSession *session)
{
    QGstreamerMediaCapture *captureSession = static_cast<QGstreamerMediaCapture *>(session);
    if (m_session == captureSession)
        return;

    m_session = captureSession;
    // is this enough?
}

void QGstreamerCamera::updateCameraProperties()
{
#if QT_CONFIG(linux_v4l)
    if (isV4L2Camera())
        initV4L2Controls();
#endif
#if QT_CONFIG(gstreamer_photography)
    if (auto *p = photography())
        gst_photography_set_white_balance_mode(p, GST_PHOTOGRAPHY_WB_MODE_AUTO);
#endif

}

#if QT_CONFIG(gstreamer_photography)
GstPhotography *QGstreamerCamera::photography() const
{
    if (!gstCamera.isNull() && GST_IS_PHOTOGRAPHY(gstCamera.element()))
        return GST_PHOTOGRAPHY(gstCamera.element());
    return nullptr;
}

void QGstreamerCamera::setFocusMode(QCamera::FocusMode mode)
{
    if (mode == focusMode())
        return;

    auto p = photography();
    if (p) {
        GstPhotographyFocusMode photographyMode = GST_PHOTOGRAPHY_FOCUS_MODE_CONTINUOUS_NORMAL;

        switch (mode) {
        case QCamera::FocusModeAutoNear:
            photographyMode = GST_PHOTOGRAPHY_FOCUS_MODE_MACRO;
            break;
        case QCamera::FocusModeAutoFar:
            // not quite, but hey :)
            Q_FALLTHROUGH();
        case QCamera::FocusModeHyperfocal:
            photographyMode = GST_PHOTOGRAPHY_FOCUS_MODE_HYPERFOCAL;
            break;
        case QCamera::FocusModeInfinity:
            photographyMode = GST_PHOTOGRAPHY_FOCUS_MODE_INFINITY;
            break;
        case QCamera::FocusModeManual:
            photographyMode = GST_PHOTOGRAPHY_FOCUS_MODE_MANUAL;
            break;
        default: // QCamera::FocusModeAuto:
            break;
        }

        if (gst_photography_set_focus_mode(p, photographyMode))
            focusModeChanged(mode);
    }
}

bool QGstreamerCamera::isFocusModeSupported(QCamera::FocusMode mode) const
{
    if (photography())
        return true;
    return mode == QCamera::FocusModeAuto;
}

void QGstreamerCamera::setFlashMode(QCamera::FlashMode mode)
{
    Q_UNUSED(mode);

    if (auto *p = photography()) {
        GstPhotographyFlashMode flashMode;
        gst_photography_get_flash_mode(p, &flashMode);

        switch (mode) {
        case QCamera::FlashAuto:
            flashMode = GST_PHOTOGRAPHY_FLASH_MODE_AUTO;
            break;
        case QCamera::FlashOff:
            flashMode = GST_PHOTOGRAPHY_FLASH_MODE_OFF;
            break;
        case QCamera::FlashOn:
            flashMode = GST_PHOTOGRAPHY_FLASH_MODE_ON;
            break;
        }

        if (gst_photography_set_flash_mode(p, flashMode))
            flashModeChanged(mode);
    }
}

bool QGstreamerCamera::isFlashModeSupported(QCamera::FlashMode mode) const
{
    if (photography())
        return true;

    return mode == QCamera::FlashAuto;
}

bool QGstreamerCamera::isFlashReady() const
{
    if (photography())
        return true;

    return false;
}

void QGstreamerCamera::setExposureMode(QCamera::ExposureMode mode)
{
#if QT_CONFIG(linux_v4l)
    if (isV4L2Camera() && v4l2AutoExposureSupported && v4l2ManualExposureSupported) {
        if (mode != QCamera::ExposureAuto && mode != QCamera::ExposureManual)
            return;
        int value = QCamera::ExposureAuto ? V4L2_EXPOSURE_AUTO : V4L2_EXPOSURE_MANUAL;
        setV4L2Parameter(V4L2_CID_EXPOSURE_AUTO, value);
        exposureModeChanged(mode);
        return;
    }
#endif

    auto *p = photography();
    if (!p)
        return;

    GstPhotographySceneMode sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_AUTO;

    switch (mode) {
    case QCamera::ExposureManual:
        sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_MANUAL;
        break;
    case QCamera::ExposurePortrait:
        sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_PORTRAIT;
        break;
    case QCamera::ExposureSports:
        sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_SPORT;
        break;
    case QCamera::ExposureNight:
        sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_NIGHT;
        break;
    case QCamera::ExposureAuto:
        sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_AUTO;
        break;
    case QCamera::ExposureLandscape:
        sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_LANDSCAPE;
        break;
    case QCamera::ExposureSnow:
        sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_SNOW;
        break;
    case QCamera::ExposureBeach:
        sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_BEACH;
        break;
    case QCamera::ExposureAction:
        sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_ACTION;
        break;
    case QCamera::ExposureNightPortrait:
        sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_NIGHT_PORTRAIT;
        break;
    case QCamera::ExposureTheatre:
        sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_THEATRE;
        break;
    case QCamera::ExposureSunset:
        sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_SUNSET;
        break;
    case QCamera::ExposureSteadyPhoto:
        sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_STEADY_PHOTO;
        break;
    case QCamera::ExposureFireworks:
        sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_FIREWORKS;
        break;
    case QCamera::ExposureParty:
        sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_PARTY;
        break;
    case QCamera::ExposureCandlelight:
        sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_CANDLELIGHT;
        break;
    case QCamera::ExposureBarcode:
        sceneMode = GST_PHOTOGRAPHY_SCENE_MODE_BARCODE;
        break;
    default:
        return;
    }

    if (gst_photography_set_scene_mode(p, sceneMode))
        exposureModeChanged(mode);
}

bool QGstreamerCamera::isExposureModeSupported(QCamera::ExposureMode mode) const
{
    if (mode == QCamera::ExposureAuto)
        return true;
#if QT_CONFIG(linux_v4l)
    if (isV4L2Camera() && v4l2ManualExposureSupported && v4l2AutoExposureSupported)
        return mode == QCamera::ExposureManual;
#endif
    if (photography())
        return true;

    return false;
}

void QGstreamerCamera::setExposureCompensation(float compensation)
{
#if QT_CONFIG(linux_v4l)
    if (isV4L2Camera() && (v4l2MinExposureAdjustment != 0 || v4l2MaxExposureAdjustment != 0)) {
        int value = qBound(v4l2MinExposureAdjustment, (int)(compensation*1000), v4l2MaxExposureAdjustment);
        setV4L2Parameter(V4L2_CID_AUTO_EXPOSURE_BIAS, value);
        exposureCompensationChanged(value/1000.);
        return;
    }
#endif

    if (auto *p = photography()) {
        if (gst_photography_set_ev_compensation(p, compensation))
            exposureCompensationChanged(compensation);
    }
}

void QGstreamerCamera::setManualIsoSensitivity(int iso)
{
    if (auto *p = photography()) {
        if (gst_photography_set_iso_speed(p, iso))
            isoSensitivityChanged(iso);
    }
}

int QGstreamerCamera::isoSensitivity() const
{
    if (auto *p = photography()) {
        guint speed = 0;
        if (gst_photography_get_iso_speed(p, &speed))
            return speed;
    }
    return 100;
}

void QGstreamerCamera::setManualShutterSpeed(float secs)
{
#if QT_CONFIG(linux_v4l)
    if (isV4L2Camera() && v4l2ManualExposureSupported && v4l2AutoExposureSupported) {
        int exposure = qBound(v4l2MinExposure, qRound(secs*10000.), v4l2MaxExposure);
        setV4L2Parameter(V4L2_CID_EXPOSURE_ABSOLUTE, exposure);
        shutterSpeedChanged(exposure/10000.);
        return;
    }
#endif

    if (auto *p = photography()) {
        if (gst_photography_set_exposure(p, guint(secs*1000000)))
            shutterSpeedChanged(secs);
    }
}

float QGstreamerCamera::shutterSpeed() const
{
    if (auto *p = photography()) {
        guint32 exposure = 0;
        if (gst_photography_get_exposure(p, &exposure))
            return exposure/1000000.;
    }
    return -1;
}

bool QGstreamerCamera::isWhiteBalanceModeSupported(QCamera::WhiteBalanceMode mode) const
{
    if (mode == QCamera::WhiteBalanceAuto)
        return true;

#if QT_CONFIG(linux_v4l)
    if (isV4L2Camera()) {
        if (v4l2AutoWhiteBalanceSupported && v4l2ColorTemperatureSupported)
            return true;
    }
#endif
#if QT_CONFIG(gstreamer_photography)
    if (photography()) {
        switch (mode) {
        case QCamera::WhiteBalanceAuto:
        case QCamera::WhiteBalanceSunlight:
        case QCamera::WhiteBalanceCloudy:
        case QCamera::WhiteBalanceShade:
        case QCamera::WhiteBalanceSunset:
        case QCamera::WhiteBalanceTungsten:
        case QCamera::WhiteBalanceFluorescent:
            return true;
        case QCamera::WhiteBalanceManual: {
#if GST_CHECK_VERSION(1, 18, 0)
            GstPhotographyInterface *iface = GST_PHOTOGRAPHY_GET_INTERFACE(p);
            if (iface->set_color_temperature && iface->get_color_temperature)
                return true;
#endif
            break;
        }
        default:
            break;
        }
    }
#endif

    return mode == QCamera::WhiteBalanceAuto;
}

void QGstreamerCamera::setWhiteBalanceMode(QCamera::WhiteBalanceMode mode)
{
    Q_ASSERT(isWhiteBalanceModeSupported(mode));

#if QT_CONFIG(gstreamer_photography)
    if (auto *p = photography()) {
        GstPhotographyWhiteBalanceMode gstMode = GST_PHOTOGRAPHY_WB_MODE_AUTO;
        switch (mode) {
        case QCamera::WhiteBalanceSunlight:
            gstMode = GST_PHOTOGRAPHY_WB_MODE_DAYLIGHT;
            break;
        case QCamera::WhiteBalanceCloudy:
            gstMode = GST_PHOTOGRAPHY_WB_MODE_CLOUDY;
            break;
        case QCamera::WhiteBalanceShade:
            gstMode = GST_PHOTOGRAPHY_WB_MODE_SHADE;
            break;
        case QCamera::WhiteBalanceSunset:
            gstMode = GST_PHOTOGRAPHY_WB_MODE_SUNSET;
            break;
        case QCamera::WhiteBalanceTungsten:
            gstMode = GST_PHOTOGRAPHY_WB_MODE_TUNGSTEN;
            break;
        case QCamera::WhiteBalanceFluorescent:
            gstMode = GST_PHOTOGRAPHY_WB_MODE_FLUORESCENT;
            break;
        case QCamera::WhiteBalanceAuto:
        default:
            break;
        }
        if (gst_photography_set_white_balance_mode(p, gstMode)) {
            whiteBalanceModeChanged(mode);
            return;
        }
    } else
#endif

    if (isV4L2Camera()) {
        int temperature = colorTemperatureForWhiteBalance(mode);
        int t = setV4L2ColorTemperature(temperature);
        if (t == 0)
            mode = QCamera::WhiteBalanceAuto;
        whiteBalanceModeChanged(mode);
    }
}

void QGstreamerCamera::setColorTemperature(int temperature)
{
    if (temperature == 0) {
        setWhiteBalanceMode(QCamera::WhiteBalanceAuto);
        return;
    }

    Q_ASSERT(isWhiteBalanceModeSupported(QCamera::WhiteBalanceManual));

#if QT_CONFIG(gstreamer_photography) && GST_CHECK_VERSION(1, 18, 0)
    if (auto *p = photography()) {
        GstPhotographyInterface *iface = GST_PHOTOGRAPHY_GET_INTERFACE(p);
        Q_ASSERT(iface->set_color_temperature);
        iface->set_color_temperature(p, temperature);
        return;
    }
#endif

    if (isV4L2Camera()) {
        int t = setV4L2ColorTemperature(temperature);
        if (t)
            colorTemperatureChanged(t);
    }
}

#if QT_CONFIG(linux_v4l)
void QGstreamerCamera::initV4L2Controls()
{
    v4l2AutoWhiteBalanceSupported = false;
    v4l2ColorTemperatureSupported = false;

    const QString deviceName = v4l2Device();
    Q_ASSERT(!deviceName.isEmpty());

    v4l2FileDescriptor = qt_safe_open(deviceName.toLocal8Bit().constData(), O_RDONLY);
    if (v4l2FileDescriptor == -1) {
        qWarning() << "Unable to open the camera" << deviceName
                   << "for read to query the parameter info:" << qt_error_string(errno);
        return;
    }

    struct v4l2_queryctrl queryControl;
    ::memset(&queryControl, 0, sizeof(queryControl));
    queryControl.id = V4L2_CID_AUTO_WHITE_BALANCE;

    if (::ioctl(v4l2FileDescriptor, VIDIOC_QUERYCTRL, &queryControl) == 0) {
        v4l2AutoWhiteBalanceSupported = true;
        struct v4l2_control control;
        control.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE;
        control.value = true;
        ::ioctl(v4l2FileDescriptor, VIDIOC_S_CTRL, &control);
    }

    ::memset(&queryControl, 0, sizeof(queryControl));
    queryControl.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE;
    if (::ioctl(v4l2FileDescriptor, VIDIOC_QUERYCTRL, &queryControl) == 0) {
        v4l2MinColorTemp = queryControl.minimum;
        v4l2MaxColorTemp = queryControl.maximum;
        v4l2ColorTemperatureSupported = true;
    }

    ::memset(&queryControl, 0, sizeof(queryControl));
    queryControl.id = V4L2_CID_EXPOSURE_AUTO;
    if (::ioctl(v4l2FileDescriptor, VIDIOC_QUERYCTRL, &queryControl) == 0) {
        v4l2AutoExposureSupported = true;
    }

    ::memset(&queryControl, 0, sizeof(queryControl));
    queryControl.id = V4L2_CID_EXPOSURE_ABSOLUTE;
    if (::ioctl(v4l2FileDescriptor, VIDIOC_QUERYCTRL, &queryControl) == 0) {
        v4l2ManualExposureSupported = true;
        v4l2MinExposure = queryControl.minimum;
        v4l2MaxExposure = queryControl.maximum;
    }

    ::memset(&queryControl, 0, sizeof(queryControl));
    queryControl.id = V4L2_CID_AUTO_EXPOSURE_BIAS;
    if (::ioctl(v4l2FileDescriptor, VIDIOC_QUERYCTRL, &queryControl) == 0) {
        v4l2MinExposureAdjustment = queryControl.minimum;
        v4l2MaxExposureAdjustment = queryControl.maximum;
    }
}

int QGstreamerCamera::setV4L2ColorTemperature(int temperature)
{
    struct v4l2_control control;
    ::memset(&control, 0, sizeof(control));

    if (v4l2AutoWhiteBalanceSupported) {
        setV4L2Parameter(V4L2_CID_AUTO_WHITE_BALANCE, temperature == 0 ? true : false);
    } else if (temperature == 0) {
        temperature = 5600;
    }

    if (temperature != 0 && v4l2ColorTemperatureSupported) {
        temperature = qBound(v4l2MinColorTemp, temperature, v4l2MaxColorTemp);
        if (!setV4L2Parameter(V4L2_CID_WHITE_BALANCE_TEMPERATURE, qBound(v4l2MinColorTemp, temperature, v4l2MaxColorTemp)))
            temperature = 0;
    } else {
        temperature = 0;
    }

    return temperature;
}

bool QGstreamerCamera::setV4L2Parameter(quint32 id, qint32 value)
{
    struct v4l2_control control;
    ::memset(&control, 0, sizeof(control));
    control.id = id;
    control.value = value;
    if (::ioctl(v4l2FileDescriptor, VIDIOC_S_CTRL, &control) != 0) {
        qWarning() << "Unable to set the V4L2 Parameter" << Qt::hex << id << "to" << value << qt_error_string(errno);
        return false;
    }
    return true;
}

#endif

#endif
