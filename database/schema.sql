-- SmartHomeMonitoringSystem 数据库设计文档中的用户表结构。
-- 创建 smart_home_monitor 数据库后执行本脚本。
CREATE TABLE IF NOT EXISTS t_user (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    name VARCHAR(20) NOT NULL,
    setting VARCHAR(64) NOT NULL,
    encrypt VARCHAR(64) NOT NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uk_t_user_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 摄像头基础信息表：type 为 0 表示枪机，1 表示球机。
CREATE TABLE IF NOT EXISTS t_camera (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    type TINYINT UNSIGNED NOT NULL,
    serial_no VARCHAR(64) NOT NULL,
    channels SMALLINT UNSIGNED NOT NULL,
    ip VARCHAR(45) NOT NULL,
    rtsp VARCHAR(512) NOT NULL,
    rtmp VARCHAR(512) NOT NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uk_t_camera_serial_no (serial_no)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
