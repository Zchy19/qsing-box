# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

qsing-box is a Windows GUI client for [sing-box](https://github.com/SagerNet/sing-box) proxy, developed using Qt6 C++. It supports Windows 10/11 x64.

## Build System

This project uses **CMake** (minimum version 3.21) with Qt6 and Ninja.

### Prerequisites

- Qt6 (Widgets, Network, LinguistTools components)
- CMake 3.21+
- MinGW 13.1.0 (Qt bundled)
- Ninja build tool

### Build Commands

```bash
# Set environment
export PATH="/d/Programs/Code/Qt/Tools/mingw1310_64/bin:$PATH"

# Test build (output to build_test/)
mkdir -p build_test && cd build_test
cmake .. -G "Ninja" \
  -DCMAKE_PREFIX_PATH="D:/Programs/Code/Qt/6.11.0/mingw_64" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER="D:/Programs/Code/Qt/Tools/mingw1310_64/bin/g++.exe" \
  -DCMAKE_MAKE_PROGRAM="D:/Programs/Code/Qt/Tools/Ninja/ninja.exe"
cmake --build .

# Production build (output to build/)
mkdir -p build && cd build
# ... same cmake configuration ...
cmake --build .

# Deploy Qt dependencies for standalone execution
windeployqt qsing-box.exe
```

### Adding Translations

```bash
cd build
cmake --build . --target qsing-box_lupdate
```

## Architecture

The application follows a modular architecture with static libraries:

```
src/
├── main.cpp              # Entry point, single-instance, privilege elevation
├── main_window.*         # Main UI controller, coordinates all modules
├── about_dialog.*        # About dialog UI
├── settings_dialog.*     # Settings dialog UI
├── tray_icon.*           # System tray icon and menu
├── config/               # Config static library
│   ├── config.*          # Config data model
│   ├── config_manager.*  # Config list management (add/edit/remove/switch)
│   └── config_editor.*   # Config editor dialog
├── proxy/                # Proxy static library
│   ├── proxy_manager.*   # sing-box process management (QProcess)
│   └── windows_proxy.*   # Windows system proxy settings
├── settings/             # Settings static library
│   ├── settings_manager.*    # App settings persistence (QSettings)
│   ├── privilege_manager.*   # UAC elevation handling
│   └── task_scheduler.*      # Windows Task Scheduler for auto-run
├── subscription/         # Subscription static library
│   ├── subscription.*         # Subscription data model
│   ├── subscription_manager.* # Subscription CRUD operations
│   ├── subscription_downloader.* # Download configs from URLs
│   └── *_dialog.*             # Subscription UI dialogs
└── utils/                # Utils static library
    ├── ansi_color_text.* # ANSI color code parsing for log display
    └── file_logger.*     # File logging utility
```

### Key Components

1. **MainWindow**: Central coordinator connecting ConfigManager, ProxyManager, SubscriptionManager, and TrayIcon
2. **ConfigManager**: Manages sing-box configuration files, stores list in Windows registry
3. **ProxyManager**: Spawns sing-box.exe as QProcess, handles lifecycle and output
4. **WindowsProxy**: Windows API interface for system proxy settings
5. **TrayIcon**: QSystemTrayIcon with context menu, toggles window on click
6. **SubscriptionManager**: Manages subscription URLs and auto-updates configs

### Data Flow

```
User -> MainWindow -> ConfigManager/ProxyManager -> sing-box.exe
                          |                           |
                          v                           v
                    QSettings (registry)      Windows System Proxy
```

### Settings Storage

Application settings stored in Windows registry:
- `HKEY_CURRENT_USER\Software\NextIn\qsing-box`
- Keys: `configIndex`, `lastOpenedFilePath`, `autoRun`, `runAsAdmin`

### Auto-Run Implementation

Uses Windows Task Scheduler (not registry Run key) to support:
- Running with administrator privileges at boot
- Task XML template: `resources/xml/task.xml`
- Passes `/autorun` argument, 3-second delay before starting proxy

### Single Instance

Uses `QSharedMemory` + `QLocalServer`/`QLocalSocket`:
- First instance creates shared memory and local server
- Second instance connects to server, sends "SHOW" message, exits silently
- First instance receives message, shows and activates window

## Development Notes

- **C++ Standard**: C++17
- **UI files**: Qt Designer forms (`.ui`) compiled to `ui_*.h`
- **Resources**: Qt resource system embeds images, translations, XML, styles
- **Windows-specific**: WinInet for proxy, COM for Task Scheduler
- **Dark theme**: Stylesheet at `resources/styles/style.qss`

## File Patterns

- Headers: `*.h`
- Sources: `*.cpp`
- UI forms: `*.ui`
- Translations: `languages/*.ts`
- Resources: `resources/images/*`, `resources/styles/*`, `resources/xml/*`
