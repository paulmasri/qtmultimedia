/****************************************************************************
**
** Copyright (C) 2014 Pelagicore AG
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "qsgvivantevideonode.h"
#include "qsgvivantevideomaterialshader.h"
#include "qsgvivantevideomaterial.h"

QMap<QVideoFrame::PixelFormat, GLenum> QSGVivanteVideoNode::static_VideoFormat2GLFormatMap = QMap<QVideoFrame::PixelFormat, GLenum>();

QSGVivanteVideoNode::QSGVivanteVideoNode(const QVideoSurfaceFormat &format) :
    mFormat(format)
{
    setFlag(QSGNode::OwnsMaterial, true);
    mMaterial = new QSGVivanteVideoMaterial();
    setMaterial(mMaterial);
}

QSGVivanteVideoNode::~QSGVivanteVideoNode()
{
}

void QSGVivanteVideoNode::setCurrentFrame(const QVideoFrame &frame)
{
    mMaterial->setCurrentFrame(frame);
    markDirty(DirtyMaterial);
}

const QMap<QVideoFrame::PixelFormat, GLenum>& QSGVivanteVideoNode::getVideoFormat2GLFormatMap()
{
    if (static_VideoFormat2GLFormatMap.isEmpty()) {
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_YV12,     GL_VIV_YV12);
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_NV12,     GL_VIV_NV12);


        // The following formats should work but are untested!
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_NV21,     GL_VIV_NV21);
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_UYVY,     GL_VIV_UYVY);
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_YUYV,     GL_VIV_YUY2);
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_RGB32,    GL_RGBA);
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_RGB24,    GL_RGB);
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_RGB565,   GL_RGB565);
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_BGRA32,   GL_BGRA_EXT);
    }

    return static_VideoFormat2GLFormatMap;
}



