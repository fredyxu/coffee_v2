# Simulator

这个目录用于本地 UI 模拟器开发，目标是：
- 不改动现有 `main/` 固件代码路径
- 在 Mac 上先做 UI 迭代，再上真机验证

当前状态：
- 已支持在本机编译并运行 LVGL + SDL2 窗口
- 已接入项目内 `page_init` 页面（含 `add_init_msg` 演示）

## 已完成内容

- `simulator/CMakeLists.txt`
- `simulator/lv_conf.h`
- `simulator/src/main.c`
- `simulator/src/app_sim.c`
- `simulator/include/esp_err.h`（仅用于桌面模拟编译兼容）

## 本地使用

在仓库根目录执行：

```bash
cmake -S simulator -B simulator/build
cmake --build simulator/build -j 8
./simulator/build/coffee_sim
```

## 下一步建议

1. 在 `app_sim.c` 里增加模拟事件入口（例如“WiFi 断开/恢复”）
2. 用按键映射或定时器触发 `add_init_msg`，模拟状态刷新
3. 再逐步接入 `state` 输出到 `ui_actor` 的路径

## 约束

- `simulator/` 是独立实验区
- 默认不改动固件主编译入口
