#include "VlcPlayerWidget.hpp"

#include <QPainter>

namespace shmsqt {

VlcPlayerWidget::VlcPlayerWidget(QWidget* parent)
    : QWidget(parent),
      instance_(nullptr),
      player_(nullptr),
      libvlcNew_(nullptr),
      libvlcRelease_(nullptr),
      libvlcMediaNewLocation_(nullptr),
      libvlcMediaRelease_(nullptr),
      libvlcMediaPlayerNew_(nullptr),
      libvlcMediaPlayerRelease_(nullptr),
      libvlcMediaPlayerSetMedia_(nullptr),
      libvlcMediaPlayerPlay_(nullptr),
      libvlcMediaPlayerStop_(nullptr),
      libvlcMediaPlayerSetHwnd_(nullptr),
      playing_(false) {
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(480, 270);
    setAutoFillBackground(false);
}

VlcPlayerWidget::~VlcPlayerWidget() {
    stop();
    releasePlayer();
    if (instance_ != nullptr && libvlcRelease_ != nullptr) {
        libvlcRelease_(instance_);
        instance_ = nullptr;
    }
    if (library_.isLoaded()) {
        library_.unload();
    }
}

bool VlcPlayerWidget::play(const QString& url) {
    const QString trimmedUrl = url.trimmed();
    if (trimmedUrl.isEmpty()) {
        lastError_ = QStringLiteral("摄像头没有配置 RTSP 地址");
        update();
        return false;
    }
#ifndef Q_OS_WIN
    lastError_ = QStringLiteral("当前播放控件只实现了 Windows 嵌入播放");
    update();
    return false;
#else
    if (!loadApi()) {
        update();
        return false;
    }
    if (player_ == nullptr) {
        player_ = libvlcMediaPlayerNew_(instance_);
        if (player_ == nullptr) {
            lastError_ = QStringLiteral("libVLC 创建播放器失败");
            update();
            return false;
        }
    }

    stop();
    const QByteArray encodedUrl = trimmedUrl.toUtf8();
    libvlc_media_t* media =
        libvlcMediaNewLocation_(instance_, encodedUrl.constData());
    if (media == nullptr) {
        lastError_ = QStringLiteral("libVLC 无法打开 RTSP 地址");
        update();
        return false;
    }
    libvlcMediaPlayerSetMedia_(player_, media);
    libvlcMediaRelease_(media);

    // Windows 下将 libVLC 的视频输出窗口绑定到当前 QT 原生窗口。
    libvlcMediaPlayerSetHwnd_(player_, reinterpret_cast<void*>(winId()));
    if (libvlcMediaPlayerPlay_(player_) < 0) {
        lastError_ = QStringLiteral("libVLC 启动 RTSP 播放失败");
        update();
        return false;
    }
    currentUrl_ = trimmedUrl;
    lastError_.clear();
    playing_ = true;
    update();
    return true;
#endif
}

void VlcPlayerWidget::stop() {
    if (player_ != nullptr && libvlcMediaPlayerStop_ != nullptr && playing_) {
        libvlcMediaPlayerStop_(player_);
    }
    playing_ = false;
    currentUrl_.clear();
    update();
}

QString VlcPlayerWidget::lastError() const {
    return lastError_;
}

void VlcPlayerWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    if (playing_) {
        // libVLC 接管播放时会直接绘制到原生窗口，保留黑色背景避免首帧前闪烁。
        QPainter painter(this);
        painter.fillRect(rect(), Qt::black);
        return;
    }

    QPainter painter(this);
    painter.fillRect(rect(), QColor(25, 25, 25));
    painter.setPen(Qt::white);
    const QString text = lastError_.isEmpty()
                             ? QStringLiteral("请选择摄像头查看实时画面")
                             : lastError_;
    painter.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, text);
}

bool VlcPlayerWidget::loadApi() {
#ifndef Q_OS_WIN
    return false;
#else
    if (instance_ != nullptr) {
        return true;
    }
    library_.setFileName(QStringLiteral("libvlc.dll"));
    if (!library_.load()) {
        lastError_ = QStringLiteral("找不到 libvlc.dll，请将 32 位 VLC 文件部署到客户端目录");
        return false;
    }

    libvlcNew_ = reinterpret_cast<LibvlcNew>(library_.resolve("libvlc_new"));
    libvlcRelease_ = reinterpret_cast<LibvlcRelease>(
        library_.resolve("libvlc_release"));
    libvlcMediaNewLocation_ = reinterpret_cast<LibvlcMediaNewLocation>(
        library_.resolve("libvlc_media_new_location"));
    libvlcMediaRelease_ = reinterpret_cast<LibvlcMediaRelease>(
        library_.resolve("libvlc_media_release"));
    libvlcMediaPlayerNew_ = reinterpret_cast<LibvlcMediaPlayerNew>(
        library_.resolve("libvlc_media_player_new"));
    libvlcMediaPlayerRelease_ = reinterpret_cast<LibvlcMediaPlayerRelease>(
        library_.resolve("libvlc_media_player_release"));
    libvlcMediaPlayerSetMedia_ = reinterpret_cast<LibvlcMediaPlayerSetMedia>(
        library_.resolve("libvlc_media_player_set_media"));
    libvlcMediaPlayerPlay_ = reinterpret_cast<LibvlcMediaPlayerPlay>(
        library_.resolve("libvlc_media_player_play"));
    libvlcMediaPlayerStop_ = reinterpret_cast<LibvlcMediaPlayerStop>(
        library_.resolve("libvlc_media_player_stop"));
    libvlcMediaPlayerSetHwnd_ = reinterpret_cast<LibvlcMediaPlayerSetHwnd>(
        library_.resolve("libvlc_media_player_set_hwnd"));

    if (libvlcNew_ == nullptr || libvlcRelease_ == nullptr ||
        libvlcMediaNewLocation_ == nullptr || libvlcMediaRelease_ == nullptr ||
        libvlcMediaPlayerNew_ == nullptr ||
        libvlcMediaPlayerRelease_ == nullptr ||
        libvlcMediaPlayerSetMedia_ == nullptr ||
        libvlcMediaPlayerPlay_ == nullptr ||
        libvlcMediaPlayerStop_ == nullptr ||
        libvlcMediaPlayerSetHwnd_ == nullptr) {
        lastError_ = QStringLiteral("libvlc.dll 缺少必要的播放接口");
        library_.unload();
        return false;
    }

    const char* arguments[] = {
        "--no-video-title-show",
        "--rtsp-tcp",
        "--network-caching=300"
    };
    instance_ = libvlcNew_(3, arguments);
    if (instance_ == nullptr) {
        lastError_ = QStringLiteral("libVLC 初始化失败，请检查 VLC plugins 目录");
        library_.unload();
        return false;
    }
    lastError_.clear();
    return true;
#endif
}

void VlcPlayerWidget::releasePlayer() {
    if (player_ != nullptr && libvlcMediaPlayerRelease_ != nullptr) {
        libvlcMediaPlayerRelease_(player_);
        player_ = nullptr;
    }
}

}  // namespace shmsqt
