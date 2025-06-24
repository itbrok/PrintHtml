#include "restserver.h"
#include <QUrl>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QFile>

RestServer::RestServer(QObject *parent)
    : QObject(parent)
{
    connect(&server, SIGNAL(newConnection()), this, SLOT(newConnection()));
    networkManager = new QNetworkAccessManager(this);
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

void RestServer::uploadImage(const QString &imagePath, const QString &uploadUrl, QTcpSocket *client)
{
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart imagePart;
    imagePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"image\"; filename=\"" + QFileInfo(imagePath).fileName() + "\""));
    QFile *file = new QFile(imagePath);
    if (!file->open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open image file for upload:" << imagePath;
        delete file;
        delete multiPart;
        if (client && client->isOpen()) {
            client->write("HTTP/1.1 500 Internal Server Error\r\nContent-Type: application/json\r\n\r\n{\"error\":\"Failed to open image file for upload\"}");
        }
        return;
    }
    imagePart.setBodyDevice(file);
    file->setParent(multiPart); // Ensure file is deleted with multiPart

    multiPart->append(imagePart);

    QUrl url(uploadUrl);
    QNetworkRequest request(url);

    QNetworkReply *reply = networkManager->post(request, multiPart);
    multiPart->setParent(reply); // Ensure multiPart is deleted with reply

    connect(reply, &QNetworkReply::finished, [this, reply, client, imagePath]() {
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "Image uploaded successfully:" << imagePath;
            if (client && client->isOpen()) {
                // Optionally, send a success message back to the original client if needed
                // client->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"image uploaded\"}");
            }
        } else {
            qWarning() << "Image upload failed:" << reply->errorString();
            if (client && client->isOpen()) {
                 // client->write("HTTP/1.1 500 Internal Server Error\r\nContent-Type: application/json\r\n\r\n{\"error\":\"Image upload failed: " + reply->errorString().toUtf8() + "\"}");
            }
        }
        reply->deleteLater();
    });
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
        QByteArray resp = "";

        QStringList urls; urls << params.value("url");
        QString printer = params.value("p", "Default");
        double l = params.value("l", "0.5").toDouble();
        double t = params.value("t", "0.5").toDouble();
        double r = params.value("r", "0.5").toDouble();
        double b = params.value("b", "0.5").toDouble();
        QString paper = params.value("a", "A4");
        QString orient = params.value("o", "portrait");
        int pageFrom = params.value("pagefrom", "0").toInt();
        int pageTo = params.value("pageto", "0").toInt();
        double width = params.value("width", "0").toDouble();
        double height = params.value("height", "0").toDouble();
        if (params.contains("a") && params.value("a").contains(',')) {
            QStringList dims = params.value("a").split(',');
            if (dims.size() == 2) {
                bool ok1, ok2;
                double w = dims[0].toDouble(&ok1);
                double h = dims[1].toDouble(&ok2);
                if (ok1 && ok2 && w > 0 && h > 0) {
                    width = w;
                    height = h;
                }
            }
        }
        // Prepare the response in json
        resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n";
        resp += "[{\"status\":\"started\",\"printer\":\"" + QUrl::toPercentEncoding(printer) + "\",";
        resp += "\"urls\":[";
        for (int i = 0; i < urls.size(); ++i) {
            resp += "\"" + QUrl::toPercentEncoding(urls[i]) + "\"";
            if (i < urls.size() - 1) {
                resp += ",";
            }
        }
        resp += "],\"left\":" + QString::number(l) + ",\"top\":" + QString::number(t) +
                ",\"right\":" + QString::number(r) + ",\"bottom\":" + QString::number(b) +
                ",\"paper\":\"" + QUrl::toPercentEncoding(paper) + "\",\"orientation\":\"" +
                QUrl::toPercentEncoding(orient) + "\",\"pageFrom\":" + QString::number(pageFrom) +
                ",\"pageTo\":" + QString::number(pageTo);
        resp += ",\"width\":" + QString::number(width) + ",\"height\":" + QString::number(height) + "}\r\n";
        

        PrintHtml *job = new PrintHtml(false, true, urls, printer, l, t, r, b, paper, orient, pageFrom, pageTo, width, height, false, client, resp);
        connect(job, SIGNAL(finished()), job, SLOT(deleteLater()));
        QTimer::singleShot(0, job, SLOT(run()));

        
    } else if (endpoint == "/scan" && params.contains("url")) {
        QByteArray resp = "";

        QStringList urls; urls << params.value("url");
        QString printer = params.value("p", "Default"); // Not used in scan mode, but kept for consistency
        double l = params.value("l", "0.5").toDouble();
        double t = params.value("t", "0.5").toDouble();
        double r = params.value("r", "0.5").toDouble();
        double b = params.value("b", "0.5").toDouble();
        QString paper = params.value("a", "A4");
        QString orient = params.value("o", "portrait");
        int pageFrom = params.value("pagefrom", "0").toInt(); // Not used in scan mode
        int pageTo = params.value("pageto", "0").toInt(); // Not used in scan mode
        double width = params.value("width", "0").toDouble();
        double height = params.value("height", "0").toDouble();
        QString outputPath = params.value("output", "output.png"); // Output path for the image
        QString uploadUrl = params.value("uploadUrl", ""); // URL to upload the image

        if (params.contains("a") && params.value("a").contains(',')) {
            QStringList dims = params.value("a").split(',');
            if (dims.size() == 2) {
                bool ok1, ok2;
                double w = dims[0].toDouble(&ok1);
                double h = dims[1].toDouble(&ok2);
                if (ok1 && ok2 && w > 0 && h > 0) {
                    width = w;
                    height = h;
                }
            }
        }
        // Prepare the response in json
        resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n";
        resp += "[{\"status\":\"started\",\"urls\":[";
        for (int i = 0; i < urls.size(); ++i) {
            resp += "\"" + QUrl::toPercentEncoding(urls[i]) + "\"";
            if (i < urls.size() - 1) {
                resp += ",";
            }
        }
        resp += "],\"left\":" + QString::number(l) + ",\"top\":" + QString::number(t) +
                ",\"right\":" + QString::number(r) + ",\"bottom\":" + QString::number(b) +
                ",\"paper\":\"" + QUrl::toPercentEncoding(paper) + "\",\"orientation\":\"" +
                QUrl::toPercentEncoding(orient) + "\",\"output\":\"" + QUrl::toPercentEncoding(outputPath) + "\"";
        if (!uploadUrl.isEmpty()) {
            resp += ",\"uploadUrl\":\"" + QUrl::toPercentEncoding(uploadUrl) + "\"";
        }
        resp += ",\"width\":" + QString::number(width) + ",\"height\":" + QString::number(height) + "}\r\n";

        PrintHtml *job = new PrintHtml(false, true, urls, printer, l, t, r, b, paper, orient, pageFrom, pageTo, width, height, false, client, resp, true, outputPath);

        if (!uploadUrl.isEmpty()) {
            // Connect a slot to upload the image after it's been saved
            connect(job, &PrintHtml::scanFinished, [this, outputPath, uploadUrl, client](bool success) {
                if (success) {
                    uploadImage(outputPath, uploadUrl, client);
                }
            });
        }

        connect(job, SIGNAL(finished()), job, SLOT(deleteLater()));
        QTimer::singleShot(0, job, SLOT(run()));

    } else {
        client->write("HTTP/1.1 404 Not Found\r\n\r\n");
        client->disconnectFromHost();
    }
}
