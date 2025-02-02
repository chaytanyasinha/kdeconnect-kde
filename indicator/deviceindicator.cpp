/*
 * Copyright 2016 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "deviceindicator.h"
#include <QFileDialog>
#include <KLocalizedString>

#include "interfaces/dbusinterfaces.h"

#include <dbushelper.h>

class BatteryAction : public QAction
{
Q_OBJECT
public:
    BatteryAction(DeviceDbusInterface* device)
        : QAction(nullptr)
        , m_batteryIface(new DeviceBatteryDbusInterface(device->id(), this))
    {
        setWhenAvailable(m_batteryIface->charge(), [this](int charge) { setCharge(charge); }, this);
        setWhenAvailable(m_batteryIface->isCharging(), [this](bool charging) { setCharging(charging); }, this);

        connect(m_batteryIface, SIGNAL(chargeChanged(int)), this, SLOT(setCharge(int)));
        connect(m_batteryIface, SIGNAL(stateChanged(bool)), this, SLOT(setCharging(bool)));

        setIcon(QIcon::fromTheme(QStringLiteral("battery")));

        update();
    }

    void update() {
        if (m_charge < 0)
            setText(i18n("No Battery"));
        else if (m_charging)
            setText(i18n("Battery: %1% (Charging)", m_charge));
        else
            setText(i18n("Battery: %1%", m_charge));
    }

private Q_SLOTS:
    void setCharge(int charge) { m_charge = charge; update(); }
    void setCharging(bool charging) { m_charging = charging; update(); }

private:
    DeviceBatteryDbusInterface* m_batteryIface;
    int m_charge = -1;
    bool m_charging = false;
};


DeviceIndicator::DeviceIndicator(DeviceDbusInterface* device)
    : QMenu(device->name(), nullptr)
    , m_device(device)
    , m_remoteCommandsInterface(new RemoteCommandsDbusInterface(m_device->id()))
{
#ifdef Q_OS_WIN
    setIcon(QIcon(QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("icons/hicolor/scalable/status/") + device->iconName() + QStringLiteral(".svg"))));
#else
    setIcon(QIcon::fromTheme(device->iconName()));
#endif

    connect(device, SIGNAL(nameChanged(QString)), this, SLOT(setText(QString)));

    auto battery = new BatteryAction(device);
    addAction(battery);
    setWhenAvailable(device->hasPlugin(QStringLiteral("kdeconnect_battery")),
                     [battery](bool available) { battery->setVisible(available);  }
                     , this);

    auto browse = addAction(QIcon::fromTheme(QStringLiteral("document-open-folder")), i18n("Browse device"));
    connect(browse, &QAction::triggered, device, [device](){
        SftpDbusInterface* sftpIface = new SftpDbusInterface(device->id(), device);
        sftpIface->startBrowsing();
        sftpIface->deleteLater();
    });
    setWhenAvailable(device->hasPlugin(QStringLiteral("kdeconnect_sftp")), [browse](bool available) { browse->setVisible(available); }, this);

    auto findDevice = addAction(QIcon::fromTheme(QStringLiteral("irc-voice")), i18n("Ring device"));
    connect(findDevice, &QAction::triggered, device, [device](){
        FindMyPhoneDeviceDbusInterface* iface = new FindMyPhoneDeviceDbusInterface(device->id(), device);
        iface->ring();
        iface->deleteLater();
    });
    setWhenAvailable(device->hasPlugin(QStringLiteral("kdeconnect_findmyphone")), [findDevice](bool available) { findDevice->setVisible(available); }, this);

    auto sendFile = addAction(QIcon::fromTheme(QStringLiteral("document-share")), i18n("Send file"));
    connect(sendFile, &QAction::triggered, device, [device, this](){
        const QUrl url = QFileDialog::getOpenFileUrl(parentWidget(), i18n("Select file to send to '%1'", device->name()), QUrl::fromLocalFile(QDir::homePath()));
        if (url.isEmpty())
            return;

        QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kdeconnect"), QStringLiteral("/modules/kdeconnect/devices/") + device->id() + QStringLiteral("/share"), QStringLiteral("org.kde.kdeconnect.device.share"), QStringLiteral("shareUrl"));
        msg.setArguments(QVariantList() << url.toString());
        DbusHelper::sessionBus().call(msg);
    });

    setWhenAvailable(device->hasPlugin(QStringLiteral("kdeconnect_share")), [sendFile](bool available) { sendFile->setVisible(available); }, this);


    if (!QStandardPaths::findExecutable(QStringLiteral("kdeconnect-sms")).isEmpty()) {
        auto smsapp = addAction(QIcon::fromTheme(QStringLiteral("message-new")), i18n("SMS Messages..."));
        QObject::connect(smsapp, &QAction::triggered, device, [device] () {
            QProcess::startDetached(QLatin1String("kdeconnect-sms"), { QStringLiteral("--device"), device->id() });
        });
        setWhenAvailable(device->hasPlugin(QStringLiteral("kdeconnect_sms")), [smsapp](bool available) { smsapp->setVisible(available); }, this);
    }

    QMenu* remoteCommandsMenu = new QMenu(i18n("Run command"), this);
    QAction* menuAction = remoteCommandsMenu->menuAction();
    QAction* addCommandAction = remoteCommandsMenu->addAction(QIcon::fromTheme(QStringLiteral("list-add")), i18n("Add commands"));
    connect(addCommandAction, &QAction::triggered, m_remoteCommandsInterface, &RemoteCommandsDbusInterface::editCommands);

    addAction(menuAction);
    setWhenAvailable(device->hasPlugin(QStringLiteral("kdeconnect_remotecommands")), [this, remoteCommandsMenu, menuAction](bool available) {
        menuAction->setVisible(available);

        if (!available)
            return;

        const auto cmds = QJsonDocument::fromJson(m_remoteCommandsInterface->commands()).object();

        for (auto it = cmds.constBegin(), itEnd = cmds.constEnd(); it!=itEnd; ++it) {
            const QJsonObject cont = it->toObject();
            QString key = it.key();
            QAction* action = remoteCommandsMenu->addAction(cont.value(QStringLiteral("name")).toString());
            connect(action, &QAction::triggered, [this, key] {
                m_remoteCommandsInterface->triggerCommand(key);
            });
        }
    }, this);
}

#include "deviceindicator.moc"
