# 常见问题

## Android 构建依赖失败

如果 Android arm64 构建在 glib、Meson 或安装阶段失败，先确认：

1. 使用仓库声明的 NDK、vcpkg manifest 和 overlay port。
2. 清理与目标 triplet 对应的失效二进制缓存后重新配置。
3. 检查 `vcpkg/ports/glib/portfile.cmake` 是否包含当前平台修复。
4. 不直接修改 vcpkg 下载目录中的第三方源码；临时环境修复不可作为项目配置提交。

## 构建缓存异常

源码改动应触发对应目标重新编译。若产物没有包含头文件改动，清理目标构建目录和 ccache 后重新配置；vcpkg 依赖缓存通常无需删除。

## Android 安装失败

确认设备满足 API 24、arm64-v8a 和 Vulkan 要求。覆盖安装时必须使用与旧包相同的签名；签名不同需要先卸载旧包。

## iOS 安装失败

iOS 产物通常为未签名 IPA，需要使用有效的 Apple 账户和签名工具重新签名。设备系统需满足项目最低版本要求。

## 黑屏或画面停帧

启用渲染探针并按 [渲染诊断](dev/rendering-diagnosis.md) 的顺序检查源纹理、重绘请求、事件投递和目标纹理。
