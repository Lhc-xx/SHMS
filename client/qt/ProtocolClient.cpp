#include "ProtocolClient.hpp"

#include <QAbstractSocket>

namespace {

const quint32 kMaxBodySize = 1024U * 1024U;
const quint32 kMaxUsernameBytes = 20U;
const quint32 kMaxPasswordBytes = 128U;
const quint32 kMaxMessageBytes = 512U;
const quint32 kMaxCameraCount = 4096U;

void appendUint32(quint32 value, QByteArray* output) {
    output->append(static_cast<char>((value >> 24) & 0xffU));
    output->append(static_cast<char>((value >> 16) & 0xffU));
    output->append(static_cast<char>((value >> 8) & 0xffU));
    output->append(static_cast<char>(value & 0xffU));
}

void appendUint64(quint64 value, QByteArray* output) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        output->append(static_cast<char>((value >> shift) & 0xffU));
    }
}

quint32 readUint32(const QByteArray& data, int offset) {
    return (static_cast<quint32>(static_cast<unsigned char>(data.at(offset)))
            << 24) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 1)))
            << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 2)))
            << 8) |
           static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 3)));
}

quint64 readUint64(const QByteArray& data, int offset) {
    quint64 value = 0;
    for (int index = 0; index < 8; ++index) {
        value = (value << 8) |
                static_cast<quint64>(
                    static_cast<unsigned char>(data.at(offset + index)));
    }
    return value;
}

QString defaultProtocolError() {
    return QStringLiteral("协议响应格式错误");
}

}  // 匿名命名空间

namespace shmsqt {

ProtocolClient::ProtocolClient(QObject* parent)
    : QObject(parent),
      authenticated_(false),
      port_(7777),
      pendingOperation_(NoOperation),
      pendingType_(0) {
    connect(&socket_, &QTcpSocket::connected,
            this, &ProtocolClient::onConnected);
    connect(&socket_, &QTcpSocket::readyRead,
            this, &ProtocolClient::onReadyRead);
    connect(&socket_, &QTcpSocket::disconnected,
            this, &ProtocolClient::onDisconnected);
    connect(&socket_,
            QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &ProtocolClient::onSocketError);
}

void ProtocolClient::connectToServer(const QString& host, quint16 port) {
    host_ = host.trimmed();
    port_ = port;
    receiveBuffer_.clear();
    clearPendingOperation();
    authenticated_ = false;
    username_.clear();

    if (socket_.state() != QAbstractSocket::UnconnectedState) {
        socket_.abort();
    }
    socket_.connectToHost(host_, port_);
}

void ProtocolClient::registerAccount(const QString& username,
                                     const QString& password) {
    QByteArray body;
    QString error;
    if (!encodeCredentials(username, password, &body, &error)) {
        reportError(error);
        return;
    }
    sendOrQueue(1001U, body, RegisterOperation);
}

void ProtocolClient::login(const QString& username, const QString& password) {
    if (authenticated_) {
        reportError(QStringLiteral("当前连接已经登录，请先退出当前账号"));
        return;
    }

    QByteArray body;
    QString error;
    if (!encodeCredentials(username, password, &body, &error)) {
        reportError(error);
        return;
    }
    pendingUsername_ = username;
    sendOrQueue(1003U, body, LoginOperation);
}

void ProtocolClient::requestCameraList() {
    if (!authenticated_) {
        reportError(QStringLiteral("请先登录，再请求摄像头列表"));
        return;
    }
    sendFrame(2001U, QByteArray());
}

void ProtocolClient::requestCameraView(quint64 cameraId) {
    if (!authenticated_) {
        reportError(QStringLiteral("请先登录，再查看摄像头"));
        return;
    }
    if (cameraId == 0) {
        reportError(QStringLiteral("摄像头编号必须大于零"));
        return;
    }
    QByteArray body;
    appendUint64(cameraId, &body);
    sendFrame(2003U, body);
}

void ProtocolClient::disconnectFromServer() {
    clearPendingOperation();
    authenticated_ = false;
    username_.clear();
    pendingUsername_.clear();
    receiveBuffer_.clear();
    if (socket_.state() == QAbstractSocket::UnconnectedState) {
        emit disconnectedFromServer();
    } else {
        socket_.disconnectFromHost();
    }
}

bool ProtocolClient::isConnected() const {
    return socket_.state() == QAbstractSocket::ConnectedState;
}

bool ProtocolClient::isAuthenticated() const {
    return authenticated_;
}

QString ProtocolClient::username() const {
    return username_;
}

void ProtocolClient::onConnected() {
    emit connectedToServer();
    if (pendingOperation_ == NoOperation) {
        return;
    }
    const quint32 type = pendingType_;
    const QByteArray body = pendingBody_;
    clearPendingOperation();
    if (!sendFrame(type, body)) {
        socket_.disconnectFromHost();
    }
}

void ProtocolClient::onReadyRead() {
    receiveBuffer_.append(socket_.readAll());
    while (receiveBuffer_.size() >= 8) {
        const quint32 type = readUint32(receiveBuffer_, 0);
        const quint32 bodySize = readUint32(receiveBuffer_, 4);
        if (bodySize > kMaxBodySize) {
            reportError(QStringLiteral("服务端响应超过允许的最大长度"));
            socket_.disconnectFromHost();
            return;
        }

        const qint64 frameSize = 8LL + static_cast<qint64>(bodySize);
        if (receiveBuffer_.size() < frameSize) {
            return;
        }
        const QByteArray body = receiveBuffer_.mid(8, static_cast<int>(bodySize));
        receiveBuffer_.remove(0, static_cast<int>(frameSize));
        processFrame(type, body);
    }
}

void ProtocolClient::onDisconnected() {
    const bool wasAuthenticated = authenticated_;
    authenticated_ = false;
    username_.clear();
    pendingUsername_.clear();
    receiveBuffer_.clear();
    clearPendingOperation();
    emit disconnectedFromServer();
    if (wasAuthenticated) {
        emit networkError(QStringLiteral("服务端连接已断开，登录状态已清除"));
    }
}

void ProtocolClient::onSocketError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    reportError(socket_.errorString());
}

bool ProtocolClient::encodeCredentials(const QString& username,
                                       const QString& password,
                                       QByteArray* body,
                                       QString* error) const {
    if (body == nullptr || error == nullptr) {
        return false;
    }
    const QByteArray usernameBytes = username.toUtf8();
    const QByteArray passwordBytes = password.toUtf8();
    if (usernameBytes.isEmpty() ||
        usernameBytes.size() > static_cast<int>(kMaxUsernameBytes)) {
        *error = QStringLiteral("用户名不能为空且不能超过 20 字节");
        return false;
    }
    if (passwordBytes.isEmpty() ||
        passwordBytes.size() > static_cast<int>(kMaxPasswordBytes)) {
        *error = QStringLiteral("密码不能为空且不能超过 128 字节");
        return false;
    }

    body->clear();
    appendUint32(static_cast<quint32>(usernameBytes.size()), body);
    body->append(usernameBytes);
    appendUint32(static_cast<quint32>(passwordBytes.size()), body);
    body->append(passwordBytes);
    error->clear();
    return true;
}

bool ProtocolClient::sendFrame(quint32 type, const QByteArray& body) {
    if (!isConnected()) {
        reportError(QStringLiteral("尚未连接服务端"));
        return false;
    }
    if (type == 0 || body.size() > static_cast<int>(kMaxBodySize)) {
        reportError(QStringLiteral("待发送协议数据无效"));
        return false;
    }

    QByteArray frame;
    frame.reserve(8 + body.size());
    appendUint32(type, &frame);
    appendUint32(static_cast<quint32>(body.size()), &frame);
    frame.append(body);
    if (socket_.write(frame) < 0) {
        reportError(socket_.errorString());
        return false;
    }
    return true;
}

bool ProtocolClient::sendOrQueue(quint32 type,
                                 const QByteArray& body,
                                 PendingOperation operation) {
    if (isConnected()) {
        clearPendingOperation();
        return sendFrame(type, body);
    }

    if (host_.isEmpty() || port_ == 0) {
        reportError(QStringLiteral("请先填写有效的服务端地址和端口"));
        return false;
    }
    pendingOperation_ = operation;
    pendingType_ = type;
    pendingBody_ = body;
    if (socket_.state() == QAbstractSocket::UnconnectedState) {
        socket_.connectToHost(host_, port_);
    }
    return true;
}

void ProtocolClient::processFrame(quint32 type, const QByteArray& body) {
    if (type == 1002U || type == 1004U) {
        quint32 code = 0;
        quint64 userId = 0;
        QString message;
        QString error;
        if (!decodeUserResponse(body, &code, &userId, &message, &error)) {
            reportError(error);
            socket_.disconnectFromHost();
            return;
        }
        if (type == 1002U) {
            emit registrationFinished(code == 0U, message);
        } else {
            const bool succeeded = code == 0U;
            authenticated_ = succeeded;
            username_ = succeeded ? pendingUsername_ : QString();
            emit loginFinished(succeeded, message, userId);
            pendingUsername_.clear();
        }
        return;
    }

    if (type == 2002U || type == 2004U) {
        quint32 code = 0;
        QList<CameraInfo> cameras;
        QString message;
        QString error;
        if (!decodeCameraResponse(body, &code, &cameras, &message, &error)) {
            reportError(error);
            socket_.disconnectFromHost();
            return;
        }
        if (type == 2002U) {
            emit cameraListFinished(cameras, message);
        } else {
            const bool succeeded = code == 0U && !cameras.isEmpty();
            emit cameraViewFinished(succeeded,
                                    succeeded ? cameras.front() : CameraInfo(),
                                    message);
        }
        return;
    }

    // 当前客户端只主动发起用户和摄像头请求，其他响应不属于客户端协议。
    reportError(defaultProtocolError());
}

bool ProtocolClient::decodeUserResponse(const QByteArray& body,
                                        quint32* code,
                                        quint64* userId,
                                        QString* message,
                                        QString* error) const {
    if (code == nullptr || userId == nullptr || message == nullptr ||
        error == nullptr || body.size() < 16) {
        if (error != nullptr) {
            *error = defaultProtocolError();
        }
        return false;
    }
    *code = readUint32(body, 0);
    *userId = readUint64(body, 4);
    if (*code > 7U) {
        *error = QStringLiteral("用户响应状态码无效");
        return false;
    }
    int offset = 12;
    if (!readString(body, &offset, kMaxMessageBytes, message, error) ||
        offset != body.size()) {
        if (offset == body.size()) {
            error->clear();
        } else if (error->isEmpty()) {
            *error = defaultProtocolError();
        }
        return false;
    }
    error->clear();
    return true;
}

bool ProtocolClient::decodeCameraResponse(const QByteArray& body,
                                          quint32* code,
                                          QList<CameraInfo>* cameras,
                                          QString* message,
                                          QString* error) const {
    if (code == nullptr || cameras == nullptr || message == nullptr ||
        error == nullptr || body.size() < 8) {
        if (error != nullptr) {
            *error = defaultProtocolError();
        }
        return false;
    }
    *code = readUint32(body, 0);
    if (*code > 4U) {
        *error = QStringLiteral("摄像头响应状态码无效");
        return false;
    }
    int offset = 4;
    if (!readString(body, &offset, kMaxMessageBytes, message, error) ||
        offset + 4 > body.size()) {
        if (error->isEmpty()) {
            *error = defaultProtocolError();
        }
        return false;
    }
    const quint32 count = readUint32(body, offset);
    offset += 4;
    if (count > kMaxCameraCount) {
        *error = QStringLiteral("摄像头数量超过允许的最大值");
        return false;
    }

    cameras->clear();
    cameras->reserve(static_cast<int>(count));
    for (quint32 index = 0; index < count; ++index) {
        if (offset + 16 > body.size()) {
            *error = defaultProtocolError();
            cameras->clear();
            return false;
        }
        CameraInfo camera;
        camera.id = readUint64(body, offset);
        camera.type = readUint32(body, offset + 8);
        camera.channels = readUint32(body, offset + 12);
        offset += 16;
        if (camera.id == 0 || camera.type > 1U || camera.channels == 0U ||
            camera.channels > 64U ||
            !readString(body, &offset, 64U, &camera.serialNo, error) ||
            !readString(body, &offset, 45U, &camera.ip, error) ||
            !readString(body, &offset, 512U, &camera.rtsp, error) ||
            !readString(body, &offset, 512U, &camera.rtmp, error)) {
            cameras->clear();
            if (error->isEmpty()) {
                *error = defaultProtocolError();
            }
            return false;
        }
        cameras->append(camera);
    }
    if (offset != body.size()) {
        cameras->clear();
        *error = QStringLiteral("摄像头响应包含多余数据");
        return false;
    }
    error->clear();
    return true;
}

bool ProtocolClient::readString(const QByteArray& body,
                                int* offset,
                                quint32 maximum,
                                QString* value,
                                QString* error) const {
    if (offset == nullptr || value == nullptr || error == nullptr ||
        *offset < 0 || *offset + 4 > body.size()) {
        if (error != nullptr) {
            *error = defaultProtocolError();
        }
        return false;
    }
    const quint32 length = readUint32(body, *offset);
    *offset += 4;
    if (length > maximum ||
        length > static_cast<quint32>(body.size() - *offset)) {
        *error = QStringLiteral("协议字符串字段无效");
        return false;
    }
    *value = QString::fromUtf8(body.constData() + *offset,
                               static_cast<int>(length));
    *offset += static_cast<int>(length);
    return true;
}

void ProtocolClient::clearPendingOperation() {
    pendingOperation_ = NoOperation;
    pendingType_ = 0;
    pendingBody_.clear();
}

void ProtocolClient::reportError(const QString& message) {
    emit networkError(message.isEmpty() ? defaultProtocolError() : message);
}

}  // namespace shmsqt
