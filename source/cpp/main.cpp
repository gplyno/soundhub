#include "mainwindow.h"

#include <QApplication>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("SoundHub"));
    app.setApplicationDisplayName(QStringLiteral("SoundHub"));
    app.setOrganizationName(QStringLiteral("gplyno"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icon.png")));

#ifdef Q_OS_WIN
    app.setQuitOnLastWindowClosed(false);

    const QString serverName = QStringLiteral("SoundHub_Local_Player_Socket_99");

    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (socket.waitForConnected(300)) {
        // A stale pipe may survive a crashed instance, so verify the peer is alive.
        socket.write("PING");
        socket.flush();
        if (socket.waitForReadyRead(400) && socket.readAll().trimmed() == "PONG") {
            socket.write("SHOW");
            socket.waitForBytesWritten(300);
            return 0;
        }
        socket.abort();
    }
#endif

    MainWindow window;

#ifdef Q_OS_WIN
    QLocalServer server;
    server.removeServer(serverName);
    server.listen(serverName);
    QObject::connect(&server, &QLocalServer::newConnection, &app, [&server, &window] {
        while (QLocalSocket *client = server.nextPendingConnection()) {
            QObject::connect(client, &QLocalSocket::readyRead, client, [client, &window] {
                const QByteArray message = client->readAll().trimmed();
                if (message == "PING") {
                    client->write("PONG");
                    client->flush();
                } else if (message == "SHOW") {
                    window.showFromTray();
                }
            });
            QObject::connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
        }
    });
#endif

    window.show();
    return app.exec();
}
