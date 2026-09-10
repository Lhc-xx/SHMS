#include "MainWindow.hpp"

#include <QAbstractItemView>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace shmsqt {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      pages_(new QStackedWidget(this)),
      hostEdit_(nullptr),
      portEdit_(nullptr),
      usernameEdit_(nullptr),
      passwordEdit_(nullptr),
      confirmPasswordEdit_(nullptr),
      loginButton_(nullptr),
      registerButton_(nullptr),
      loginStatusLabel_(nullptr),
      accountLabel_(nullptr),
      cameraStatusLabel_(nullptr),
      cameraTable_(nullptr),
      refreshButton_(nullptr),
      playButton_(nullptr),
      stopButton_(nullptr),
      logoutButton_(nullptr),
      videoWidget_(nullptr) {
    setWindowTitle(QStringLiteral("智能家居监控系统"));
    resize(1100, 700);
    setCentralWidget(pages_);
    pages_->addWidget(createLoginPage());
    pages_->addWidget(createCameraPage());
    pages_->setCurrentIndex(0);

    connect(&client_, &ProtocolClient::registrationFinished,
            this, &MainWindow::onRegistrationFinished);
    connect(&client_, &ProtocolClient::loginFinished,
            this, &MainWindow::onLoginFinished);
    connect(&client_, &ProtocolClient::cameraListFinished,
            this, &MainWindow::onCameraListFinished);
    connect(&client_, &ProtocolClient::cameraViewFinished,
            this, &MainWindow::onCameraViewFinished);
    connect(&client_, &ProtocolClient::networkError,
            this, &MainWindow::onNetworkError);
    connect(&client_, &ProtocolClient::disconnectedFromServer,
            this, [this]() {
                if (pages_->currentIndex() == 1) {
                    videoWidget_->stop();
                    pages_->setCurrentIndex(0);
                    setLoginControlsEnabled(true);
                    loginStatusLabel_->setText(
                        QStringLiteral("服务端连接已断开，请重新登录"));
                }
            });
}

MainWindow::~MainWindow() {
    client_.disconnectFromServer();
}

QWidget* MainWindow::createLoginPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(30, 30, 30, 30);

    QGroupBox* group = new QGroupBox(QStringLiteral("连接服务端并登录"), page);
    group->setMaximumWidth(520);
    QFormLayout* form = new QFormLayout(group);

    hostEdit_ = new QLineEdit(QStringLiteral("106.55.9.115"), group);
    portEdit_ = new QLineEdit(QStringLiteral("7777"), group);
    portEdit_->setValidator(new QIntValidator(1, 65535, portEdit_));
    usernameEdit_ = new QLineEdit(group);
    passwordEdit_ = new QLineEdit(group);
    passwordEdit_->setEchoMode(QLineEdit::Password);
    confirmPasswordEdit_ = new QLineEdit(group);
    confirmPasswordEdit_->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("服务端地址："), hostEdit_);
    form->addRow(QStringLiteral("服务端端口："), portEdit_);
    form->addRow(QStringLiteral("用户名："), usernameEdit_);
    form->addRow(QStringLiteral("密码："), passwordEdit_);
    form->addRow(QStringLiteral("确认密码："), confirmPasswordEdit_);

    QHBoxLayout* buttons = new QHBoxLayout();
    loginButton_ = new QPushButton(QStringLiteral("登录"), group);
    registerButton_ = new QPushButton(QStringLiteral("注册"), group);
    buttons->addWidget(loginButton_);
    buttons->addWidget(registerButton_);
    form->addRow(QString(), buttons);

    loginStatusLabel_ = new QLabel(QStringLiteral("请输入系统账号和密码"), page);
    loginStatusLabel_->setWordWrap(true);
    loginStatusLabel_->setAlignment(Qt::AlignCenter);

    pageLayout->addStretch();
    pageLayout->addWidget(group, 0, Qt::AlignHCenter);
    pageLayout->addWidget(loginStatusLabel_, 0, Qt::AlignHCenter);
    pageLayout->addStretch();

    connect(loginButton_, &QPushButton::clicked,
            this, &MainWindow::onLoginClicked);
    connect(registerButton_, &QPushButton::clicked,
            this, &MainWindow::onRegisterClicked);
    return page;
}

QWidget* MainWindow::createCameraPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);

    QHBoxLayout* topBar = new QHBoxLayout();
    accountLabel_ = new QLabel(page);
    refreshButton_ = new QPushButton(QStringLiteral("刷新列表"), page);
    playButton_ = new QPushButton(QStringLiteral("查看实时画面"), page);
    stopButton_ = new QPushButton(QStringLiteral("停止播放"), page);
    logoutButton_ = new QPushButton(QStringLiteral("退出登录"), page);
    topBar->addWidget(accountLabel_);
    topBar->addStretch();
    topBar->addWidget(refreshButton_);
    topBar->addWidget(playButton_);
    topBar->addWidget(stopButton_);
    topBar->addWidget(logoutButton_);
    layout->addLayout(topBar);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, page);
    QWidget* listPanel = new QWidget(splitter);
    QVBoxLayout* listLayout = new QVBoxLayout(listPanel);
    listLayout->setContentsMargins(0, 0, 5, 0);
    QLabel* listTitle = new QLabel(QStringLiteral("摄像头列表（双击即可查看）"),
                                   listPanel);
    cameraTable_ = new QTableWidget(listPanel);
    cameraTable_->setColumnCount(7);
    cameraTable_->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("编号")
                      << QStringLiteral("类型")
                      << QStringLiteral("通道")
                      << QStringLiteral("序列号")
                      << QStringLiteral("摄像头 IP")
                      << QStringLiteral("RTSP 地址")
                      << QStringLiteral("RTMP 地址"));
    cameraTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    cameraTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    cameraTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    cameraTable_->horizontalHeader()->setStretchLastSection(true);
    cameraTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    listLayout->addWidget(listTitle);
    listLayout->addWidget(cameraTable_);

    QWidget* videoPanel = new QWidget(splitter);
    QVBoxLayout* videoLayout = new QVBoxLayout(videoPanel);
    videoLayout->setContentsMargins(5, 0, 0, 0);
    videoWidget_ = new VlcPlayerWidget(videoPanel);
    cameraStatusLabel_ = new QLabel(QStringLiteral("请选择摄像头查看实时画面"),
                                    videoPanel);
    cameraStatusLabel_->setWordWrap(true);
    videoLayout->addWidget(videoWidget_, 1);
    videoLayout->addWidget(cameraStatusLabel_);
    splitter->addWidget(listPanel);
    splitter->addWidget(videoPanel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 5);
    layout->addWidget(splitter, 1);

    connect(refreshButton_, &QPushButton::clicked,
            this, &MainWindow::onRefreshClicked);
    connect(playButton_, &QPushButton::clicked,
            this, &MainWindow::onPlayClicked);
    connect(stopButton_, &QPushButton::clicked,
            this, &MainWindow::onStopClicked);
    connect(logoutButton_, &QPushButton::clicked,
            this, &MainWindow::onLogoutClicked);
    connect(cameraTable_, &QTableWidget::cellDoubleClicked,
            this, [this](int, int) { onPlayClicked(); });
    return page;
}

void MainWindow::onLoginClicked() {
    bool validPort = false;
    const quint16 port = portFromInput(&validPort);
    if (hostEdit_->text().trimmed().isEmpty() || !validPort ||
        usernameEdit_->text().trimmed().isEmpty() ||
        passwordEdit_->text().isEmpty()) {
        loginStatusLabel_->setText(QStringLiteral("请填写有效的地址、端口、用户名和密码"));
        return;
    }

    setLoginControlsEnabled(false);
    loginStatusLabel_->setText(QStringLiteral("正在连接服务端并登录……"));
    client_.connectToServer(hostEdit_->text().trimmed(), port);
    client_.login(usernameEdit_->text().trimmed(), passwordEdit_->text());
}

void MainWindow::onRegisterClicked() {
    bool validPort = false;
    const quint16 port = portFromInput(&validPort);
    if (hostEdit_->text().trimmed().isEmpty() || !validPort ||
        usernameEdit_->text().trimmed().isEmpty() ||
        passwordEdit_->text().isEmpty()) {
        loginStatusLabel_->setText(QStringLiteral("请填写有效的地址、端口、用户名和密码"));
        return;
    }
    if (passwordEdit_->text() != confirmPasswordEdit_->text()) {
        loginStatusLabel_->setText(QStringLiteral("两次输入的密码不一致"));
        return;
    }

    setLoginControlsEnabled(false);
    loginStatusLabel_->setText(QStringLiteral("正在连接服务端并注册……"));
    client_.connectToServer(hostEdit_->text().trimmed(), port);
    client_.registerAccount(usernameEdit_->text().trimmed(),
                            passwordEdit_->text());
}

void MainWindow::onLoginFinished(bool succeeded,
                                  const QString& message,
                                  quint64 userId) {
    setLoginControlsEnabled(true);
    if (!succeeded) {
        loginStatusLabel_->setText(responseMessage(message, false, true));
        return;
    }
    accountLabel_->setText(QStringLiteral("当前用户：%1（ID：%2）")
                           .arg(client_.username())
                           .arg(QString::number(userId)));
    cameraStatusLabel_->setText(QStringLiteral("登录成功，正在加载摄像头列表……"));
    pages_->setCurrentIndex(1);
    client_.requestCameraList();
}

void MainWindow::onRegistrationFinished(bool succeeded,
                                         const QString& message) {
    setLoginControlsEnabled(true);
    loginStatusLabel_->setText(responseMessage(message, succeeded, false));
    if (succeeded) {
        passwordEdit_->clear();
        confirmPasswordEdit_->clear();
        passwordEdit_->setFocus();
    }
}

void MainWindow::onCameraListFinished(const QList<CameraInfo>& cameras,
                                      const QString& message) {
    populateCameraTable(cameras);
    if (message.isEmpty()) {
        cameraStatusLabel_->setText(
            QStringLiteral("已加载 %1 个摄像头，请选择后查看实时画面")
                .arg(cameras.size()));
    } else {
        cameraStatusLabel_->setText(message);
    }
}

void MainWindow::onCameraViewFinished(bool succeeded,
                                       const CameraInfo& camera,
                                       const QString& message) {
    if (!succeeded) {
        cameraStatusLabel_->setText(message.isEmpty()
                                        ? QStringLiteral("摄像头查看失败")
                                        : message);
        return;
    }
    if (!videoWidget_->play(camera.rtsp)) {
        cameraStatusLabel_->setText(videoWidget_->lastError());
        return;
    }
    cameraStatusLabel_->setText(
        QStringLiteral("正在播放摄像头 %1：%2").arg(camera.id).arg(camera.rtsp));
}

void MainWindow::onNetworkError(const QString& message) {
    if (pages_->currentIndex() == 0) {
        loginStatusLabel_->setText(message);
        setLoginControlsEnabled(true);
    } else {
        cameraStatusLabel_->setText(message);
    }
}

void MainWindow::onRefreshClicked() {
    cameraStatusLabel_->setText(QStringLiteral("正在刷新摄像头列表……"));
    client_.requestCameraList();
}

void MainWindow::onPlayClicked() {
    if (cameraTable_->currentRow() < 0) {
        cameraStatusLabel_->setText(QStringLiteral("请先选择一个摄像头"));
        return;
    }
    QTableWidgetItem* idItem = cameraTable_->item(cameraTable_->currentRow(), 0);
    bool validId = false;
    const quint64 cameraId = idItem == nullptr
                                 ? 0
                                 : idItem->text().toULongLong(&validId);
    if (!validId || cameraId == 0) {
        cameraStatusLabel_->setText(QStringLiteral("摄像头编号无效"));
        return;
    }
    cameraStatusLabel_->setText(QStringLiteral("正在获取摄像头信息……"));
    client_.requestCameraView(cameraId);
}

void MainWindow::onStopClicked() {
    videoWidget_->stop();
    cameraStatusLabel_->setText(QStringLiteral("已停止播放"));
}

void MainWindow::onLogoutClicked() {
    videoWidget_->stop();
    cameraTable_->setRowCount(0);
    client_.disconnectFromServer();
    pages_->setCurrentIndex(0);
    loginStatusLabel_->setText(QStringLiteral("已退出登录，请重新登录"));
    setLoginControlsEnabled(true);
}

void MainWindow::setLoginControlsEnabled(bool enabled) {
    loginButton_->setEnabled(enabled);
    registerButton_->setEnabled(enabled);
    hostEdit_->setEnabled(enabled);
    portEdit_->setEnabled(enabled);
    usernameEdit_->setEnabled(enabled);
    passwordEdit_->setEnabled(enabled);
    confirmPasswordEdit_->setEnabled(enabled);
}

void MainWindow::populateCameraTable(const QList<CameraInfo>& cameras) {
    cameraTable_->setRowCount(0);
    for (int row = 0; row < cameras.size(); ++row) {
        const CameraInfo& camera = cameras.at(row);
        cameraTable_->insertRow(row);
        cameraTable_->setItem(row, 0,
                              new QTableWidgetItem(QString::number(camera.id)));
        cameraTable_->setItem(row, 1,
                              new QTableWidgetItem(camera.type == 0
                                                       ? QStringLiteral("枪机")
                                                       : QStringLiteral("球机")));
        cameraTable_->setItem(row, 2,
                              new QTableWidgetItem(QString::number(camera.channels)));
        cameraTable_->setItem(row, 3, new QTableWidgetItem(camera.serialNo));
        cameraTable_->setItem(row, 4, new QTableWidgetItem(camera.ip));
        cameraTable_->setItem(row, 5, new QTableWidgetItem(camera.rtsp));
        cameraTable_->setItem(row, 6, new QTableWidgetItem(camera.rtmp));
    }
}

quint16 MainWindow::portFromInput(bool* valid) const {
    if (valid == nullptr) {
        return 0;
    }
    bool converted = false;
    const uint value = portEdit_->text().toUInt(&converted);
    *valid = converted && value >= 1U && value <= 65535U;
    return *valid ? static_cast<quint16>(value) : 0;
}

QString MainWindow::responseMessage(const QString& serverMessage,
                                    bool succeeded,
                                    bool login) const {
    if (succeeded) {
        return login ? QStringLiteral("登录成功")
                     : QStringLiteral("注册成功，请使用新账号登录");
    }
    if (!serverMessage.isEmpty()) {
        return serverMessage;
    }
    return login ? QStringLiteral("登录失败") : QStringLiteral("注册失败");
}

}  // namespace shmsqt
