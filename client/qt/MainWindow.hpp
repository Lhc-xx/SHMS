#ifndef SHMS_QT_MAIN_WINDOW_HPP
#define SHMS_QT_MAIN_WINDOW_HPP

#include "ProtocolClient.hpp"
#include "VlcPlayerWidget.hpp"

#include <QMainWindow>

class QLineEdit;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTableWidget;

namespace shmsqt {

// QT 客户端主窗口：未登录时显示登录/注册表单，登录成功后显示摄像头列表
// 和实时画面区域。
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onLoginFinished(bool succeeded,
                         const QString& message,
                         quint64 userId);
    void onRegistrationFinished(bool succeeded, const QString& message);
    void onCameraListFinished(const QList<shmsqt::CameraInfo>& cameras,
                              const QString& message);
    void onCameraViewFinished(bool succeeded,
                              const shmsqt::CameraInfo& camera,
                              const QString& message);
    void onNetworkError(const QString& message);
    void onRefreshClicked();
    void onPlayClicked();
    void onStopClicked();
    void onLogoutClicked();

private:
    QWidget* createLoginPage();
    QWidget* createCameraPage();
    void setLoginControlsEnabled(bool enabled);
    void populateCameraTable(const QList<CameraInfo>& cameras);
    quint16 portFromInput(bool* valid) const;
    QString responseMessage(const QString& serverMessage,
                            bool succeeded,
                            bool login) const;

    ProtocolClient client_;
    QStackedWidget* pages_;

    QLineEdit* hostEdit_;
    QLineEdit* portEdit_;
    QLineEdit* usernameEdit_;
    QLineEdit* passwordEdit_;
    QLineEdit* confirmPasswordEdit_;
    QPushButton* loginButton_;
    QPushButton* registerButton_;
    QLabel* loginStatusLabel_;

    QLabel* accountLabel_;
    QLabel* cameraStatusLabel_;
    QTableWidget* cameraTable_;
    QPushButton* refreshButton_;
    QPushButton* playButton_;
    QPushButton* stopButton_;
    QPushButton* logoutButton_;
    VlcPlayerWidget* videoWidget_;
};

}  // namespace shmsqt

#endif  // SHMS_QT_MAIN_WINDOW_HPP
