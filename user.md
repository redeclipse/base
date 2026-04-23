# Red Eclipse 游戏项目文档

## 游戏概述

**Red Eclipse** 是一款面向现代的经典竞技场射击游戏，版本为 2.0.9 ("Big Bang Beta")。该游戏基于 Tesseract 和 Cube Engine 2 引擎开发，融合了从《Doom》、《Quake》、《Halo》、《Team Fortress》到《Mirror's Edge》等经典游戏的精髓元素。

### 主要特点

- **免费开源软件**: 无内购或其他付费要求，完全免费使用
- **跑酷元素**: 包含墙跑、加速、冲刺和其他技巧动作
- **丰富的游戏模式**: 多种流行游戏模式，支持多种变体和可配置变量
- **内置地图编辑器**: 所见即所得的实时编辑器，支持在线协作创建地图
- **跨平台支持**: 支持 Windows 和 GNU/Linux 系统

---

## 游戏规则

### 核心游戏模式

游戏提供了多种游戏模式，每种模式都有独特的玩法：

#### 1. 死亡竞赛 (Deathmatch)
- **描述**: 经典的"击杀即得分"模式
- **玩法**: 击杀其他玩家以增加分数
- **变体**:
  - **Gladiator**: 在有限区域内战斗，伤害带来更强的击退效果
  - **Old School**: 每次击杀只获得1分，类似传统竞技模式

#### 2. 夺旗 (Capture the Flag)
- **描述**: 团队协作模式，夺取敌方旗帜并带回基地
- **玩法**: 双方队伍各有一面旗帜，需要夺取对方旗帜并带回己方基地得分
- **变体**:
  - **Quick**: 掉落的旗帜立即返回基地
  - **Defend**: 掉落的旗帜需要防守直到重置
  - **Protect**: 保护己方旗帜并持有敌方旗帜来得分

#### 3. 防守与控制 (Defend and Control)
- **描述**: 争夺并防守控制点以获得分数
- **玩法**: 地图上有多个控制点，占领并保持控制即可持续得分
- **变体**:
  - **Quick**: 控制点占领速度更快
  - **King of the Hill**: 保持在控制点上以获得分数

#### 4. 炸弹球 (Bomber Ball)
- **描述**: 携带炸弹进入敌方球门得分
- **玩法**: 类似球类游戏，携带或投掷炸弹进入对方球门
- **变体**:
  - **Hold**: 尽可能长时间持有炸弹以获得分数
  - **Basket**: 投掷炸弹进入敌方球门得分
  - **Assault**: 队伍轮流进攻和防守

#### 5. 速通 (Speedrun)
- **描述**: 尝试以最快时间完成障碍赛道
- **玩法**: 在设计好的赛道上竞速，追求最快时间
- **变体**:
  - **Lapped**: 尝试完成最多圈数
  - **Endurance**: 必须全程不死亡才能完成
  - **Gauntlet**: 队伍轮流挑战障碍赛道

#### 6. 编辑模式 (Editing)
- **描述**: 创建和编辑地图
- **功能**: 完整的地图编辑功能，支持在线协作

#### 7. 演示模式 (Demo)
- **描述**: 回放之前录制的游戏
- **用途**: 观看精彩时刻或学习技巧

### 变体模式 (Mutators)

除了核心游戏模式外，游戏还支持多种变体模式，可以组合使用：

| 变体模式 | 描述 |
|---------|------|
| **FFA** | 自由对战，每个玩家为自己而战 |
| **Coop** | 合作模式，玩家对抗AI无人机 |
| **Instagib** | 一击必杀模式，所有武器秒杀 |
| **Medieval** | 中世纪模式，玩家只携带剑 |
| **Kaboom** | 爆炸模式，玩家只携带爆炸物 |
| **Duel** | 决斗模式，一对一战斗 |
| **Survivor** | 生存模式，玩家战斗到最后 |
| **Classic** | 经典模式，武器需要从地图上拾取 |
| **Onslaught** | 猛攻模式，一波波敌人涌入 |
| **Vampire** | 吸血鬼模式，造成伤害可恢复生命值 |
| **Resize** | 缩放模式，玩家大小随生命值变化 |
| **Hard** | 困难模式，禁用雷达和生命恢复 |
| **Arena** | 竞技场模式，可同时携带所有武器 |
| **Dark** | 黑暗模式，在黑暗中战斗 |

### 武器系统

游戏包含丰富的武器库，每种武器都有独特的特性：

#### 基础武器
- **Claw (爪)**: 近战武器
- **Pistol (手枪)**: 基础远程武器
- **Sword (剑)**: 强力近战武器

#### 主力武器
- **Shotgun (霰弹枪)**: 近距离高伤害
- **SMG (冲锋枪)**: 高射速，中等伤害
- **Flamer (火焰喷射器)**: 持续伤害，范围攻击
- **Plasma (等离子)**: 中等射速，能量武器
- **Zapper (电击枪)**: 快速射击，电击效果
- **Rifle (步枪)**: 高伤害，低射速
- **Corroder (腐蚀器)**: 持续腐蚀伤害

#### 特殊武器
- **Grenade (手榴弹)**: 投掷爆炸物
- **Mine (地雷)**: 放置后触发爆炸
- **Rocket (火箭筒)**: 高伤害火箭弹
- **Minigun (加特林)**: 极高射速
- **Jetsaw (喷射锯)**: 近战高伤害
- **Eclipse (日食)**: 超级武器

### 游戏机制

#### 跑酷系统
游戏特色之一是跑酷元素：
- **墙跑**: 沿着墙壁奔跑
- **加速**: 通过动作获得速度增益
- **冲刺**: 快速移动
- **二段跳**: 空中额外跳跃

#### 生命值和护甲
- 玩家有生命值和护甲值
- 正常模式下会自动恢复生命值
- **Hard** 模式下禁用自动恢复

#### 重生系统
- 死亡后会在特定重生点复活
- 有短暂的保护时间
- 重生点分布在地图各处

---

## 如何运行游戏

### 系统要求

- **操作系统**: Windows 7 或更高版本 / GNU/Linux
- **处理器**: 支持 SSE2 的处理器
- **内存**: 至少 2GB RAM
- **显卡**: 支持 OpenGL 2.0 或更高版本
- **存储**: 约 1GB 可用空间

### Windows 系统运行

#### 1. 直接运行（已编译版本）

如果您有已编译的游戏版本：

```batch
# 进入游戏目录
cd "f:\新建文件夹\怕？\base"

# 运行游戏客户端
redeclipse.bat

# 或运行专用服务器
redeclipse_server.bat
```

#### 2. 从源代码编译

如果需要从源代码编译：

```batch
# 进入源代码目录
cd src

# 使用 MinGW 编译
mingw32-make

# 或使用 CMake
cmake -B build
cmake --build build
```

### Linux 系统运行

#### 1. 直接运行（已编译版本）

```bash
# 进入游戏目录
cd "f:\新建文件夹\怕？\base"

# 运行游戏客户端
./redeclipse.sh

# 或运行专用服务器
./redeclipse_server.sh
```

#### 2. 从源代码编译

```bash
# 进入源代码目录
cd src

# 编译
make

# 或使用 CMake
cmake -B build
cmake --build build
```

### 启动参数

游戏支持多种命令行参数：

```batch
# 基本启动
redeclipse.bat

# 指定主目录
redeclipse.bat -h"游戏目录路径"

# 连接到特定服务器
redeclipse.bat -c服务器地址 -p端口

# 以全屏模式启动
redeclipse.bat -f1

# 以窗口模式启动
redeclipse.bat -f0

# 指定分辨率
redeclipse.bat -r1920x1080
```

### 常用游戏内命令

游戏内使用 `~` 键打开控制台，可以使用以下命令：

| 命令 | 描述 |
|------|------|
| `map 地图名` | 加载指定地图 |
| `mode 模式号` | 切换游戏模式 |
| `mutator 变体名` | 启用变体模式 |
| `addbot` | 添加机器人 |
| `edit` | 进入编辑模式 |
| `record 文件名` | 开始录制演示 |
| `demo 文件名` | 播放演示 |
| `connect 地址` | 连接到服务器 |
| `disconnect` | 断开连接 |
| `quit` | 退出游戏 |

---

## 游戏控制

### 默认键位配置

| 按键 | 功能 |
|------|------|
| **W** | 向前移动 |
| **S** | 向后移动 |
| **A** | 向左移动 |
| **D** | 向右移动 |
| **空格键** | 跳跃 |
| **鼠标左键** | 主攻击 |
| **鼠标右键** | 副攻击 |
| **鼠标中键** | 缩放 |
| **Q** | 切换武器（上一个）|
| **E** | 切换武器（下一个）|
| **R** | 重新加载 |
| **F** | 使用/投掷 |
| **Shift** | 冲刺/墙跑 |
| **Ctrl** | 下蹲 |
| **Tab** | 查看分数板 |
| **T** | 聊天 |
| **Y** | 团队聊天 |
| **C** | 语音命令 |
| **~** | 控制台 |
| **Esc** | 菜单 |

### 自定义控制

可以通过游戏内设置菜单或编辑配置文件来自定义控制：

1. 在游戏中按 `Esc` → **Settings** → **Controls**
2. 或编辑 `config/tool/binds/default.cfg` 文件

---

## 如何创建地图

Red Eclipse 内置了强大的实时地图编辑器。

### 进入编辑模式

```batch
# 启动游戏后进入编辑模式
# 或在游戏内使用命令
/edit

# 或加载现有地图进行编辑
/map 地图名
/edit
```

### 基本编辑操作

| 操作 | 描述 |
|------|------|
| **左键** | 选择/放置方块 |
| **右键** | 取消选择/删除 |
| **中键拖拽** | 旋转视角 |
| **滚轮** | 缩放 |
| **G** | 网格模式 |
| **V** | 视角切换 |
| **N** | 新建地图 |
| **S** | 保存地图 |
| **L** | 加载地图 |

### 地图编辑功能

编辑器提供以下功能：
- **几何编辑**: 八叉树式的立体几何编辑
- **纹理应用**: 多种纹理和材质
- **实体放置**: 玩家出生点、武器、控制点等
- **光照设置**: 动态光照和阴影
- **粒子效果**: 添加特效
- **合作编辑**: 多人在线协作编辑地图

---

## 代码实现概述

### 项目结构

```
base/
├── bin/              # 预编译的二进制文件
├── config/           # 配置文件
│   ├── ui/           # 界面配置
│   ├── glsl/         # 着色器配置
│   ├── fx/           # 特效配置
│   └── tool/         # 工具配置
├── doc/              # 文档
├── src/              # 源代码
│   ├── engine/       # 引擎核心代码
│   ├── game/         # 游戏逻辑代码
│   ├── shared/       # 共享代码
│   ├── enet/         # 网络库
│   ├── include/      # 第三方库头文件
│   └── steam/        # Steam 集成
└── readme.md         # 项目说明
```

### 核心架构

#### 1. 引擎层 (`src/engine/`)

引擎层负责底层系统：

- **渲染系统**: 
  - `rendergl.cpp`: OpenGL 渲染核心
  - `rendermodel.cpp`: 模型渲染
  - `renderlights.cpp`: 光照系统
  - `shader.cpp`: GLSL 着色器管理
  
- **世界系统**:
  - `octa.cpp`: 八叉树几何管理
  - `world.cpp`: 世界物理和逻辑
  - `physics.cpp`: 物理引擎
  
- **资源管理**:
  - `texture.cpp`: 纹理加载和管理
  - `sound.cpp`: 音频系统 (OpenAL)
  - `model.cpp`: 模型加载 (IQM, MD2, MD3, MD5, OBJ, SMD)

- **网络系统**:
  - `client.cpp`: 客户端网络
  - `server.cpp`: 服务器网络
  - 使用 ENet 库进行可靠 UDP 通信

#### 2. 游戏层 (`src/game/`)

游戏层实现具体游戏逻辑：

- **游戏模式**:
  - `gamemode.h`: 游戏模式和变体定义
  - `game.cpp`: 核心游戏逻辑
  - `capture.cpp`: 夺旗模式
  - `defend.cpp`: 防守模式
  - `bomber.cpp`: 炸弹模式

- **玩家系统**:
  - `player.h`: 玩家实体定义
  - `physics.cpp`: 玩家物理和跑酷
  - `ai.cpp`: 机器人 AI

- **武器系统**:
  - `weapons.h`: 武器定义
  - `weapons.cpp`: 武器逻辑
  - `projs.cpp`: 投射物系统

- **界面系统**:
  - `hud.cpp`: 抬头显示
  - `scoreboard.cpp`: 分数板

#### 3. 共享层 (`src/shared/`)

共享层提供通用功能：

- `command.h`: 变量和命令系统
- `cube.h`: 基础数学和工具函数
- `geom.h`: 几何计算
- `crypto.cpp`: 加密和认证

### 关键技术实现

#### 1. 变量和命令系统

游戏使用宏定义系统来管理配置变量：

```cpp
// 整数变量
VAR(IDF_PERSIST, playerhealth, 1, 100, 1000);

// 浮点数变量
FVAR(IDF_PERSIST, movespeed, 0.1f, 1.0f, 10.0f);

// 字符串变量
SVAR(IDF_PERSIST, playername, "Player");

// 命令
COMMAND(0, say, "s", (const char *text), {
    // 发送聊天消息
});
```

#### 2. 枚举系统

游戏使用宏生成枚举和辅助函数：

```cpp
// 游戏模式枚举
#define G_ENUM(en, um) \
    en(um, Demo, DEMO) en(um, Editing, EDITING) en(um, Deathmatch, DEATHMATCH)
ENUM_DLN(G);

// 自动生成:
// - G_DEMO, G_EDITING, G_DEATHMATCH 等常量
// - 辅助宏如 m_dm(), m_capture()
```

#### 3. 武器属性系统

使用宏批量定义武器属性：

```cpp
// 定义多种武器的伤害值
WPVAR(IDF_GAMEMOD, 0, damage, 1, 1000, 
    50, 35, 100, 80, 25,    // claw, pistol, sword, shotgun, smg
    30, 40, 35, 80, 50,     // flamer, plasma, zapper, rifle, corroder
    150, 75, 200, 40, 200,  // grenade, mine, rocket, minigun, jetsaw
    250, 50);               // eclipse, melee
```

#### 4. 八叉树几何

世界使用八叉树结构存储：

```cpp
// 八叉树节点
struct octa
{
    uchar edges[12];    // 边缘信息
    uchar children[8];  // 子节点索引
    ushort tex;         // 纹理索引
    uchar mat;          // 材质
    uchar blend;        // 混合值
};
```

#### 5. 网络通信

使用 ENet 进行可靠 UDP 通信：

```cpp
// 发送网络消息
packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
putint(p, N_POS);       // 消息类型
putint(p, clientnum);   // 客户端编号
putv(p, pos);           // 位置向量
sendpacket(clientnum, 1, p.finalize());

// 处理网络消息
void parsemessages(int cn, gameent *d, ucharbuf &p)
{
    while(p.remaining())
    {
        int type = getint(p);
        switch(type)
        {
            case N_POS:
                // 处理位置更新
                break;
            // ... 其他消息类型
        }
    }
}
```

#### 6. 配置系统

使用 CubeScript 语言进行配置：

```cubescript
// 执行配置文件
exec "config/keymap.cfg"
exec "config/engine.cfg"
exec "config/ui/package.cfg"

// 定义变量
var fov 0 75 180
fvar sensitivity 0.1 1.0 10.0

// 绑定按键
bind W [ forward 1 ]
bind S [ back 1 ]
bind MOUSE1 [ attack ]
```

### 编译系统

#### CMake 构建

```cmake
# src/CMakeLists.txt
project(redeclipse)

# 源文件
file(GLOB ENGINE_SOURCES engine/*.cpp)
file(GLOB GAME_SOURCES game/*.cpp)

# 可执行文件
add_executable(redeclipse ${ENGINE_SOURCES} ${GAME_SOURCES})

# 链接库
target_link_libraries(redeclipse
    SDL2::SDL2
    SDL2::SDL2_image
    OpenAL::OpenAL
    enet
    zlib
)
```

#### Makefile 构建

```makefile
# src/Makefile
CC = gcc
CXX = g++
CFLAGS = -O2 -Wall
CXXFLAGS = $(CFLAGS) -std=c++11

# 目标
all: redeclipse redeclipse_server

redeclipse: $(ENGINE_OBJS) $(GAME_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

redeclipse_server: $(ENGINE_OBJS) $(GAME_SERVER_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)
```

---

## 常见问题

### Q: 游戏无法启动？
- 检查显卡驱动是否最新
- 确保 OpenGL 2.0+ 支持
- 检查是否缺少 DLL 文件（Windows）
- 尝试使用窗口模式启动：`redeclipse.bat -f0`

### Q: 如何连接到服务器？
- 在主菜单选择 "Online"
- 或使用控制台命令：`connect 服务器地址`
- 默认端口：28801

### Q: 如何添加机器人？
- 使用命令：`addbot`
- 或在创建游戏时设置机器人数量

### Q: 如何录制和播放演示？
- 录制：`record demo_name`
- 停止录制：`stoprecord`
- 播放：`demo demo_name`

### Q: 如何获得更多帮助？
- 访问官方网站：https://www.redeclipse.net/
- 加入 Discord 社区
- 查看 `doc/` 目录下的详细文档

---

## 许可证信息

### 源代码许可证

Red Eclipse 基于 ZLIB 许可证发布：

```
Red Eclipse, Copyright (C) 2009-2025 Quinton Reeves, Lee Salzman, Sławomir Błauciak
Tesseract, Copyright (C) 2014-2019 Wouter van Oortmerssen, Lee Salzman, etc.
Cube Engine 2, Copyright (C) 2001-2019 Wouter van Oortmerssen, Lee Salzman, etc.
```

**ZLIB 许可证条款**：
1. 不得歪曲软件的原始来源
2. 更改后的源版本必须明确标记
3. 不得移除或更改此许可证声明

### 内容许可证

游戏内容（地图、纹理、声音、模型等）使用 CC-BY-SA 4.0 许可证：
- **BY (Attribution)**: 必须给予适当的署名
- **SA (Share-alike)**: 衍生作品必须使用相同的许可证

---

## 贡献

Red Eclipse 是一个由志愿者开发的开源项目。如果您想贡献：

1. 访问官方网站：https://www.redeclipse.net/
2. 加入 Discord 服务器参与讨论
3. 获取开发版本的 Git 仓库
4. 报告问题、提出建议或提交代码

### 活跃开发者
- Quinton "Quin" Reeves
- Lee "eihrul" Salzman
- Sławomir "Q009" Błauciak
- 以及其他众多贡献者

---

## 版本信息

- **游戏名称**: Red Eclipse
- **版本号**: 2.0.9
- **版本名称**: Big Bang Beta
- **Steam AppID**: 967460
- **官方网站**: www.redeclipse.net
- **版权年份**: 2009-2025
- **描述**: An arena shooter for the modern era

---

*此文档基于 Red Eclipse 2.0.9 版本编写*
