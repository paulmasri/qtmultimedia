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

#ifndef QABSTRACTMEDIASOURCE_H
#define QABSTRACTMEDIASOURCE_H

#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>

#include <QtMultimedia/qtmultimediaglobal.h>
#include <QtMultimedia/qmultimedia.h>

QT_BEGIN_NAMESPACE


class QMediaService;
class QMediaSink;

class QMediaSourcePrivate;
class Q_MULTIMEDIA_EXPORT QMediaSource : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int notifyInterval READ notifyInterval WRITE setNotifyInterval NOTIFY notifyIntervalChanged)
public:
    ~QMediaSource();

    virtual QMultimedia::AvailabilityStatus availability() const;
    bool isAvailable() const
    {
        return availability() == QMultimedia::Available;
    }

    virtual QMediaService* service() const;

    int notifyInterval() const;
    void setNotifyInterval(int milliSeconds);

    bool bind(QMediaSink *);
    void unbind(QMediaSink *);

    bool isMetaDataAvailable() const;

    QVariant metaData(const QString &key) const;
    QStringList availableMetaData() const;

Q_SIGNALS:
    void notifyIntervalChanged(int milliSeconds);

    void metaDataAvailableChanged(bool available);
    void metaDataChanged();
    void metaDataChanged(const QString &key, const QVariant &value);

protected:
    QMediaSource(QObject *parent, QMediaService *service);
    QMediaSource(QMediaSourcePrivate &dd, QObject *parent, QMediaService *service);

    void addPropertyWatch(QByteArray const &name);
    void removePropertyWatch(QByteArray const &name);

private:
    void setupControls();

    Q_DECLARE_PRIVATE(QMediaSource)
    Q_PRIVATE_SLOT(d_func(), void _q_notify())
};


QT_END_NAMESPACE


#endif  // QABSTRACTMEDIASOURCE_H
