# log4cpp 云服务器测试说明

本模块的服务端目标环境是 Ubuntu 22.04。日志模块使用 `log4cpp` 的 `Category`、`FileAppender` 和 `PatternLayout`，日志文件路径由 `conf/server.conf` 中的 `log_file` 指定。

## 1. 安装依赖

```bash
sudo apt-get update
sudo apt-get install -y g++ cmake make liblog4cpp5-dev
```

## 2. 构建与单元测试

在项目根目录执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

预期结果：`configuration_test` 和 `my_logger_test` 均通过。

## 3. 启动验证

```bash
mkdir -p data log
./build/SmartHomeServer ./conf/server.conf
grep -E "server configuration loaded|server bootstrap completed" log/server.log
```

预期结果：程序输出配置加载成功，`log/server.log` 中出现服务启动日志，并包含时间、级别、类别和消息。

## 4. 业务日志验证

用户注册、用户登录和查看摄像头的业务代码应分别调用：

```cpp
shms::MyLogger::instance().recordUserRegistration(username, true);
shms::MyLogger::instance().recordUserLogin(username, true);
shms::MyLogger::instance().recordCameraView(username, cameraId);
```

日志接口不会接收或记录密码。验证结束后再将已验证的代码合并到 `test` 分支。
