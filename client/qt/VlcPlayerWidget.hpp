#ifndef SHMS_QT_VLC_PLAYER_WIDGET_HPP
#define SHMS_QT_VLC_PLAYER_WIDGET_HPP

#include <QLibrary>
#include <QString>
#include <QWidget>

// 只声明 libVLC 的不透明类型，客户端运行时动态加载 libvlc.dll，
// 因此 QT 工程不需要绑定某一种编译器格式的 VLC 导入库。
struct libvlc_instance_t;
struct libvlc_media_t;
struct libvlc_media_player_t;

namespace shmsqt {

// 使用 libVLC 将 RTSP 画面渲染到当前 QT 窗口。客户端和 libVLC 必须同为
// 32 位，libvlc.dll 以及 plugins 目录需要部署到程序运行目录。
class VlcPlayerWidget : public QWidget {
    Q_OBJECT

public:
    explicit VlcPlayerWidget(QWidget* parent = nullptr);
    ~VlcPlayerWidget() override;

    // 播放指定 RTSP 地址。返回 false 时可通过 lastError 获取原因。
    bool play(const QString& url);
    void stop();
    QString lastError() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    typedef libvlc_instance_t* (*LibvlcNew)(int, const char* const*);
    typedef void (*LibvlcRelease)(libvlc_instance_t*);
    typedef libvlc_media_t* (*LibvlcMediaNewLocation)(
        libvlc_instance_t*, const char*);
    typedef void (*LibvlcMediaRelease)(libvlc_media_t*);
    typedef libvlc_media_player_t* (*LibvlcMediaPlayerNew)(
        libvlc_instance_t*);
    typedef void (*LibvlcMediaPlayerRelease)(libvlc_media_player_t*);
    typedef void (*LibvlcMediaPlayerSetMedia)(libvlc_media_player_t*,
                                               libvlc_media_t*);
    typedef int (*LibvlcMediaPlayerPlay)(libvlc_media_player_t*);
    typedef void (*LibvlcMediaPlayerStop)(libvlc_media_player_t*);
    typedef void (*LibvlcMediaPlayerSetHwnd)(libvlc_media_player_t*, void*);

    bool loadApi();
    void releasePlayer();

    QLibrary library_;
    libvlc_instance_t* instance_;
    libvlc_media_player_t* player_;
    LibvlcNew libvlcNew_;
    LibvlcRelease libvlcRelease_;
    LibvlcMediaNewLocation libvlcMediaNewLocation_;
    LibvlcMediaRelease libvlcMediaRelease_;
    LibvlcMediaPlayerNew libvlcMediaPlayerNew_;
    LibvlcMediaPlayerRelease libvlcMediaPlayerRelease_;
    LibvlcMediaPlayerSetMedia libvlcMediaPlayerSetMedia_;
    LibvlcMediaPlayerPlay libvlcMediaPlayerPlay_;
    LibvlcMediaPlayerStop libvlcMediaPlayerStop_;
    LibvlcMediaPlayerSetHwnd libvlcMediaPlayerSetHwnd_;
    QString lastError_;
    QString currentUrl_;
    bool playing_;
};

}  // namespace shmsqt

#endif  // SHMS_QT_VLC_PLAYER_WIDGET_HPP
