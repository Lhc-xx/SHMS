-- SmartHomeMonitoringSystem 数据库设计文档中的用户表结构。
-- 创建 smart_home_monitor 数据库后执行本脚本。
CREATE TABLE IF NOT EXISTS t_user (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    name VARCHAR(20) NOT NULL,
    setting CHAR(64) NOT NULL,
    encrypt CHAR(64) NOT NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uk_t_user_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
