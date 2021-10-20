/*
 * This file is a part of the Voice Call Manager project
 *
 * Copyright (C) 2011-2012  Tom Swindell <t.swindell@rubyx.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#include "common.h"
#include "ngfringtoneplugin.h"

#include <voicecallmanagerinterface.h>
#include <mlite5/MGConfItem>

#include <NgfClient>

#include <QDir>
#include <QFileInfo>
#include <QtPlugin>

namespace {

const QString personalName = QStringLiteral("personal_ringtone");
const QString importantName = QStringLiteral("important_ringtone");
const QString confPath = QStringLiteral("/apps/personal-ringtones/%1");

template <typename T>
T getConfValue(const QString &key, const T &def)
{
    QVariant value = MGConfItem(confPath.arg(key)).value();
    if (value.isNull()) {
        return def;
    }
    if (!value.canConvert<T>()) {
        return def;
    }
    return value.value<T>();
}

QString getPersonalRingtone(const QString &number)
{
    QString ringtone = getConfValue(number, QString());
    if (!ringtone.isEmpty() && QFileInfo::exists(ringtone)) {
        return ringtone;
    }
    return QString();
}

} // namespace

class NgfRingtonePluginPrivate
{
    Q_DECLARE_PUBLIC(NgfRingtonePlugin)

public:
    NgfRingtonePluginPrivate(NgfRingtonePlugin *q)
        : q_ptr(q), manager(NULL), currentCall(NULL), activeCallCount(0), ngf(NULL)
    { /* ... */ }

    NgfRingtonePlugin *q_ptr;
    VoiceCallManagerInterface *manager;

    AbstractVoiceCallHandler *currentCall;
    int activeCallCount;

    Ngf::Client *ngf;
    QString lastEvent;
};

NgfRingtonePlugin::NgfRingtonePlugin(QObject *parent)
    : AbstractVoiceCallManagerPlugin(parent), d_ptr(new NgfRingtonePluginPrivate(this))
{
    TRACE

    qDebug() << Q_FUNC_INFO << "Personal ringtones plugin created";

    qsrand(QDateTime::currentMSecsSinceEpoch());
}

NgfRingtonePlugin::~NgfRingtonePlugin()
{
    TRACE

    qDebug() << Q_FUNC_INFO << "Personal ringtones plugin deleted";

    delete this->d_ptr;
}

QString NgfRingtonePlugin::pluginId() const
{
    TRACE
    return PLUGIN_NAME;
}

bool NgfRingtonePlugin::initialize()
{
    TRACE
    qDebug() << Q_FUNC_INFO << "Personal ringtones plugin initialize";
    Q_D(NgfRingtonePlugin);
    d->ngf = new Ngf::Client(this);
    return true;
}

bool NgfRingtonePlugin::configure(VoiceCallManagerInterface *manager)
{
    TRACE
    qDebug() << Q_FUNC_INFO << "Personal ringtones plugin configure";
    Q_D(NgfRingtonePlugin);
    d->manager = manager;

    QObject::connect(d->ngf, SIGNAL(connectionStatus(bool)), SLOT(onConnectionStatus(bool)));
    QObject::connect(d->ngf, SIGNAL(eventFailed(quint32)), SLOT(onEventFailed(quint32)));
    QObject::connect(d->ngf, SIGNAL(eventCompleted(quint32)), SLOT(onEventCompleted(quint32)));
    QObject::connect(d->ngf, SIGNAL(eventPlaying(quint32)), SLOT(onEventPlaying(quint32)));
    QObject::connect(d->ngf, SIGNAL(eventPaused(quint32)), SLOT(onEventPaused(quint32)));

    return true;
}

bool NgfRingtonePlugin::start()
{
    TRACE
    qDebug() << Q_FUNC_INFO << "Personal ringtones plugin start";
    Q_D(NgfRingtonePlugin);

    QObject::connect(d->manager, SIGNAL(voiceCallAdded(AbstractVoiceCallHandler*)), SLOT(onVoiceCallAdded(AbstractVoiceCallHandler*)));
    QObject::connect(d->manager, SIGNAL(silenceRingtoneRequested()), SLOT(onSilenceRingtoneRequested()));

    d->ngf->connect();
    return true;
}

bool NgfRingtonePlugin::suspend()
{
    TRACE
    qDebug() << Q_FUNC_INFO << "Personal ringtones plugin suspend";
    return true;
}

bool NgfRingtonePlugin::resume()
{
    TRACE
    qDebug() << Q_FUNC_INFO << "Personal ringtones plugin resume";
    return true;
}

void NgfRingtonePlugin::finalize()
{
    TRACE
    qDebug() << Q_FUNC_INFO << "Personal ringtones plugin finalize";
}

void NgfRingtonePlugin::onVoiceCallAdded(AbstractVoiceCallHandler *handler)
{
    TRACE
    Q_D(NgfRingtonePlugin);

    ++d->activeCallCount;
    qDebug("Active call count: %d", d->activeCallCount);

    QObject::connect(handler, SIGNAL(statusChanged(VoiceCallStatus)), SLOT(onVoiceCallStatusChanged()));
    QObject::connect(handler, SIGNAL(destroyed()), SLOT(onVoiceCallDestroyed()));
    if (handler->status() != AbstractVoiceCallHandler::STATUS_NULL)
        onVoiceCallStatusChanged(handler);
}

void NgfRingtonePlugin::onVoiceCallStatusChanged(AbstractVoiceCallHandler *handler)
{
    TRACE
    Q_D(NgfRingtonePlugin);

    if (!handler)
    {
        handler = qobject_cast<AbstractVoiceCallHandler*>(sender());
        if (!handler)
            return;
    }

    qDebug() << Q_FUNC_INFO << "Voice call status changed to:" << handler->statusText();

    if (handler->status() != AbstractVoiceCallHandler::STATUS_INCOMING)
    {
        if (d->currentCall == handler) {
            d->currentCall = NULL;

            if (!d->lastEvent.isEmpty())
            {
                qDebug("Stopping ringtone");
                d->ngf->stop(d->lastEvent);
                d->lastEvent = QString();
            }
        }
    } else if (d->lastEvent.isEmpty() && !d->currentCall) {
        d->currentCall = handler;
        QString number = handler->lineId();
        qDebug() << Q_FUNC_INFO
            << "Incoming from:"
            << number;

        QMap<QString, QVariant> props;
        if (d->activeCallCount > 1)
        {
            props.insert("play.mode", "short");
        }

        if (handler->provider()->providerType() != "tel")
        {
            props.insert("type", "voip");
        }

        int match = getConfValue(QStringLiteral("match"), 0);
        if (match > 0) {
            QString numbersItem = getConfValue(QStringLiteral("numbers"), QString());
            QStringList numbers = numbersItem.split(QChar(u';'));
            for (const QString &num : numbers) {
                if (num.right(match) == number.right(match)) {
                    number = num;
                    qDebug() << Q_FUNC_INFO
                        << "Matching number:"
                        << number;
                    break;
                }
            }
        }

        QString ringtone = getPersonalRingtone(number);
        qDebug() << Q_FUNC_INFO
            << "Ringtone for:"
            << number
            << ringtone;
        if (ringtone == QLatin1String("muted")) {
            d->lastEvent = QString();
            qDebug("Ignoring ringtone");
            return;
        } else if (ringtone.isEmpty()) {
            bool isRandom = getConfValue(QStringLiteral("random"), false);
            if (isRandom) {
                QString randomPath = getConfValue(QStringLiteral("randomPath"), QStringLiteral("/usr/share/sounds/jolla-ringtones/stereo"));
                QDir randomDir(randomPath);
                if (randomDir.exists()) {
                    QStringList ringtones = randomDir.entryList(QDir::Files, QDir::Name);
                    if (ringtones.length() > 0) {
                        ringtone = ringtones.at(qrand() % ringtones.length());
                        ringtone = randomDir.absoluteFilePath(ringtone);
                        qDebug() << Q_FUNC_INFO
                            << "Random ringtone:"
                            << ringtone;
                    }
                }
            }
        }

        if (!ringtone.isEmpty()) {
            props.insert("audio", ringtone);
        }

        d->lastEvent = personalName;

        QStringList importantNumbers = getConfValue(QStringLiteral("important"), QString()).split(QChar(u';'));
        if (match > 0) {
            for (const QString &importantNumber : importantNumbers) {
                if (importantNumber.right(match) == number.right(match)) {
                    qDebug() << Q_FUNC_INFO
                        << "Matching important number:"
                        << importantNumber;
                    d->lastEvent = importantName;
                    break;
                }
            }
        } else if (importantNumbers.contains(number)) {
            d->lastEvent = importantName;
        }
        qDebug() << props;

        quint32 eventId = d->ngf->play(d->lastEvent, props);
        qDebug("Playing ringtone, event (%s) id: %u", d->lastEvent.toLatin1().constData(), eventId);
    }
}

void NgfRingtonePlugin::onVoiceCallDestroyed()
{
    TRACE
    Q_D(NgfRingtonePlugin);

    if (d->currentCall == sender())
    {
        d->currentCall = NULL;

        if (!d->lastEvent.isEmpty())
        {
            qDebug("Stopping ringtone");
            d->ngf->stop(d->lastEvent);
            d->lastEvent = QString();
        }
    }

    --d->activeCallCount;
    qDebug("Active call count: %d", d->activeCallCount);
}

void NgfRingtonePlugin::onSilenceRingtoneRequested()
{
    TRACE
    Q_D(NgfRingtonePlugin);
    if (!d->lastEvent.isEmpty())
    {
        qDebug("Pausing ringtone due to silence");
        d->ngf->pause(d->lastEvent);
    }
}

void NgfRingtonePlugin::onConnectionStatus(bool connected)
{
    Q_UNUSED(connected)
    TRACE
    qDebug("Connection to NGF daemon changed to: %s", connected ? "connected" : "disconnected");
}

void NgfRingtonePlugin::onEventFailed(quint32 eventId)
{
    Q_UNUSED(eventId)
    TRACE
}

void NgfRingtonePlugin::onEventCompleted(quint32 eventId)
{
    Q_UNUSED(eventId)
    TRACE
}

void NgfRingtonePlugin::onEventPlaying(quint32 eventId)
{
    Q_UNUSED(eventId)
    TRACE
}

void NgfRingtonePlugin::onEventPaused(quint32 eventId)
{
    Q_UNUSED(eventId)
    TRACE
}

