# MyIoT

MyIoT 是一个基于 `macchina.io EDGE`、`POCO OSP` 和 `CMake` 的 IoT/边缘侧项目模板。

它把一个可运行的边缘服务拆成了几层：

- 一个宿主程序 `macchina`，负责启动 bundle 容器。
- 一套基于 `POCO OSP` 的平台能力和扩展模块。
- 一组面向浏览器的 Web UI bundle，用于登录、主页和运行时诊断。
- 一套可自动引导的第三方依赖构建流程，当前包含 Poco 和 Fast-DDS。

这个仓库现在更适合作为“新项目骨架”使用，而不是单一业务应用。后续你可以在它上面继续叠加设备接入、协议适配、数据采集、规则引擎或云端连接逻辑。

## 当前功能

- 推荐入口：`/`
- 未登录时会跳转到：`/myiot/login/index.html`
- 已登录后会跳转到：`/myiot/home/index.html`
- 系统包列表：`/myiot/packages/index.html`
- 登录后主页显示当前设备访问地址、IP 和端口
- 支持 OSP bundle 打包与加载
- 支持本地日志轮询展示
- 支持进程诊断控制台接口：`/myiot/services/process-console/exec`
- 支持通过 CMake 自动拉取和构建 Poco、Fast-CDR、Fast-DDS、foonathan_memory_vendor

默认示例账号：

- 用户名：`admin`
- 密码：`admin`

## 目录结构

```text
MyIoT/
├─ CMakeLists.txt          # 顶层构建入口
├─ cmake/                  # 自定义 CMake 模块
├─ server/                 # 宿主程序 macchina 和运行配置
├─ webUI/                  # 登录页、主页、包列表等 Web UI bundle
├─ platform/               # vendored macchina.io / POCO OSP 平台扩展源码
└─ build/                  # 本地构建输出目录（已被 .gitignore 忽略）
```

主要子目录说明：

- `server/`
  负责生成 `macchina` 可执行程序（Windows 下为 `macchina.exe`），并携带 `macchina.properties`、证书和启动配置。
- `webUI/Launcher/`
  登录入口与会话接口。
- `webUI/Home/`
  登录后的主控制台页面。
- `webUI/BundleList/`
  展示当前已加载 bundle 的诊断页面。
- `platform/`
  vendored 的平台模块与 OSP 相关源码，项目模板的底座基本都在这里。

## 构建方式

当前项目使用 `CMake` 构建，支持 Windows/Linux 双平台。

### 首次构建

Windows（Visual Studio 2022）：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug --parallel
```

Linux（Ninja 或 Unix Makefiles）：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

### 使用 JNDM123 的 PetaLinux 环境交叉编译

如果目标板使用 `/home/ph/work/proj/JNDM123` 这个 PetaLinux 工程生成的根文件系统和工具链，可以直接使用项目内置的 toolchain 文件和脚本：

```bash
./scripts/build-petalinux-jndm123.sh
```

这条命令会自动做几件事：

- 复用 `JNDM123/build/tmp` 里的 ARM 交叉编译器、sysroot 和 OpenSSL 组件。
- 在交叉编译场景下通过 `qemu-arm` 在主机上执行 `OSPBundleCreator`，保证 `.bndl` 包也能生成。
- 生成安装目录：`build-petalinux-jndm123/install`
- 生成部署压缩包：`build-petalinux-jndm123/myiot-petalinux-install.tar.gz`

如果你的 PetaLinux 工程不在默认路径，可以这样指定：

```bash
MYIOT_PETALINUX_PROJECT=/path/to/your/petalinux/project ./scripts/build-petalinux-jndm123.sh
```

也可以手动调用 CMake：

```bash
cmake -S . -B build-petalinux \
  -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchains/petalinux-jndm123.cmake \
  -DMYIOT_PETALINUX_PROJECT=/home/ph/work/proj/JNDM123

cmake --build build-petalinux --parallel
cmake --install build-petalinux
```

说明：

- 当前机器如果没有 `npm`，脚本会自动加上 `-DMYIOT_ENABLE_WEBUI=OFF`，优先保证板端核心服务和 OSP bundle 可以成功交叉编译。
- 如果后续安装了 `node/npm`，重新执行同一脚本即可连同 Web UI 一起构建。
- 安装后的目录里会包含 `bin/macchina`、`lib/bundles/*.bndl` 以及运行所需配置，可直接作为板端部署目录使用。

### 输出目录

项目已经统一配置了输出路径：

- 可执行文件和 DLL：`build/bin`
- 静态库和导入库：`build/lib`

例如：

- `build/bin/macchina`（Windows 下为 `build/bin/macchina.exe`）
- `build/bin/io.myiot.webui.launcherd.dll`
- `build/bin/io.myiot.webui.homed.dll`

## 运行方式

构建完成后，直接运行：

Windows：

```powershell
.\build\bin\macchina.exe
```

Linux：

```bash
./build/bin/macchina
```

运行时需要确保同目录下存在这些文件，当前构建流程会自动复制：

- `macchina.properties`

Windows 额外会复制：

- `libcrypto-3-x64.dll`
- `libssl-3-x64.dll`
- `legacy.dll`

## 默认访问地址

代码里的默认监听配置是：

- HTTP：`0.0.0.0:22080`

因此推荐的访问方式是先打开根路径：

- `http://127.0.0.1:22080/`

程序会自动跳转：

- 未登录：跳到 `/myiot/login/index.html`
- 已登录：跳到 `/myiot/home/index.html`

如果你想直接访问静态页面，也可以使用：

- `http://127.0.0.1:22080/myiot/login/index.html`
- `http://127.0.0.1:22080/myiot/home/index.html`

如果在局域网内访问，请把 `127.0.0.1` 换成设备自身的局域网 IP。

说明：

- 多台设备同时运行这个项目通常不会互相冲突，因为每台设备都有自己的 IP。
- 只有同一台机器上重复占用相同端口时，实例才会冲突。

## 配置说明

主要运行配置文件：

- `server/macchina.properties`
- `server/macchina.properties.install`

当前配置里已经包含：

- OSP bundle 仓库路径
- Web session 配置
- 简单认证配置
- 模拟传感器配置
- WebTunnel 相关预留配置

如果你准备把它当模板复用，最先建议调整这些内容：

- 管理员密码
- bundle 仓库路径
- 设备相关默认配置
- HTTP 监听地址和端口

## Web UI 说明

当前 Web UI 使用的是：

- `Vue 3`
- `Vite`
- `Vuetify`

每个 UI bundle 都有自己的前端目录，例如：

- `webUI/Launcher/ui`
- `webUI/Home/ui`
- `webUI/BundleList/ui`

如果只改前端页面，重新构建对应 bundle 即可。

## 适合作为模板的地方

这个仓库已经具备一套完整的“边缘服务模板”基础设施：

- 有宿主进程
- 有插件化模块机制
- 有 Web 登录与主页
- 有运行时诊断页面
- 有第三方依赖自动引导
- 有比较明确的输出目录结构

比较适合继续扩展的方向：

- 设备接入网关
- 传感器数据采集
- Modbus / MQTT / OPC UA / 串口协议适配
- 边缘侧规则处理
- 本地缓存与断点续传
- 云边协同控制台

## 当前仓库状态

这个仓库已经被整理为适合发布到 GitHub 的项目模板形式：

- 已初始化 git 仓库
- 已加入顶层 `.gitignore`
- 已忽略 `build/`、`node_modules/`、前端 `dist/` 等生成物

如果后面发布为 GitHub Template Repository，就可以直接用于新项目派生。
