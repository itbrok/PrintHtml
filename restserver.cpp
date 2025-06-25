#include "restserver.h"
#include <QUrl>
#include <QTimer>
#include "scanimage.h" // Added for ScanImage functionality

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

    } else if (endpoint == "/scan") {
        // Parameters for scanning
        QString scannerName = params.value("scanner", "Default");
        QString outputFile = params.value("output_file", "");
        QString uploadUrl = params.value("upload_url", "");
        bool jsonOutput = true; // REST server implies JSON output for scan results

        // Prepare initial response
        // Unlike print, the full scan result (success/failure, paths) will be handled by ScanImage's own JSON output.
        // The REST server will just acknowledge the request.
        // However, ScanImage is currently designed to print JSON to stdout.
        // For a true REST API, ScanImage would need to return its JSON data to RestServer
        // or RestServer would need to capture stdout if ScanImage is run in a separate process.
        // For now, we'll assume ScanImage's direct JSON output is acceptable for the use case,
        // and the client will receive a simple ack then wait for ScanImage's own output (if applicable, or check logs).
        // This is a point of potential redesign if ScanImage's output needs to be part of the HTTP response body.

        QByteArray resp = "HTTP/1.1 202 Accepted\r\nContent-Type: application/json\r\n\r\n";
        resp += "{\"status\":\"scan_initiated\",";
        resp += "\"scanner\":\"" + QUrl::toPercentEncoding(scannerName) + "\",";
        if (!outputFile.isEmpty()) {
            resp += "\"output_file\":\"" + QUrl::toPercentEncoding(outputFile) + "\",";
        }
        if (!uploadUrl.isEmpty()) {
            resp += "\"upload_url\":\"" + QUrl::toPercentEncoding(uploadUrl) + "\",";
        }
        // Remove trailing comma if any
        if (resp.endsWith(',')) {
            resp.chop(1);
        }
        resp += "}\r\n";
        client->write(resp);
        client->disconnectFromHost(); // Disconnect after sending ack; ScanImage runs async

        // Create and run the scan job
        // Note: ScanImage currently prints JSON to stdout. This might not be what a REST client expects.
        // A more robust solution would involve ScanImage emitting a signal with the JSON data,
        // which RestServer would then send back to the client.
        // However, sticking to current ScanImage design for this step.
        ScanImage *scanJob = new ScanImage(scannerName, outputFile, uploadUrl, jsonOutput);
        // Make sure ScanImage deletes itself after it's done if it's not managed by a client socket response
        QObject::connect(scanJob, &ScanImage::finished, scanJob, &QObject::deleteLater);
        QObject::connect(scanJob, &ScanImage::errorOccurred, scanJob, &QObject::deleteLater); // Also delete on error
        QTimer::singleShot(0, scanJob, SLOT(run())); // Run immediately in the event loop

    } else {
        client->write("HTTP/1.1 404 Not Found\r\n\r\n");
        client->disconnectFromHost();
    }
}
