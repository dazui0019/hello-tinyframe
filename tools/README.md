# Python 测试工具

在工程根目录运行以下命令。uv 会管理根目录的 `.venv`，无需手动激活环境。

```powershell
uv sync --locked
uv run python tools/uart_stress.py --list-ports
uv run python tools/uart_stress.py --port COM5 --baud 115200 --duration 60
```

运行压力测试前复位开发板，确认板端与电脑端波特率一致，并关闭占用该串口的其他程序。
脚本不要求固件回显；测试结束后等待接收处理完，再对照板端 `rx_stats`。

添加或删除依赖：

```powershell
uv add <包名>
uv remove <包名>
```

将 `pyproject.toml` 和 `uv.lock` 提交到 Git；`.venv` 和 `__pycache__` 不提交。
本配置只管理测试脚本；现有 pyOCD 烧录和调试任务仍使用原来的环境。
