# Red Eclipse 用户指南

## 1. 游戏简介

Red Eclipse 是一款面向现代时代的复古竞技场射击游戏。它融合了《Doom》、《Quake》、《Halo》、《Team Fortress》和《Mirror's Edge》等经典游戏的元素，提供快节奏的跑酷射击体验。

### 主要特点：
- **免费开源软件**：无内购或付费内容
- **跑酷玩法**：支持墙壁奔跑、加速、冲刺等技巧
- **丰富的游戏模式**：多种游戏模式和变体
- **内置编辑器**：支持在线协作创建高质量地图
- **跨平台支持**：支持 Windows 和 GNU/Linux

## 2. 游戏规则

### 2.1 主要游戏模式

Red Eclipse 提供了多种游戏模式，每种模式都有独特的规则和目标：

#### 死亡竞赛 (Deathmatch)
- **目标**：通过击杀其他玩家来增加分数
- **规则**：
  - 每击杀一名敌人获得分数
  - 死亡后在指定的出生点重生
  - 游戏时间结束时，分数最高的玩家获胜
- **变体**：
  - **角斗士模式 (Gladiator)**：在受限区域战斗，伤害有更强的击退效果
  - **老派模式 (Old School)**：每次击杀只获得1分

#### 夺旗模式 (Capture the Flag)
- **目标**：夺取敌方旗帜并带回己方基地得分
- **规则**：
  - 团队合作，保护己方旗帜并夺取敌方旗帜
  - 成功带回旗帜得1分
  - 得分最高的团队获胜
- **变体**：
  - **快速模式 (Quick)**：掉落的旗帜立即返回基地
  - **防守模式 (Defend)**：掉落的旗帜必须防守直到重置
  - **保护模式 (Protect)**：保护己方旗帜并持有敌方旗帜才能得分

#### 防守与控制 (Defend and Control)
- **目标**：占领并防守控制点来得分
- **规则**：
  - 占领地图上的控制点
  - 保持对控制点的控制来持续得分
  - 得分最高的团队获胜
- **变体**：
  - **快速模式 (Quick)**：控制点占领速度更快
  - **山丘之王 (King)**：保持在控制点内才能得分

#### 炸弹球 (Bomber Ball)
- **目标**：携带炸弹进入敌方球门得分
- **规则**：
  - 拾取地图上的炸弹
  - 将炸弹带入敌方球门得分
  - 得分最高的团队获胜
- **变体**：
  - **持有模式 (Hold)**：尽可能长时间持有炸弹来得分
  - **投篮模式 (Basket)**：将炸弹投入敌方球门得分
  - **攻防模式 (Assault)**：团队轮流进攻和防守

#### 速度跑 (Speedrun)
- **目标**：以最快的时间完成障碍赛道
- **规则**：
  - 穿过地图上的检查点
  - 以最快的时间完成赛道
  - 时间最短的玩家获胜
- **变体**：
  - **圈数模式 (Lapped)**：尽可能多圈地完成赛道
  - **耐力模式 (Endurance)**：全程不死亡完成赛道
  - **挑战模式 (Gauntlet)**：团队轮流挑战赛道

### 2.2 游戏变体 (Mutators)

游戏变体可以修改游戏规则，创造独特的游戏体验：

| 变体名称 | 描述 |
|---------|------|
| **FFA** | 自由对战，每个玩家为自己而战 |
| **Coop** | 合作模式，玩家对抗机器人 |
| **Instagib** | 一击必杀模式，任何伤害都能立即击杀 |
| **Medieval** | 中世纪模式，玩家只生成剑 |
| **Kaboom** | 炸弹模式，玩家只生成爆炸物 |
| **Duel** | 决斗模式，一对一战斗 |
| **Survivor** | 生存者模式，最后存活的玩家获胜 |
| **Classic** | 经典模式，武器必须从地图上收集 |
| **Onslaught** | 猛攻模式，一波接一波的敌人 |
| **Vampire** | 吸血鬼模式，造成伤害可以恢复生命值 |
| **Resize** | 大小变化模式，玩家大小随生命值变化 |
| **Hard** | 困难模式，雷达和生命恢复禁用 |
| **Arena** | 竞技场模式，玩家可以携带所有武器 |
| **Dark** | 黑暗模式，在黑暗中战斗 |

### 2.3 武器系统

游戏提供多种武器，每种武器有独特的属性：

1. **爪 (Claw)** - 高伤害狙击武器
2. **手枪 (Pistol)** - 中等伤害的副武器
3. **剑 (Sword)** - 近战武器
4. **霰弹枪 (Shotgun)** - 近距离高伤害
5. **冲锋枪 (SMG)** - 高射速中等伤害
6. **火焰喷射器 (Flamer)** - 持续伤害
7. **等离子枪 (Plasma)** - 能量武器
8. **电击枪 (Zapper)** - 电击效果
9. **步枪 (Rifle)** - 精确射击
10. **腐蚀器 (Corroder)** - 腐蚀伤害
11. **手榴弹 (Grenade)** - 投掷爆炸物
12. **地雷 (Mine)** - 放置爆炸物
13. **火箭 (Rocket)** - 火箭发射器
14. **机枪 (Minigun)** - 极高射速
15. **电锯 (Jetsaw)** - 近战高伤害
16. **日蚀 (Eclipse)** - 特殊武器
17. **近战 (Melee)** - 基础近战攻击

### 2.4 跑酷系统

Red Eclipse 的核心特色之一是跑酷系统：

- **墙壁奔跑**：靠近墙壁时可以在墙上奔跑
- **加速跳跃**：落地时精确跳跃可以获得加速
- **冲刺**：快速向前冲刺
- **滑行**：高速时滑行
- **翻跃**：翻越高墙
- **踩踏**：从高处落下时踩踏敌人

## 3. 如何运行游戏

### 3.1 系统要求

- **操作系统**：Windows 或 GNU/Linux
- **处理器**：支持 SSE 的现代处理器
- **内存**：至少 2GB RAM
- **显卡**：支持 OpenGL 3.0+ 的显卡
- **硬盘**：至少 2GB 可用空间

### 3.2 启动游戏

#### Windows 系统：

1. **双击启动脚本**：
   - 客户端：双击 `redeclipse.bat`
   - 服务器：双击 `redeclipse_server.bat`

2. **脚本说明**：
   - `redeclipse.bat` 会自动检测系统架构 (x86 或 amd64)
   - 尝试从 `bin/amd64/redeclipse.exe` 启动
   - 如果失败，尝试从 `bin/x86/redeclipse.exe` 启动

3. **可选参数**：
   - 设置 `REDECLIPSE_HOME` 环境变量指定配置目录
   - 设置 `REDECLIPSE_BINARY` 指定可执行文件名

#### Linux 系统：

1. **使用启动脚本**：
   ```bash
   ./redeclipse.sh
   ```

2. **或直接运行可执行文件**：
   ```bash
   ./bin/<架构>/redeclipse
   ```

### 3.3 从源代码编译

如果没有预编译的二进制文件，需要从源代码编译：

#### Windows 系统：

1. **安装编译工具**：
   - MinGW-w64 或 Visual Studio
   - CMake

2. **使用 Makefile**：
   ```bash
   cd src
   mingw32-make
   ```

3. **或使用 CMake**：
   ```bash
   cd src
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```

#### Linux 系统：

1. **安装依赖**：
   ```bash
   # Debian/Ubuntu
   sudo apt-get install build-essential cmake libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev

   # Fedora
   sudo dnf install gcc-c++ cmake SDL2-devel SDL2_image-devel SDL2_mixer-devel
   ```

2. **编译**：
   ```bash
   cd src
   make
   ```

3. **安装**：
   ```bash
   sudo make install
   ```

## 4. 游戏操作

### 4.1 基本控制

默认控制键位：

| 动作 | 按键 |
|-----|------|
| **移动** | |
| 前进 | W |
| 后退 | S |
| 左移 | A |
| 右移 | D |
| **视角** | |
| 鼠标移动 | 控制视角 |
| **射击** | |
| 主攻击 | 鼠标左键 |
| 次攻击 | 鼠标右键 |
| **特殊动作** | |
| 跳跃 | 空格键 |
| 蹲伏 | C |
| 冲刺 | Shift |
| **武器** | |
| 切换武器 | 数字键 1-0 |
| 上一武器 | Q |
| 下一武器 | E |
| 丢弃武器 | G |
| **其他** | |
| 聊天 | T |
| 团队聊天 | Y |
| 投票菜单 | V |
| 控制台 | ` |
| 暂停/菜单 | Esc |

### 4.2 高级操作

- **墙壁奔跑**：靠近墙壁并保持移动
- **加速跳跃**：落地瞬间按空格键
- **翻跃**：靠近矮墙时按跳跃键
- **滑行**：高速移动时按蹲伏键
- **踩踏攻击**：从高处落下时踩踏敌人

## 5. 代码实现概述

### 5.1 项目结构

```
base/
├── bin/              # 预编译二进制文件
├── config/           # 游戏配置文件
│   ├── announcer/    # 播音员配置
│   ├── comp/         # 组件配置
│   ├── fx/           # 特效配置
│   ├── glsl/         # GLSL 着色器配置
│   ├── map/          # 地图配置
│   ├── tool/         # 工具配置
│   └── ui/           # UI 配置
├── doc/              # 文档
├── src/              # 源代码
│   ├── engine/       # 引擎代码
│   ├── game/         # 游戏逻辑
│   ├── enet/         # 网络库
│   ├── include/      # 第三方库头文件
│   ├── shared/       # 共享代码
│   └── steam/        # Steam 集成
└── redeclipse.bat    # Windows 启动脚本
```

### 5.2 核心架构

#### 引擎层 (`src/engine/`)

引擎层负责底层系统：

1. **渲染系统** (`rendergl.cpp`, `rendermodel.cpp`, `renderparticles.cpp`)
   - OpenGL 渲染
   - 模型渲染
   - 粒子效果
   - 天空渲染

2. **物理系统** (`physics.cpp`)
   - 碰撞检测
   - 刚体物理
   - 角色移动

3. **网络系统** (`client.cpp`, `server.cpp`, `enet/`)
   - 客户端-服务器架构
   - 可靠 UDP 传输
   - 消息序列化

4. **资源管理** (`texture.cpp`, `model.h`, `sound.cpp`)
   - 纹理加载
   - 模型加载
   - 音频系统

5. **UI 系统** (`ui.cpp`, `menus.cpp`)
   - 界面渲染
   - 菜单系统
   - 控制台

#### 游戏层 (`src/game/`)

游戏层实现具体的游戏逻辑：

1. **游戏模式** (`gamemode.h`)
   - 定义所有游戏模式
   - 模式特定规则
   - 变体系统

2. **玩家系统** (`player.h`, `client.cpp`)
   - 玩家状态管理
   - 移动控制
   - 生命值和伤害

3. **武器系统** (`weapons.h`, `weapons.cpp`, `weapdef.h`)
   - 武器定义
   - 射击逻辑
   - 弹药管理

4. **AI 系统** (`ai.cpp`, `ai.h`)
   - 机器人行为
   - 路径寻找
   - 决策制定

5. **实体系统** (`entities.cpp`, `game.h`)
   - 游戏实体定义
   - 触发器
   - 传送门
   - 检查点

### 5.3 关键技术实现

#### 1. 变量和命令系统

Red Eclipse 使用自定义的变量和命令系统：

```cpp
// 变量定义
VAR(IDF_PERSIST, playerhealth, 1, 100, 1000);  // 整数变量
FVAR(IDF_PERSIST, movespeed, 0.1f, 1.0f, 10.0f); // 浮点变量
SVAR(IDF_PERSIST, playername, "Player");           // 字符串变量

// 带回调的变量
VARF(IDF_PERSIST, playerhealth, 1, 100, 1000, 
{
    setplayerhealth(playerhealth);
});

// 命令定义
COMMAND(IDF_CLIENT, say, "s", (char *text), 
{
    sendchat(text, SAY_MESSAGE);
});
```

#### 2. 游戏模式和变体系统

游戏模式使用枚举和宏定义：

```cpp
// 游戏模式枚举
#define G_ENUM(en, um) \
    en(um, Demo, DEMO) \
    en(um, Editing, EDITING) \
    en(um, Deathmatch, DEATHMATCH) \
    en(um, Capture the Flag, CAPTURE) \
    en(um, Defend and Control, DEFEND) \
    en(um, Bomber, BOMBER) \
    en(um, Speedrun, SPEEDRUN) \
    en(um, Maximum, MAX)
ENUM_DLN(G);

// 变体枚举
#define G_M_ENUM(en, um) \
    en(um, Free for All, FFA) \
    en(um, Coop, COOP) \
    en(um, Instagib, INSTAGIB) \
    // ... 更多变体
ENUM_DLN(G_M);
```

#### 3. 网络协议

使用自定义的网络消息系统：

```cpp
// 网络消息类型
enum
{
    N_CONNECT = 0, N_SERVERINIT, N_WELCOME, N_CLIENTINIT, 
    N_POS, N_SPHY, N_TEXT, N_COMMAND, N_GAMELOG, N_DISCONNECT,
    N_SHOOT, N_DESTROY, N_STICKY, N_SUICIDE, N_DIED, N_POINTS,
    // ... 更多消息类型
    NUMMSG
};

// 消息发送示例
packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
putint(p, N_SERVMSG);
sendstring("欢迎来到游戏！", p);
sendpacket(clientnum, 1, p.finalize());
```

#### 4. 武器定义系统

使用宏定义武器属性：

```cpp
#define WPVAR(IDF_GAMEMOD, 0, damage, 1, 1000, 
    50, 35, 100, 80, 25,    // claw, pistol, sword, shotgun, smg
    30, 40, 35, 80, 50,     // flamer, plasma, zapper, rifle, corroder
    150, 75, 200, 40, 200,  // grenade, mine, rocket, minigun, jetsaw
    250, 50);               // eclipse, melee
```

#### 5. 实体系统

游戏实体使用结构化定义：

```cpp
struct enttypes
{
    int type, priority, links, radius, usetype;
    int numattrs, palattr, modesattr, idattr, mvattr;
    int fxattr, yawattr, pitchattr;
    int canlink, reclink, canuse;
    bool noisy, syncs, resyncs, syncpos, synckin;
    const char *name, *displayname;
    const char *attrs[MAXENTATTRS];
};

// 实体类型示例
extern const enttypes enttype[] = {
    {
        WEAPON, 2, 59, 16, EU_ITEM,
        6, -1, 2, 4, 5, -1, -1, -1,
        0, 0,
        (1<<ENT_PLAYER)|(1<<ENT_AI),
        false, true, true, false, false,
        "weapon", "Weapon",
        { "type", "flags", "modes", "muts", "id", "variant" }
    },
    // ... 更多实体类型
};
```

### 5.4 配置系统

游戏使用 CubeScript 配置文件：

```
// config/defaults.cfg 示例

// 玩家设置
player_name "Player"
player_model 0
player_colour1 0xFFFFFF
player_colour2 0x000000

// 控制设置
bind W forward
bind S backward
bind A left
bind D right
bind SPACE jump
bind MOUSE1 attack
bind MOUSE1 attack2

// 视频设置
vid_fullscreen 0
vid_width 1920
vid_height 1080
vid_fsaa 4

// 音频设置
snd_master 100
snd_music 50
snd_effects 80
```

## 6. 地图编辑

Red Eclipse 包含强大的实时地图编辑器：

### 6.1 进入编辑模式

1. 在游戏中按 `E` 键或使用 `/edit` 命令
2. 或启动游戏时使用 `-e` 参数：
   ```bash
   redeclipse -e
   ```

### 6.2 基本编辑操作

| 操作 | 按键/命令 |
|-----|-----------|
| 选择方块 | 左键点击 |
| 添加方块 | 中键点击 |
| 删除方块 | 右键点击 |
| 编辑属性 | `E` 键 |
| 放置实体 | 选择实体类型后点击 |
| 保存地图 | `/savemap <mapname>` |
| 加载地图 | `/loadmap <mapname>` |

### 6.3 实体类型

编辑器支持多种实体类型：
- **玩家出生点** (Player Start)
- **武器** (Weapon)
- **传送门** (Teleport)
- **触发器** (Trigger)
- **推动器** (Pusher)
- **检查点** (Checkpoint)
- **灯光** (Light)
- **地图模型** (Map Model)
- **音效** (Sound)
- **粒子效果** (Particles)

## 7. 服务器配置

### 7.1 启动服务器

#### Windows：
```batch
redeclipse_server.bat
```

#### Linux：
```bash
./redeclipse_server.sh
```

### 7.2 服务器配置文件

服务器配置通常存储在 `servinit.cfg`：

```
// 基本设置
server_name "My Red Eclipse Server"
server_desc "欢迎来到我的服务器！"
server_port 28800

// 游戏设置
server_gamemode 2  // 0=Demo, 1=Editing, 2=Deathmatch, 3=Capture, 4=Defend, 5=Bomber, 6=Speedrun
server_mutators 0   // 变体掩码
server_timelimit 10  // 时间限制（分钟）
server_scorelimit 50 // 分数限制

// 玩家设置
server_maxclients 16
server_maxplayers 12
server_bots 4
server_botskill 50

// 管理员设置
server_adminpass "password123"
server_mastermode 0  // 0=Open, 1=Veto, 2=Locked, 3=Private, 4=Password

// 地图循环
server_maplist "complex, complex2, outpost, garden"
```

## 8. 常见问题

### Q1: 游戏无法启动？
- 检查是否安装了最新的显卡驱动
- 确保 OpenGL 3.0+ 支持
- 尝试以管理员权限运行
- 检查 `bin/` 目录下是否有正确的二进制文件

### Q2: 如何添加机器人？
- 在游戏中使用 `/addbot` 命令
- 或在服务器配置中设置 `server_bots`

### Q3: 如何创建自定义地图？
- 按 `E` 进入编辑模式
- 使用方块工具构建地形
- 放置玩家出生点和其他实体
- 使用 `/savemap <name>` 保存

### Q4: 如何连接到服务器？
- 在主菜单选择"在线游戏"
- 或使用 `/connect <server_ip>:<port>` 命令

### Q5: 如何更改控制设置？
- 在游戏中进入"设置" > "控制"
- 或直接编辑配置文件中的 `bind` 命令

## 9. 资源链接

- **官方网站**：https://www.redeclipse.net/
- **下载页面**：https://www.redeclipse.net/download
- **开发仓库**：https://www.redeclipse.net/devel
- **Discord 社区**：https://www.redeclipse.net/chat
- **论坛**：https://www.redeclipse.net/forums
- **安装指南**：https://www.redeclipse.net/docs/Install-Guide

## 10. 许可证

Red Eclipse 是自由开源软件，遵循 GPL 许可证。详细信息请查看 `doc/license.txt` 和 `doc/all-licenses.txt`。

游戏包含的第三方库许可证：
- SDL：查看 `bin/LICENSE.SDL.txt`
- Freetype：查看 `bin/LICENSE.FTL.txt`
- 其他库：查看 `bin/` 目录下的其他许可证文件
