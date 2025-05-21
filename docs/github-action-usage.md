# GitHub Action 自动发布说明

本项目配置了GitHub Action自动构建和发布功能，当你推送以`v`开头的标签（如`v1.0.0`）时，会自动触发构建流程并创建Release。

## 自动发布流程

1. 推送标签触发构建
2. 自动构建Windows和Ubuntu版本
3. 创建GitHub Release并上传构建产物

## 使用方法

### 1. 更新版本号

在发布新版本前，请确保更新项目中的版本号。主要需要更新以下文件：

- `SimpleNote.pro` 文件中的 `VERSION` 变量
- `main.cpp` 文件中的应用程序版本设置

### 2. 创建并推送标签

本地创建标签并推送到GitHub仓库：

```bash
# 确保最新提交已拉取
git pull

# 创建标签（例如v1.0.0）
git tag v1.0.0

# 推送标签到远程仓库
git push origin v1.0.0
```

### 3. 自动构建过程

推送标签后，GitHub Action会自动执行以下操作：

1. 检出代码
2. 安装Qt和相关依赖
3. 编译项目
4. 打包程序
5. 创建Release并上传构建产物

### 4. 查看Release

构建完成后，你可以在GitHub仓库的Releases页面查看新创建的Release：

`https://github.com/[用户名]/SimpleNote/releases`

## 构建产物

每次发布会生成以下文件：

- `SimpleNote-[版本号]-windows.zip` - Windows版本的可执行程序包
- `SimpleNote-[版本号]-x86_64.AppImage` - Linux AppImage可执行文件
- `SimpleNote-[版本号]-ubuntu.tar.gz` - Ubuntu版本的压缩包

## 手动触发构建

如果需要手动触发构建，可以删除并重新创建标签：

```bash
# 删除本地标签
git tag -d v1.0.0

# 删除远程标签
git push --delete origin v1.0.0

# 重新创建并推送标签
git tag v1.0.0
git push origin v1.0.0
```
