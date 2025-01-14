/*
    Copyright (C) 2015 Jolla Ltd.
    Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>

    This file is part of geoclue-hybris.

    Geoclue-hybris is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License.
*/

#ifndef HYBRISPROVIDER_H
#define HYBRISPROVIDER_H

#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QBasicTimer>
#include <QtCore/QQueue>
#include <QtDBus/QDBusContext>
#include <QtNetwork/QNetworkReply>

#include "hybrislocationbackend.h"

#include "locationtypes.h"

Q_DECLARE_LOGGING_CATEGORY(lcGeoclueHybris)
Q_DECLARE_LOGGING_CATEGORY(lcGeoclueHybrisNmea)
Q_DECLARE_LOGGING_CATEGORY(lcGeoclueHybrisPosition)

QT_FORWARD_DECLARE_CLASS(QFileSystemWatcher)
QT_FORWARD_DECLARE_CLASS(QDBusServiceWatcher)
QT_FORWARD_DECLARE_CLASS(QNetworkAccessManager)
QT_FORWARD_DECLARE_CLASS(QHostAddress)
QT_FORWARD_DECLARE_CLASS(QUdpSocket)
QT_FORWARD_DECLARE_CLASS(QHostInfo)

class ComJollaConnectiondInterface;
class ComJollaLipstickConnectionSelectorIfInterface;
class MGConfItem;
class DeviceControl;
class NetworkTechnology;
class QOfonoExtModemManager;
class QOfonoConnectionManager;
class QOfonoConnectionContext;

class HybrisProvider : public QObject, public QDBusContext
{
friend class HybrisBackend;
    Q_OBJECT

public:
    explicit HybrisProvider(QObject *parent = 0);
    ~HybrisProvider();

    // org.freedesktop.Geoclue
    void AddReference();
    void RemoveReference();

    // org.freedesktop.Geoclue
    QString GetProviderInfo(QString &description);

    // Must match GeoclueStatus enum
    enum Status {
        StatusError,
        StatusUnavailable,
        StatusAcquiring,
        StatusAvailable
    };

    // org.freedesktop.Geoclue
    int GetStatus();

    // org.freedesktop.Geoclue
    void SetOptions(const QVariantMap &options);

    // Must match GeocluePositionFields enum
    enum PositionField {
        NoPositionFields = 0x00,
        LatitudePresent = 0x01,
        LongitudePresent = 0x02,
        AltitudePresent = 0x04
    };
    Q_DECLARE_FLAGS(PositionFields, PositionField)

    // org.freedesktop.Geoclue.Position
    int GetPosition(int &timestamp, double &latitude, double &longitude, double &altitude, Accuracy &accuracy);

    // Must match GeoclueVelocityFields enum
    enum VelocityField {
        NoVelocityFields = 0x00,
        SpeedPresent = 0x01,
        DirectionPresent = 0x02,
        ClimbPresent = 0x04
    };
    Q_DECLARE_FLAGS(VelocityFields, VelocityField)

    // org.freedesktop.Geoclue.Velocity
    int GetVelocity(int &timestamp, double &speed, double &direction, double &climb);

    // org.freedesktop.Geoclue.Satellite
    int GetLastSatellite(int &satelliteUsed, int &satelliteVisible, QList<int> &usedPrn, QList<SatelliteInfo> &satInfo);
    int GetSatellite(int &satelliteUsed, int &satelliteVisible, QList<int> &usedPrn, QList<SatelliteInfo> &satInfo);

signals:
    // org.freedesktop.Geoclue
    void StatusChanged(int status);

    // org.freedesktop.Geoclue.Position
    void PositionChanged(int fields, int timestamp, double latitude, double longitude, double altitude, const Accuracy &accuracy);

    // org.freedesktop.Geoclue.Velocity
    void VelocityChanged(int fields, int timestamp, double speed, double direction, double climb);

    // org.freedesktop.Geoclue.Satellite
    void SatelliteChanged(int timestamp, int satelliteUsed, int satelliteVisible, const QList<int> &usedPrn, const QList<SatelliteInfo> &satInfos);

protected:
    void timerEvent(QTimerEvent *event);

private slots:
    void setLocation(const Location &location);
    void setSatellite(const QList<SatelliteInfo> &satellites, const QList<int> &used);
    void serviceUnregistered(const QString &service);
    void locationEnabledChanged();
    void injectPosition(int fields, int timestamp, double latitude, double longitude,
                        double altitude, const Accuracy &accuracy);
    void sendNtpRequest(const QHostInfo &host);
    void handleNtpResponse();
    void xtraDownloadRequest();
    void xtraDownloadRequestSendNext();
    void xtraDownloadFinished();
    void agpsStatus(qint16 type, quint16 status, const QHostAddress &ipv4,
                    const QHostAddress &ipv6, const QByteArray &ssid, const QByteArray &password);
    void connectionErrorReported(const QString &path, const QString &error);
    void connectionSelected(bool selected);

    void setMagneticVariation(double variation);

    void engineOn();
    void engineOff();

    void stateChanged(const QString &state);

private:
    void loadDefaultsFromConfigurationFile();

    void emitLocationChanged();
    void emitSatelliteChanged();
    void startPositioningIfNeeded();
    void stopPositioningIfNeeded();
    void setStatus(Status status);
    bool positioningEnabled();
    quint32 minimumRequestedUpdateInterval() const;

    void startDataConnection();
    void stopDataConnection();

    void sendNtpRequest();

    void processConnectionContexts();
    void processNextConnectionContext();

    HybrisLocationBackend *m_backend;

    Location m_currentLocation;

    qint64 m_satelliteTimestamp;
    QList<SatelliteInfo> m_visibleSatellites;
    QList<int> m_usedPrns;

    qint64 m_previousSatelliteTimestamp;
    QList<SatelliteInfo> m_previousVisibleSatellites;
    QList<int> m_previousUsedPrns;

    QDBusServiceWatcher *m_watcher;
    struct ServiceData {
        ServiceData()
        :   referenceCount(0), updateInterval(0)
        {
        }

        int referenceCount;
        quint32 updateInterval;
    };
    QMap<QString, ServiceData> m_watchedServices;

    QBasicTimer m_idleTimer;
    QBasicTimer m_fixLostTimer;

    Status m_status;

    bool m_positionInjectionConnected;

    QNetworkAccessManager *m_manager;
    QNetworkReply *m_xtraDownloadReply;
    QQueue<QUrl> m_xtraServers;
    int m_xtraServerIndex;

    ComJollaConnectiondInterface *m_connectiond;
    ComJollaLipstickConnectionSelectorIfInterface *m_connectionSelector;

    QString m_networkServicePath;
    QString m_agpsInterface;
    QStringList m_connectionContexts;
    bool m_requestedConnect;

    bool m_gpsStarted;

    QOfonoExtModemManager *m_ofonoExtModemManager;
    QOfonoConnectionManager *m_connectionManager;
    QOfonoConnectionContext *m_connectionContext;

    QUdpSocket *m_ntpSocket;
    QBasicTimer m_ntpRetryTimer;
    QStringList m_ntpServers;
    qint64 m_ntpRequestTime;
    qint64 m_ntpRequestTicks;

    bool m_agpsEnabled;
    bool m_agpsOnlineEnabled;
    bool m_useForcedNtpInject;
    bool m_useForcedXtraInject;
    QString m_xtraUserAgent;
    double m_magneticVariation;

    QString m_suplHost;
    int m_suplPort;
};

extern HybrisProvider *staticProvider;

Q_DECLARE_OPERATORS_FOR_FLAGS(HybrisProvider::PositionFields)
Q_DECLARE_OPERATORS_FOR_FLAGS(HybrisProvider::VelocityFields)

#endif // HYBRISPROVIDER_H
