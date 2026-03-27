# 编译指南

## 环境要求

- Qt 6.11.0 (MinGW 64-bit)
- CMake 3.21+
- MinGW 13.1.0 (Qt 自带)
- Ninja 构建工具

## 输出目录规范

| 用途 | 目录 |
|------|------|
| 正式编译 | `build/` |
| 测试编译 | `build_test/` |

## 编译命令

### 测试编译

```bash
# 设置环境变量
export PATH="/d/Programs/Code/Qt/Tools/mingw1310_64/bin:$PATH"

# 创建目录并配置
mkdir -p build_test && cd build_test
cmake .. -G "Ninja" \
  -DCMAKE_PREFIX_PATH="D:/Programs/Code/Qt/6.11.0/mingw_64" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER="D:/Programs/Code/Qt/Tools/mingw1310_64/bin/g++.exe" \
  -DCMAKE_MAKE_PROGRAM="D:/Programs/Code/Qt/Tools/Ninja/ninja.exe"

# 构建
cmake --build .
```

### 正式编译

```bash
export PATH="/d/Programs/Code/Qt/Tools/mingw1310_64/bin:$PATH"

mkdir -p build && cd build
cmake .. -G "Ninja" \
  -DCMAKE_PREFIX_PATH="D:/Programs/Code/Qt/6.11.0/mingw_64" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER="D:/Programs/Code/Qt/Tools/mingw1310_64/bin/g++.exe" \
  -DCMAKE_MAKE_PROGRAM="D:/Programs/Code/Qt/Tools/Ninja/ninja.exe"

cmake --build .
```

## 部署 Qt 依赖

编译后的程序需要 Qt DLL 才能运行。使用 `windeployqt` 自动部署：

```bash
cd build  # 或 build_test
windeployqt qsing-box.exe
```

## 工具路径

| 工具 | 路径 |
|------|------|
| Qt | `D:/Programs/Code/Qt/6.11.0/mingw_64` |
| MinGW | `D:/Programs/Code/Qt/Tools/mingw1310_64` |
| CMake | `D:/Programs/Code/Qt/Tools/CMake_64/bin/cmake.exe` |
| Ninja | `D:/Programs/Code/Qt/Tools/Ninja/ninja.exe` |
| windeployqt | `D:/Programs/Code/Qt/6.11.0/mingw_64/bin/windeployqt.exe` |
