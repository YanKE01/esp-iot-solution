# Sensor Hub + LP Core Technical Notes

这份文档记录 `sensor_hub + esp-amp + ESP32-C6 LP core` 这条路径已经验证过的实现方案，以及过程中踩过的坑。

适用目录：
- [sensor_hub_lp](/home/yanke/project/esp-iot-solution/examples/ulp/lp_cpu/sensor_hub_lp)
- [sensor_hub](/home/yanke/project/esp-iot-solution/components/sensors/sensor_hub)

## 目标

目标不是把现有 `sensor_hub` 整体搬到 `LP core`，而是：

- `HP` 侧继续使用 `sensor_hub` 作为统一管理和事件分发入口
- `LP` 侧只负责轻量采样和上报
- 用户在应用里只配置 `sensor_remote_config_t`
- 用户如果要扩展 `LP` 驱动，只在自己的 `LP` 代码里注册函数

## 最终采用的架构

### HP 侧

`HP` 侧保留 `sensor_hub` 主体能力：

- `iot_sensor_create_remote()`
- `iot_sensor_start()`
- `iot_sensor_stop()`
- `iot_sensor_handler_register()`
- `iot_sensor_remote_report_group()`

职责：

- 创建 remote sensor 实例
- 启动 `esp-amp`
- 加载并启动组件内嵌的 `LP subcore` 固件
- 下发 `START/STOP` 配置
- 接收 `LP` 上报的采样数据
- 转成 `sensor_hub` 事件再派发给应用 handler

### LP 侧

`LP` 侧只做：

- 等待 `HP` 发配置
- 根据 `dev_id` 找到驱动
- 调 `init/start/sample/stop`
- 把采样结果作为 `sensor_remote_amp_data_msg_t` 回传

`LP` 不承担：

- `sensor_hub` 事件系统
- 动态传感器发现
- 复杂业务逻辑
- 应用配置管理

## 为什么不能把 sensor_hub 直接跑在 LP 上

现有 `sensor_hub` 是 `HP/FreeRTOS` 组件，依赖：

- task
- timer
- event group
- `esp_event`

这些都不适合直接照搬到 `LP subcore`。

所以正确做法不是“把 `sensor_hub` 搬过去”，而是“让 `sensor_hub` 管理一个 remote backend”。

## 为什么 remote 接口不能继续走静态 detect 模型

当前本地传感器的核心机制是链接期确定驱动，再通过段扫描发现实现。

这个模型适合：

- 驱动和应用在同一个镜像
- 本地函数调用

不适合：

- 传感器逻辑在 `LP` 镜像
- `HP` 和 `LP` 之间通过消息通信
- 运行时由 `HP` 选择 `LP` 驱动

所以新增了 remote 路径，而不是强行复用本地 detect 机制。

## 当前接口设计

### 用户在 HP 侧的入口

用户只需要在 `HP` 主程序里创建 remote sensor：

```c
sensor_remote_config_t remote_cfg = {
    .dev_id = SENSOR_REMOTE_DEV_FAKE_HUMITURE,
    .name = "lp_fake_sht3x",
    .addr = 0x44,
    .type = HUMITURE_ID,
    .min_delay = 1000,
    .amp = {
        .enabled = true,
        .use_interrupt = true,
    },
};
```

然后正常：

```c
iot_sensor_create_remote(...)
iot_sensor_handler_register(...)
iot_sensor_start(...)
```

### 用户在 LP 侧的入口

用户在自己的 `LP` 文件里注册驱动：

```c
static const lp_remote_sensor_driver_t my_driver = {
    .dev_id = SENSOR_REMOTE_DEV_FAKE_HUMITURE,
    .name = "fake_humiture",
    .ops = &my_ops,
};

void sensor_hub_lp_user_init(void)
{
    ESP_ERROR_CHECK(lp_remote_sensor_register(&my_driver));
}
```

当前示例位置：
- [sensor_hub_lp.c](/home/yanke/project/esp-iot-solution/examples/ulp/lp_cpu/sensor_hub_lp/main/sensor_hub_lp.c)
- [main.c](/home/yanke/project/esp-iot-solution/examples/ulp/lp_cpu/sensor_hub_lp/lp/main.c)

## 为什么 LP 注册要放在单独的 lp/ 目录

这是构建边界决定的，不是接口故意做复杂。

- `main/sensor_hub_lp.c` 编进 `HP` 镜像
- `lp/main.c` 编进 `LP` 镜像

`LP` 驱动注册本质上是注册一个函数指针表，这些符号必须存在于 `LP` 最终 ELF 里。

如果把注册代码写在 `HP` 文件里，`LP` 固件根本看不到这些函数。

## 为什么没有让 HP 传寄存器脚本

这条路明确不推荐。

不要把主接口设计成：

- 设备地址
- 寄存器地址
- 读多少字节
- 写哪些寄存器

原因：

- 真实传感器不是简单的 `read reg/write reg`
- 会涉及初始化时序、WHO_AM_I、bitfield 配置、状态位、burst read、换算
- 如果把这些都放到 `HP -> LP` 协议里，接口会迅速失控

最终选择是：

- `HP` 只发语义配置和 `dev_id`
- 具体寄存器逻辑封装在 `LP` 驱动里

## 为什么做成手动注册，而不是自动注册

这次先选了手动注册机制，原因是首版更稳：

- 少碰链接脚本
- 少碰 section 自动收集
- 调试直接
- 更容易先跑通完整链路

当前公共接口：
- [lp_remote_sensor_driver.h](/home/yanke/project/esp-iot-solution/components/sensors/sensor_hub/include/lp_remote_sensor_driver.h)

后面如果需要，可以再演进成自动注册。

## 已验证通过的技术路径

### 1. 在 sensor_hub 组件内集成 AMP

已验证：

- `sensor_hub` 组件内部统一构建 `subcore`
- `HP` 启动时自动加载嵌入式 `LP` 固件
- 示例工程不需要单独维护一套旧式 subcore 工程

关键点：

- 组件 `CMakeLists.txt` 里调用 `esp_amp_add_subcore_project(...)`
- 仅在 `esp32c5/esp32c6` 且 `CONFIG_SENSOR_REMOTE_AMP_ENABLE=y` 时启用

### 2. 用户在 HP 侧只配置 sensor_remote_config_t

已验证：

- 应用层不需要手写 `esp_amp` 启动代码
- 应用层只配置 remote sensor 和 handler

### 3. LP 驱动由用户工程提供

已验证：

- 用户可以在示例 `lp/main.c` 里提供自己的 fake driver
- `sensor_hub` subcore 启动时会调用用户钩子 `sensor_hub_lp_user_init()`

### 4. HP -> LP 配置，LP -> HP 上报，HP 打印

已验证：

- `HP` 下发 `START`
- `LP` 根据 `dev_id` 找驱动
- `LP` 调 `sample()`
- `HP` 接收并通过 `sensor_hub` 事件回调打印

## 踩过的坑

### 1. ESP_AMP_PATH 没有兜底

现象：

- CMake 报错：`/components/esp_amp/include is not a directory`

原因：

- `ESP_AMP_PATH` 为空时，`esp_amp` 组件内部路径拼接失效

处理：

- 在示例顶层 `CMakeLists.txt` 给 `ESP_AMP_PATH` 提供默认值

### 2. LP 用户代码虽然编译了，但没真正进入子核镜像

现象：

- `SUB: unsupported dev_id=1`
- 看不到 `SUB: user registered fake driver ...`

原因：

- 用户 `lp` 组件是静态库
- 子核里已经存在一个弱符号 `sensor_hub_lp_user_init()`
- 链接器认为引用已经满足，不再从用户静态库里拉入真正实现

处理：

- 将示例 `lp` 组件设置为 `WHOLE_ARCHIVE`

结果：

- 用户 `lp/main.c` 被强制链接进子核 ELF
- 注册函数真正执行

### 3. fake driver 把 RPMsg 包头清掉了

现象：

- `LP` 能打印 `sample`
- `HP` 没有任何 `RX` 日志，也没有事件打印

原因：

- `send_sensor_data()` 先写了 `msg_type/seq`
- fake driver 的 `sample()` 又执行了 `memset(packet, 0, sizeof(*packet))`
- 导致 `msg_type` 被清零
- `HP` 收到包后不再识别为数据消息

处理：

- 去掉 fake driver 里的整包 `memset`
- 并把 `msg_type/seq` 放在 `sample()` 之后再次写入

### 4. LP 浮点 printf 不能直接相信

现象：

- `SUB: sample[fake_humiture] seq=0 temp=%f000 humi=%f0000`

原因：

- 子核环境下 `printf` 对浮点支持受限

处理：

- 调试日志里把浮点转成整数后再打印

这只是日志问题，不影响真实数据通过 RPMsg 传到 `HP`。

### 5. 不能把 LP 注册写进 HP 的 main 文件

现象：

- 一开始很容易以为可以直接在 `main/sensor_hub_lp.c` 里注册 LP 函数

原因：

- 这是 `HP` 源码，不会进 `LP` 镜像

结论：

- `LP` 驱动代码必须放在 `LP` 编译单元里
- 当前采用 `lp/` 目录是合理做法

## 当前示例的工作方式

### HP 侧

[sensor_hub_lp.c](/home/yanke/project/esp-iot-solution/examples/ulp/lp_cpu/sensor_hub_lp/main/sensor_hub_lp.c)

做的事情：

- 创建 remote sensor
- 注册 `sensor_hub` handler
- 启动 sensor
- 等待来自 `LP` 的温湿度事件

### LP 侧

[main.c](/home/yanke/project/esp-iot-solution/examples/ulp/lp_cpu/sensor_hub_lp/lp/main.c)

做的事情：

- 注册 fake humiture driver
- 在 `sample()` 中构造两条数据
  - `SENSOR_TEMP_DATA_READY`
  - `SENSOR_HUMI_DATA_READY`

### 框架层

- [sensor_remote_amp.c](/home/yanke/project/esp-iot-solution/components/sensors/sensor_hub/sensor_remote_amp.c)
- [main.c](/home/yanke/project/esp-iot-solution/components/sensors/sensor_hub/subcore/main/main.c)

做的事情：

- 启动 `esp-amp`
- 建立 RPMsg endpoint
- 下发 `START/STOP`
- 在 `HP` 和 `LP` 之间搬运数据

## 接真实传感器时的建议

建议直接按 fake driver 的骨架接一个真实 `LP` 驱动，比如：

- `SHT3X`
- `BH1750`

做法：

1. 在 `lp/main.c` 或独立 `lp_xxx.c` 里实现驱动
2. `init()` 里做寄存器初始化
3. `sample()` 里做寄存器读取、换算、填充 `packet`
4. 注册成一个新的 `dev_id`
5. `HP` 侧只改 `sensor_remote_config_t.dev_id`

不要做：

- 让 `HP` 下发寄存器脚本
- 把寄存器细节暴露到 `sensor_remote_config_t`

## 当前状态

这条链路已经验证通过：

- 用户在 `HP` 配置 remote sensor
- 用户在 `LP` 注册驱动
- `HP` 启动 `LP`
- `LP` 上报假数据
- `HP` 通过 `sensor_hub` handler 打印结果

这已经可以作为后续接入真实 `LP` 传感器的基础模板。

## 2026-03 当前技术路线更新

下面这部分是当前仓库已经落地并验证通过的方案，用来覆盖前面那套旧的 `remote/RPMsg` 思路。

### 目标约束

当前方案遵循这几个约束：

- 不再引入 `remote` 框架
- 不再增加第二套 LP 注册接口
- `sensor_hub` 主框架尽量少改
- `LP` 路径只支持 `MODE_POLLING`
- `HP` 继续复用现有 `sensor_hub` 事件机制

### 当前总架构

现在采用的是“`LP` 采样，`HP` 桥接”的方式：

- `HP` 负责：
  - 创建 `enable_amp = true` 的 sensor
  - 初始化 `LP I2C`
  - 启动嵌入式 `LP subcore`
  - 接收 `LP` 上报的数据包
  - 转成 `sensor_data_t`
  - 调用现有 `sensors_event_post(...)`

- `LP` 负责：
  - 通过链接期机制注册传感器
  - 扫描 detect 表
  - 按 `min_delay` 轮询采样
  - 通过 `esp_amp_queue` 向 `HP` 发送数据包

也就是说，`HP` 不再自己做 LP proxy 的二次轮询，真正的采样权在 `LP`。

### AMP 构建方式

当前 AMP 构建链路是：

1. 用户工程在自己的 `main/CMakeLists.txt` 里引入 helper：

   ```cmake
   include(../../../sensor_hub/subcore/subcore_config.cmake)
   ```

2. 用户工程在 `idf_component_register(...)` 之后显式提供 LP component 目录：

   ```cmake
   sensor_hub_add_lp_subcore("${CMAKE_CURRENT_LIST_DIR}/../lp")
   ```

3. `sensor_hub_add_lp_subcore(...)` 会：
   - 通过 `EXTRA_CMAKE_ARGS` 把 `SENSOR_HUB_LP_COMPONENT_DIR` 传给 subcore 工程
   - 调 `esp_amp_add_subcore_project(... EMBED)` 构建并嵌入 LP 固件

4. subcore 工程只编两类代码：
   - 用户提供的 `LP` component 目录
   - `sensor_hub/subcore/main`

5. 生成出的 `subcore_sensor_hub_lp_detect.bin` 再嵌回主工程

当前相关文件：

- [subcore_config.cmake](/home/yanke/project/esp-iot-solution/components/sensors/sensor_hub/subcore/subcore_config.cmake)
- [subcore/CMakeLists.txt](/home/yanke/project/esp-iot-solution/components/sensors/sensor_hub/subcore/CMakeLists.txt)
- [test_apps/main/CMakeLists.txt](/home/yanke/project/esp-iot-solution/components/sensors/sensor_hub/test_apps/main/CMakeLists.txt)

### 为什么 `SENSOR_HUB_LP_DETECT_FN` 不需要 `linker.lf`

`HP` 现有的 `SENSOR_HUB_DETECT_FN` 依赖 `linker.lf + SURROUND(...)` 生成：

- `__sensor_hub_detect_fn_array_start`
- `__sensor_hub_detect_fn_array_end`

而 `LP` 这套 [sensor_hub_lp_detect.h](/home/yanke/project/esp-iot-solution/components/sensors/sensor_hub/include/sensor_hub_lp_detect.h) 直接把注册项放进：

- `section("sensor_hub_lp_detect")`

然后使用 GNU ld 自动提供的：

- `__start_sensor_hub_lp_detect`
- `__stop_sensor_hub_lp_detect`

来遍历 detect 表。

所以：

- `HP` detect 仍然需要 `linker.lf`
- `LP` detect 当前不需要 `linker.lf`

当前 `LP` 链接时会看到 orphan section warning，但功能上已经验证成立。

### `enable_amp` 路径

当前 `sensor_config_t` 已增加：

- `enable_amp`
- `lp_i2c_clk_hz`
- `lp_i2c_sda_pullup_en`
- `lp_i2c_scl_pullup_en`

当用户：

```c
sensor_config_t cfg = {
    .enable_amp = true,
    .type = HUMITURE_ID,
    .mode = MODE_POLLING,
    .addr = 0x68,
    .min_delay = 100,
};
```

调用 `iot_sensor_create()` 时：

- 如果 `enable_amp = true` 且不是 `MODE_POLLING`，直接返回 `ESP_ERR_NOT_SUPPORTED`
- `sensor_hub` 内部会自动：
  - 初始化 `LP I2C`
  - 启动 `LP subcore`
  - 分配 `amp slot`
  - 建立 `slot -> sensor` 映射

用户不需要自己手动调用 `start_lp_subcore_if_needed()`。

### HP / LP 的 I2C 分工

这里要特别注意：

- `lp_core_i2c_master_init()` 必须在 `HP` 侧调用
- `LP` 侧只能用 `ulp_lp_core_i2c_*` 访问设备

这符合 IDF 本身的 LP I2C 设计：

- `HP` 负责初始化 LP I2C 外设
- `LP` 负责实际读写寄存器

当前实现已经按这个边界收口：

- `HP`：在 `enable_amp` 路径里做 `lp_core_i2c_master_init()`
- `LP`：在用户 `lp/main.c` 里用 `lp_core_i2c_master_write_read_device()` 采样

### 当前真实 LP 读取示例

当前 [test_apps/lp/main.c](/home/yanke/project/esp-iot-solution/components/sensors/sensor_hub/test_apps/lp/main.c) 已经改成真实寄存器读取示例：

- I2C 地址：`0x68`
- 温度寄存器：`0x05`
- 湿度寄存器：`0x08`
- 各读 1 字节

读取方式是：

- `write_read_device()` 先写寄存器地址
- 再读 1 字节
- 读到的原始值直接转成 `float` 上报

如果后面要接真实量产驱动，通常还需要在 `init()` 里补：

- 上电初始化
- 模式配置
- 触发测量
- 数据换算

### 当前状态

当前已经验证通过的链路是：

- 用户在 `main/CMakeLists.txt` 显式提供 LP component 目录
- `SENSOR_HUB_LP_DETECT_FN` 在 `LP` 链接阶段完成注册
- `enable_amp` 路径下 `sensor_hub` 自动初始化 `LP I2C`
- `sensor_hub` 自动加载并启动嵌入式 subcore
- `LP` 轮询并通过 `esp_amp_queue` 上报数据
- `HP` 收包后直接桥接到现有 `sensor_hub` event loop
- 用户注册的 `sensor_event_handler` 可以无感收到 LP 数据事件

这就是当前应继续演进的主线方案。
