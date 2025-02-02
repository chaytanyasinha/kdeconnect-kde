/**
 * Copyright 2019 Weixuan XIAO <veyx.shaw@gmail.com>
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

#include "dbushelper.h"

#include <QtTest>
#include <QDBusMessage>
#include <QDBusConnectionInterface>

/**
 * This class tests the working of private dbus in kdeconnect
 */
class PrivateDBusTest : public QObject
{
    Q_OBJECT

public:
    PrivateDBusTest()
    {
        DbusHelper::launchDBusDaemon();
    }

    ~PrivateDBusTest()
    {
        DbusHelper::closeDBusDaemon();
    }

private Q_SLOTS:
    void testConnectionWithPrivateDBus();
    void testServiceRegistrationWithPrivateDBus();
    void testMethodCallWithPrivateDBus();
};

/**
 * Open private DBus normally and get connection info
 */
void PrivateDBusTest::testConnectionWithPrivateDBus()
{
    QDBusConnection conn = DbusHelper::sessionBus();

    QVERIFY2(conn.isConnected(), "Connection not established");
    QVERIFY2(conn.name() == QStringLiteral(KDECONNECT_PRIVATE_DBUS_NAME), 
            "DBus Connection is not the right one");
}

/**
 * Open private DBus connection normally and register a service
 */
void PrivateDBusTest::testServiceRegistrationWithPrivateDBus()
{
    QDBusConnection conn = DbusHelper::sessionBus();
    QVERIFY2(conn.isConnected(), "DBus not connected");

    QDBusConnectionInterface *bus = conn.interface();
    QVERIFY2(bus != nullptr, "Failed to get DBus interface");

    QVERIFY2(bus->registerService(QStringLiteral("privatedbus.test")) == QDBusConnectionInterface::ServiceRegistered,
        "Failed to register DBus Serice");

    bus->unregisterService(QStringLiteral("privatedbus.test"));
}

/**
 * Open private DBus connection normally, call a method and get its reply
 */
void PrivateDBusTest::testMethodCallWithPrivateDBus()
{
    QDBusConnection conn = DbusHelper::sessionBus();
    QVERIFY2(conn.isConnected(), "DBus not connected");

    /*
    dbus-send --session           \
        --dest=org.freedesktop.DBus \
        --type=method_call          \
        --print-reply               \
        /org/freedesktop/DBus       \
        org.freedesktop.DBus.ListNames
    */
    QDBusMessage msg = conn.call(
        QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.DBus"),     // Service 
            QStringLiteral("/org/freedesktop/DBus"),    // Path
            QStringLiteral("org.freedesktop.DBus"),     // Interface
            QStringLiteral("ListNames")                 // Method
        )
    );

    QVERIFY2(msg.type() == QDBusMessage::ReplyMessage, "Failed calling method on private DBus");
}

QTEST_MAIN(PrivateDBusTest);
#include "testprivatedbus.moc"
