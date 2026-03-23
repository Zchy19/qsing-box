# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

qsing-box is a Windows GUI client for [sing-box](https://github.com/SagerNet/sing-box) proxy, developed using Qt6 C++. It supports Windows 10/11 x64.

## Build System

This project uses **CMake** (minimum version 3.21) with Qt6.

### Prerequisites

- Qt6 (Widgets, LinguistTools components)
- CMake 3.21+
- C++17 compatible compiler
- Windows SDK (for Windows-specific APIs)

### Build Commands

```bash
# Configure build
mkdir build && cd build
cmake ..

# Build
cmake --build . --config Release
# or
cmake --build . --config Debug
```

The build generates:
- `qsing-box.exe` - Main executable
- `.qm` translation files (embedded as resources)

### Adding Translations

Translation files are in `languages/` (.ts files). To update translations:

```bash
cd build
cmake --build . --target qsing-box_lupdate
```

## Architecture

The application follows a modular architecture with static libraries:

### Module Structure

```
src/
├── main.cpp              # Entry point, single-instance check, privilege elevation
├── main_window.*         # Main UI controller, coordinates all modules
├── about_dialog.*        # About dialog UI
├── settings_dialog.*     # Settings dialog UI
├── tray_icon.*           # System tray icon and menu
├── config/               # Config static library
│   ├── config.*          # Config data model (path, name)
│   ├── config_manager.*  # Config list management (add/edit/remove/switch)
│   └── config_editor.*   # Config editor dialog
├── proxy/                # Proxy static library
│   ├── proxy_manager.*   # sing-box process management (QProcess)
│   └── windows_proxy.*   # Windows system proxy settings
├── settings/             # Settings static library
│   ├── settings_manager.* # App settings persistence (QSettings)
│   ├── privilege_manager.* # UAC elevation handling
│   └── task_scheduler.*   # Windows Task Scheduler for auto-run
└── utils/                # Utils static library
    └── ansi_color_text.*  # ANSI color code parsing for log display
```

### Key Components

1. **MainWindow**: Central coordinator that connects ConfigManager, ProxyManager, and TrayIcon
2. **ConfigManager**: Manages sing-box configuration files, stores list in Windows registry via QSettings
3. **ProxyManager**: Spawns sing-box.exe as QProcess, handles process lifecycle and output
4. **WindowsProxy**: Interfaces with Windows API to set/clear system proxy settings
5. **TrayIcon**: QSystemTrayIcon wrapper with context menu for enable/disable proxy
6. **PrivilegeManager**: Handles UAC elevation for TUN mode support
7. **TaskScheduler**: Creates Windows scheduled task for boot-time auto-run

### Data Flow

```
User -> MainWindow -> ConfigManager/ProxyManager -> sing-box.exe
                          |                           |
                          v                           v
                    QSettings (registry)      Windows System Proxy
```

### Settings Storage

Application settings are stored in Windows registry under:
- `HKEY_CURRENT_USER\Software\NextIn\qsing-box`

Settings include:
- `configIndex` - Currently selected config index
- `lastOpenedFilePath` - Last directory for file dialogs
- `autoRun` - Boot-time auto-run enabled
- `runAsAdmin` - Always run with administrator privileges

### Auto-Run Implementation

Boot-time auto-run uses Windows Task Scheduler (not registry Run key) to support:
- Running with administrator privileges at boot
- The task XML template is in `resources/xml/task.xml`
- Passes `/autorun` argument to skip showing main window
- Uses 3-second delay before starting proxy (see `main.cpp:76`)

### Single Instance

Uses `QSharedMemory` with key "qsing-box" to prevent multiple instances.

## Development Notes

- **UI files**: `.ui` files are Qt Designer forms compiled to `ui_*.h` headers
- **Resources**: Qt resource system embeds images, translations, and XML files
- **C++ Standard**: C++17
- **Windows-specific**: Heavy use of Windows APIs (WinInet for proxy, COM for Task Scheduler)

## File Patterns

- Headers: `*.h`
- Sources: `*.cpp`
- UI forms: `*.ui`
- Translations: `languages/*.ts`
- Resources: `resources/images/*.png`, `resources/images/*.ico`
