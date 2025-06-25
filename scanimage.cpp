#include "scanimage.h"
#include <QDebug>
#include <QCoreApplication> // For QCoreApplication::exit()
#include <QJsonObject>
#include <QJsonDocument>
#include <QStandardPaths> // For a default save location
#include <QDir>           // For creating directories
#include <QHttpMultiPart> // For file uploads

ScanImage::ScanImage(const QString& scannerName, const QString& outputFile, const QString& uploadUrl, bool jsonOutput, QObject *parent)
    : QObject(parent),
      m_scannerName(scannerName),
      m_outputFile(outputFile),
      m_uploadUrl(uploadUrl),
      m_jsonOutput(jsonOutput),
      m_networkManager(nullptr)
{
    if (!m_uploadUrl.isEmpty()) {
        m_networkManager = new QNetworkAccessManager(this);
    }
}

ScanImage::~ScanImage()
{
    // m_networkManager is a child of ScanImage, so it will be deleted automatically by Qt's parent-child system.
    // If manual cleanup for scanner objects is needed, it would go here.
    qDebug() << "ScanImage object destroyed.";
}

void ScanImage::run()
{
    qDebug() << "ScanImage::run() called. Starting scan process...";
    performScan();
}

void ScanImage::aboutToQuitApp()
{
    qDebug() << "ScanImage::aboutToQuitApp() called. Performing cleanup...";
    // Perform any necessary cleanup before the application quits
}

void ScanImage::performScan()
{
    // **Placeholder for actual scanning logic**
    // This is where you would interact with a scanning library (SANE, TWAIN, WIA, etc.)
    // For now, we'll simulate a scan and call scanFinished.

    qDebug() << "Simulating scanning from:" << m_scannerName;

    // Simulate a successful scan with a dummy image
    QImage dummyImage(100, 100, QImage::Format_RGB32);
    dummyImage.fill(Qt::blue); // Create a blue dummy image

    // Simulate some delay
    QTimer::singleShot(1000, this, [this, dummyImage]() {
        scanFinished(true, dummyImage);
    });
}

void ScanImage::scanFinished(bool success, const QImage& scannedImage)
{
    if (!success) {
        processFinished(false, "Scanning failed.");
        return;
    }

    qDebug() << "Scanning finished successfully.";

    bool savedLocally = false;
    if (!m_outputFile.isEmpty()) {
        saveImageToFile(scannedImage, m_outputFile);
        savedLocally = true;
    }

    if (!m_uploadUrl.isEmpty()) {
        uploadImage(scannedImage, m_uploadUrl);
        // The process will finish when uploadFinished is called
    } else {
        // If not uploading, the process is finished now
        processFinished(true, savedLocally ? "Scan successful and image saved." : "Scan successful (no output specified).");
    }
}

void ScanImage::saveImageToFile(const QImage& image, const QString& filePath)
{
    QFile file(filePath);
    QDir dir;
    if (!dir.mkpath(QFileInfo(filePath).absolutePath())) {
         processFinished(false, "Failed to create directory for output file: " + filePath);
         return;
    }

    if (image.save(filePath)) {
        qDebug() << "Scanned image saved to" << filePath;
    } else {
        qDebug() << "Failed to save image to" << filePath;
        // Potentially emit an error or handle it
        // For now, we'll let processFinished handle the overall status if no upload
    }
}

void ScanImage::uploadImage(const QImage& image, const QString& urlString)
{
    if (!m_networkManager) {
        m_networkManager = new QNetworkAccessManager(this);
    }

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart imagePart;
    imagePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/png")); // Or appropriate format
    imagePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"image\"; filename=\"scan.png\"")); // filename can be dynamic

    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG"); // Save image to buffer in PNG format

    imagePart.setBody(imageData);
    multiPart->append(imagePart);

    QUrl url(urlString);
    QNetworkRequest request(url);

    qDebug() << "Uploading scanned image to" << urlString;
    QNetworkReply *reply = m_networkManager->post(request, multiPart);
    multiPart->setParent(reply); // Delete multipart with reply

    connect(reply, &QNetworkReply::finished, this, &ScanImage::uploadFinished);
    // connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error), this, &ScanImage::uploadError); // Optional: more specific error handling
}

void ScanImage::uploadFinished(QNetworkReply *reply)
{
    QString message;
    bool success = false;

    if (reply->error() == QNetworkReply::NoError) {
        qDebug() << "Image uploaded successfully.";
        message = "Scan and upload successful.";
        success = true;
    } else {
        qDebug() << "Upload failed:" << reply->errorString();
        message = "Scan successful, but upload failed: " + reply->errorString();
        success = false; // Or partial success depending on requirements
    }
    reply->deleteLater();
    processFinished(success, message);
}

void ScanImage::processFinished(bool success, const QString& message)
{
    if (m_jsonOutput) {
        QJsonObject jsonResponse;
        jsonResponse["status"] = success ? "success" : "error";
        jsonResponse["message"] = message;
        jsonResponse["scanner"] = m_scannerName;
        if (!m_outputFile.isEmpty()) {
            jsonResponse["output_file"] = m_outputFile;
        }
        if (!m_uploadUrl.isEmpty()) {
            jsonResponse["upload_url"] = m_uploadUrl;
        }
        QJsonDocument doc(jsonResponse);
        printf("%s\n", doc.toJson(QJsonDocument::Compact).constData());
    } else {
        // For non-JSON output, qDebug messages have already printed details.
        // A simple final message might be useful, or a QMessageBox if it's a GUI app.
        qDebug() << "Process finished. Status:" << (success ? "Success" : "Error") << "Message:" << message;
        // If this were a GUI app or if main.cpp showed QMessageBox for scan mode:
        // QMessageBox msgBox;
        // msgBox.setWindowTitle(success ? "Scan Successful" : "Scan Error");
        // msgBox.setText(message);
        // msgBox.exec();
    }

    emit finished(); // Signal that the operation is complete
    // QCoreApplication::exit(success ? 0 : 1); // Exit application, only if not server mode and exitOnCompletion is true
}

// Note: If ScanImage instances are meant to be reusable or managed by a server,
// then QCoreApplication::exit() should not be called here.
// The 'finished()' signal is the primary way to indicate completion.
// main.cpp (or RestServer) would then decide whether to exit or handle the next request.
