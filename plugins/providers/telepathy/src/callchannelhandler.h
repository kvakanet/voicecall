/*
 * This file is a part of the Voice Call Manager project
 *
 * Copyright (C) 2011-2015  Tom Swindell <tom.swindell@jollamobile.com>
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
#ifndef CALLCHANNELHANDLER_H
#define CALLCHANNELHANDLER_H

#include "basechannelhandler.h"

#include <TelepathyQt/Channel>
#include <TelepathyQt/CallContent>

class TelepathyProvider;

class CallChannelHandler : public BaseChannelHandler
{
    Q_OBJECT

public:
    explicit CallChannelHandler(const QString &id, Tp::CallChannelPtr channel, const QDateTime &userActionTime, TelepathyProvider *provider = 0);
            ~CallChannelHandler();

    /*** AbstractVoiceCallHandler Implementation ***/
    AbstractVoiceCallProvider* provider() const;
    QString handlerId() const;
    QString lineId() const;
    QDateTime startedAt() const;
    int duration() const;
    bool isIncoming() const;
    bool isMultiparty() const;
    bool isEmergency() const;
    bool isForwarded() const;
    bool isRemoteHeld() const;

    // TODO: unimplemented
    QString parentHandlerId() const override { return QString(); }
    QList<AbstractVoiceCallHandler*> childCalls() const override { return QList<AbstractVoiceCallHandler*>(); }

    VoiceCallStatus status() const;

    /*** BaseChannelHandler Implementation ***/
    Tp::ChannelPtr channel() const override;

    // TODO: unimplemented
    void setParentHandlerId(const QString &/*handler*/) override {}

public Q_SLOTS:
    /*** AbstractVoiceCallHandler Implementation ***/
    void answer();
    void hangup();
    void hold(bool on);
    void deflect(const QString &target);
    void sendDtmf(const QString &tones);

    // TODO: unimplemented
    void merge(const QString &) {}
    void split() {}

protected Q_SLOTS:
    void onStatusChanged();

    // CallChannel Interface Handling
    void onCallChannelChannelReady(Tp::PendingOperation *op);
    void onCallChannelChannelInvalidated(Tp::DBusProxy*,const QString &errorName, const QString &errorMessage);

    void onCallChannelCallStateChanged(Tp::CallState state);

    void onCallChannelCallContentAdded(Tp::CallContentPtr content);
    void onCallChannelCallContentRemoved(Tp::CallContentPtr content, Tp::CallStateReason reason);

    void onCallChannelAcceptCallFinished(Tp::PendingOperation *op);
    void onCallChannelHangupCallFinished(Tp::PendingOperation *op);

    // Telepathy Farstream Interface Handling
    void onFarstreamCreateChannelFinished(Tp::PendingOperation *op);

protected:
    void timerEvent(QTimerEvent *event);

    // TODO: unimplemented
    void addChildCall(BaseChannelHandler */*handler*/) override {}
    void removeChildCall(BaseChannelHandler */*handler*/) override {}

private:
    void setStatus(VoiceCallStatus newStatus);

    class CallChannelHandlerPrivate *d_ptr;

    Q_DISABLE_COPY(CallChannelHandler)
    Q_DECLARE_PRIVATE(CallChannelHandler)
};

#endif // CALLCHANNELHANDLER_H
