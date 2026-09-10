#ifndef SHMS_QT_PROTOCOL_CLIENT_HPP
#define SHMS_QT_PROTOCOL_CLIENT_HPP

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QTcpSocket>

#include <QtGlobal>

namespace shmsqt {

// 客户端收到的摄像头记录，与服务端 t_camera 表字段一一对应。
struct CameraInfo {
    quint64 id;
    quint32 type;
    quint32 channels;
    QString serialNo;
    QString ip;
    QString rtsp;
    QString rtmp;

    CameraInfo() : id(0), type(0), channels(0) {}
};

// QT 客户端的 TCP 协议封装。该类只负责连接、TLV 帧和业务消息编解码，
// 界面层通过信号接收结果，不直接操作 QTcpSocket。
class ProtocolClient : public QObject {
    Q_OBJECT

public:
    explicit ProtocolClient(QObject* parent = nullptr);

    // 连接服务端。连接成功后会自动发送此前排队的登录或注册请求。
    void connectToServer(const QString& host, quint16 port);

    // 发送注册请求。密码由服务端负责哈希和保存，客户端不保存密码。
    void registerAccount(const QString& username, const QString& password);

    // 发送登录请求。登录成功后当前 TCP 连接才具备访问摄像头的权限。
    void login(const QString& username, const QString& password);

    // 登录成功后请求摄像头列表或查看指定摄像头。
    void requestCameraList();
    void requestCameraView(quint64 cameraId);

    // 主动断开当前连接并清除登录状态。
    void disconnectFromServer();

    bool isConnected() const;
    bool isAuthenticated() const;
    QString username() const;

signals:
    void connectedToServer();
    void disconnectedFromServer();
    void registrationFinished(bool succeeded, const QString& message);
    void loginFinished(bool succeeded,
                       const QString& message,
                       quint64 userId);
    void cameraListFinished(const QList<shmsqt::CameraInfo>& cameras,
                            const QString& message);
    void cameraViewFinished(bool succeeded,
                            const shmsqt::CameraInfo& camera,
                            const QString& message);
    void networkError(const QString& message);

private slots:
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    enum PendingOperation {
        NoOperation,
        RegisterOperation,
        LoginOperation
    };

    bool encodeCredentials(const QString& username,
                           const QString& password,
                           QByteArray* body,
                           QString* error) const;
    bool sendFrame(quint32 type, const QByteArray& body);
    bool sendOrQueue(quint32 type,
                     const QByteArray& body,
                     PendingOperation operation);
    void processFrame(quint32 type, const QByteArray& body);
    bool decodeUserResponse(const QByteArray& body,
                            quint32* code,
                            quint64* userId,
                            QString* message,
                            QString* error) const;
    bool decodeCameraResponse(const QByteArray& body,
                              quint32* code,
                              QList<CameraInfo>* cameras,
                              QString* message,
                              QString* error) const;
    bool readString(const QByteArray& body,
                    int* offset,
                    quint32 maximum,
                    QString* value,
                    QString* error) const;
    void clearPendingOperation();
    void reportError(const QString& message);

    QTcpSocket socket_;
    QByteArray receiveBuffer_;
    bool authenticated_;
    QString username_;
    QString host_;
    quint16 port_;
    QString pendingUsername_;
    PendingOperation pendingOperation_;
    quint32 pendingType_;
    QByteArray pendingBody_;
};

}  // namespace shmsqt

Q_DECLARE_METATYPE(shmsqt::CameraInfo)

#endif  // SHMS_QT_PROTOCOL_CLIENT_HPP
