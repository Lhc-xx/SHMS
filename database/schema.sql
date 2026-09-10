-- SmartHomeMonitoringSystem user schema from the database design document.
-- Execute after creating the smart_home_monitor database.
CREATE TABLE IF NOT EXISTS t_user (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    name VARCHAR(20) NOT NULL,
    setting CHAR(64) NOT NULL,
    encrypt CHAR(64) NOT NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uk_t_user_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
