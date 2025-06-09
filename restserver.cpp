#include "restserver.h"
#include <QUrl>
#include <QTimer>

RestServer::RestServer(QObject *parent)
    : QObject(parent)
{
    connect(&server, SIGNAL(newConnection()), this, SLOT(newConnection()));
}

bool RestServer::listen(quint16 port)
{
    return server.listen(QHostAddress::Any, port);
}

void RestServer::newConnection()
{
    QTcpSocket *client = server.nextPendingConnection();
    connect(client, SIGNAL(readyRead()), this, SLOT(readClient()));
    connect(client, SIGNAL(disconnected()), client, SLOT(deleteLater()));
}

static QMap<QString, QString> parseQuery(const QString &query)
{
    QMap<QString, QString> params;
    foreach (const QString &pair, query.split('&', QString::SkipEmptyParts)) {
        int eq = pair.indexOf('=');
        if (eq > 0) {
            QString key = QUrl::fromPercentEncoding(pair.left(eq).toUtf8());
            QString val = QUrl::fromPercentEncoding(pair.mid(eq+1).toUtf8());
            params.insert(key, val);
        }
    }
    return params;
}

void RestServer::readClient()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client)
        return;
    if (!client->canReadLine())
        return;

    QString requestLine = QString::fromUtf8(client->readLine()).trimmed();
    if (!requestLine.startsWith("GET")) {
        client->write("HTTP/1.1 400 Bad Request\r\n\r\n");
        client->disconnectFromHost();
        return;
    }

    QString path = requestLine.section(' ', 1, 1);
    QString endpoint = path.section('?', 0, 0);
    QString query = path.section('?', 1);
    QMap<QString, QString> params = parseQuery(query);

    if (endpoint == "/print" && params.contains("url")) {
        QStringList urls; urls << params.value("url");
        QString printer = params.value("printer", "Default");
        double l = params.value("left", "0.5").toDouble();
        double t = params.value("top", "0.5").toDouble();
        double r = params.value("right", "0.5").toDouble();
        double b = params.value("bottom", "0.5").toDouble();
        QString paper = params.value("paper", "A4");
        QString orient = params.value("orientation", "portrait");
        int pageFrom = params.value("pagefrom", "0").toInt();
        int pageTo = params.value("pageto", "0").toInt();
        double width = params.value("width", "0").toDouble();
        double height = params.value("height", "0").toDouble();

        PrintHtml *job = new PrintHtml(false, true, urls, printer, l, t, r, b, paper, orient, pageFrom, pageTo, width, height);
        connect(job, SIGNAL(finished()), job, SLOT(deleteLater()));
        QTimer::singleShot(0, job, SLOT(run()));

        QByteArray resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nPrinting started";
        client->write(resp);
        client->disconnectFromHost();
    } else {
        client->write("HTTP/1.1 404 Not Found\r\n\r\n");
        client->disconnectFromHost();
    }
}
