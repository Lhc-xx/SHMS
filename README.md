# Smart Home Monitoring System

智能家居监控系统服务端项目，服务端目标环境为 Ubuntu 22.04，使用 C++11 和 CMake 构建。当前开发阶段完成了配置文件、服务器日志、ThreadPool、Reactor、TCP 通信、TLV 协议、MySQL 用户 DAO 和用户注册/登录服务层，为后续协议接入及摄像头业务提供基础。

## 当前实现

- `Configuration`：单例配置对象，读取并校验 `conf/server.conf`。
- `MyLogger`：单例日志对象，使用 `log4cpp` 写入服务端日志文件。
- `ThreadPool`：有界任务队列和可优雅停止的 C++11 工作线程池。
- `Reactor`：基于 Linux `epoll + eventfd` 的单线程事件循环，支持文件描述符注册、修改、移除和跨线程唤醒停止。
- `TcpConnection`：非阻塞 TCP 连接、收包回调、发送队列和 4 MB 待发送数据上限。
- `TcpServer`：IPv4 监听、接受连接、连接生命周期管理和 Reactor 事件注册。
- `ProtocolParser`：解析 `Type(4) + Length(4) + Body` 网络字节序帧，支持 TCP 分包/粘包和最大消息体限制。
- `MessageDispatcher`：按消息类型注册和调用业务处理器，隔离协议解析与业务逻辑。
- `UserProtocol`：定义注册/登录消息类型，并使用长度前缀字段编码用户请求和响应。
- `UserProtocolHandler`：将用户协议请求接入 `UserService`，返回结构化结果码。
- `ProtocolSession`：为每条 TCP 连接维护协议解析状态、消息分发和响应发送，并处理心跳。
- `ProtocolTcpServer`：将 TCP 连接生命周期与 `ProtocolSession` 绑定，统一处理协议收包和异常断开。
- `MySqlClient`：MySQL C API RAII 封装，所有带用户输入的 SQL 使用预处理参数绑定。
- `UserDao`：实现 `t_user` 建表、用户创建和按用户名查询。
- `CameraDao`：实现 `t_camera` 建表、摄像头创建、单个查询和列表查询。
- `CameraService`：在用户登录后加载摄像头列表到内存，提供按 ID 查询和查看日志。
- `PasswordHasher`：实现与 `$1$` MD5-crypt 兼容的加盐密码生成和校验。
- `UserService`：实现用户注册、用户登录、重复用户/错误密码处理，并通过依赖抽象隔离 DAO。
- `SmartHomeServer`：启动时读取配置、初始化日志、启动工作线程池和协议 TCP 服务，并进入 Reactor 事件循环。
- 单元测试：配置解析、错误回滚、日志级别、业务操作日志、并发写入、Reactor 事件分发、TCP 回环收发、协议分包/粘包、消息分发和 DAO 输入校验。

## 目录结构

```text
include/       公共头文件
src/           服务端源代码
test/          单元测试
conf/          服务端配置
database/      数据库建表脚本
data/          录像文件目录
log/           运行日志目录
docs/          需求、设计和测试文档
```

## 云服务器依赖

在 Ubuntu 22.04 云服务器上执行：

```bash
sudo apt-get update
sudo apt-get install -y g++ cmake make liblog4cpp5-dev default-libmysqlclient-dev
```

`CMakeLists.txt` 会检查 `log4cpp/Category.hh`、`liblog4cpp` 和 MySQL 客户端库。如果依赖缺失，配置阶段会直接失败，避免服务端误用未配置的第三方依赖。

## 构建与测试

在项目根目录执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel "$(nproc)"
ctest --test-dir build --output-on-failure
```

预期测试包括：

- `configuration_test`
- `my_logger_test`
- `thread_pool_test`
- `reactor_test`
- `tcp_server_test`
- `protocol_test`
- `database_test`
- `user_service_test`
- `camera_service_test`
- `user_protocol_test`
- `protocol_session_test`
- `protocol_tcp_server_test`

所有测试都通过后，再进行服务启动验证。

## 服务启动与日志验证

默认监听地址为 `127.0.0.1:7777`，可在 `conf/server.conf` 中调整。

```bash
mkdir -p data log
./build/SmartHomeServer ./conf/server.conf
grep -E "server configuration loaded|server bootstrap completed" log/server.log
```

成功时，终端会打印配置内容，`log/server.log` 会追加包含时间、级别、类别和消息的日志记录，并包含 `reactor initialized`、`TCP server listening` 启动记录。服务会持续运行，使用 `Ctrl+C` 停止。

在云服务器的另一个终端验证端口和 TCP 连接：

```bash
ss -ltnp | grep ':7777'
printf 'tcp-probe' | nc -w 2 127.0.0.1 7777
grep -E "TCP server listening|tcp client connected|tcp client disconnected" log/server.log
```

TCP 层负责可靠的非阻塞连接收发和生命周期管理。服务端主程序已通过 `ProtocolTcpServer` 接入协议会话，协议层已完成通用 TLV 帧解析、连接级会话、消息分发和心跳处理，用户注册/登录服务层、用户协议处理器和摄像头列表缓存服务已经完成；具体 TCP 会话接入数据库业务、数据库连接配置以及视频和录像业务将在后续模块接入。

数据库层当前不在 `server.conf` 中保存账号密码，需由部署环境向 `MySqlClient::connect()` 提供连接参数。`database_test` 不需要真实数据库连接，会验证用户和摄像头 DAO 的输入校验；`user_service_test` 使用内存存储验证注册、登录、重复用户、错误密码和 MD5-crypt 兼容哈希。初始化用户和摄像头表可执行：

```bash
mysql -u <db_user> -p smart_home_monitor < database/schema.sql
```

生产服务仍需由部署层创建 DAO、连接 MySQL 后注入业务服务；用户登录成功后调用 `CameraService::load()` 加载设备列表。

用户协议消息类型为 `1001/1002`（注册请求/响应）和 `1003/1004`（登录请求/响应）。注册/登录请求体依次为 `username_length(uint32)`、`username`、`password_length(uint32)`、`password`；响应体依次为 `code(uint32)`、`user_id(uint64)`、`message_length(uint32)`、`message`，整数均为网络字节序。协议处理器返回业务失败响应，不会把密码写入日志。

系统心跳消息类型为 `9001/9002`（`HEARTBEAT_REQ/HEARTBEAT_RESP`），心跳请求体为空，服务端收到后返回空响应体。

业务模块完成用户注册、用户登录或查看摄像头后，应调用以下接口记录操作：

```cpp
shms::MyLogger::instance().recordUserRegistration(username, true);
shms::MyLogger::instance().recordUserLogin(username, true);
shms::MyLogger::instance().recordCameraView(username, cameraId);
```

日志接口不接收密码，业务代码不得将密码写入日志。

## 分支流程

```text
dev-lhc  开发并推送，供云服务器验证
test     云服务器验证通过后再推送
master   最终分支，当前阶段不推送
```

本阶段只推送 `dev-lhc`。云服务器验证通过并收到确认后，再将相同提交推送到 `test`。
