# Simulator

这个目录用于本地 UI 模拟器开发，目标是：
- 不改动现有 `main/` 固件代码路径
- 在 Mac 上先做 UI 迭代，再上真机验证

当前状态：
- 已支持在本机编译并运行 LVGL + SDL2 窗口
- 模拟器入口已改为调用正式 UI 入口 `ui_init()`（`main/modules/ui/ui.c`）

## 已完成内容

- `simulator/CMakeLists.txt`
- `simulator/lv_conf.h`
- `simulator/src/main.c`
- `simulator/src/app_sim.c`
- `simulator/src/sim_shim.c`（模拟器桩：`lvgl_port_run/ui_actor_init/log_output`）
- `simulator/include/esp_err.h`（桌面兼容）
- `simulator/include/esp_lcd_panel_io.h`（桌面兼容）
- `simulator/include/driver/ledc.h`（桌面兼容）

## 本地使用

在仓库根目录执行：

```bash
cmake -S simulator -B simulator/build
cmake --build simulator/build -j 8
./simulator/build/coffee_sim
```

或使用根目录脚本：

```bash
./sim
```

热更新（文件改动后自动重编译并重启模拟器）：

```bash
./sim-watch
```

## 外接显示器定位

默认窗口会放到 `SIM_DISPLAY_INDEX=1` 的显示器（通常是第一块外接屏）。

- 单次运行：
  ```bash
  SIM_DISPLAY_INDEX=1 ./sim
  ```
- 热更新运行：
  ```bash
  SIM_DISPLAY_INDEX=1 ./sim-watch
  ```

如果只有主屏或索引不对，改为 `0/2/...` 即可。

## 约束

- `simulator/` 是独立实验区
- 不改动固件主编译入口
