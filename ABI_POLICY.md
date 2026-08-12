# FFMedia SDK ABI Policy

## 版本轴

FFMedia 同时维护两个版本：

- SDK 版本使用 `major.minor.patch`，由 `FFMEDIA_SDK_VERSION` 和 CMake package version 表示。
- Module ABI 使用独立整数，由 `FFMEDIA_MODULE_ABI_VERSION`、
  `ffmedia_module_abi_version()` 和 `libff_media.so.<abi>` SONAME 表示。

SDK patch/minor 版本可以增加向后兼容的类、函数、参数和模块。只要已经发布的 C++ 二进制
接口仍兼容，Module ABI 和 SONAME 保持不变。删除或改变已发布虚函数、类布局、函数签名、
枚举底层类型、异常/所有权约定，或改变外部派生类所需的 protected 契约时，必须递增 Module
ABI 和 SONAME，并在发布说明中列出迁移方法。

## 受支持的公开面

稳定契约仅包括：

- SDK `include/ffmedia/` 中安装的声明；
- `FFMedia::FFMedia` 和兼容 target `FFMedia::ff_media`；
- CMake package、pkg-config 元数据及文档明确说明的运行期行为；
- 公开类的 public 接口，以及明确用于外部派生模块的 protected 接口。

源码树中未安装的头、私有成员、`Impl` 类型、内部符号、Demo/Test 辅助函数和未文档化行为
不属于兼容承诺。动态库中偶然可见但未在安装头中声明的符号也不是公开 ABI。

## C++ 工具链约束

SDK 使用 C++17。消费者必须匹配：

- AArch64 ELF ABI；
- GCC/libstdc++ ABI，尤其 `_GLIBCXX_USE_CXX11_ABI`；
- 不高于 `SDK_MANIFEST.txt` 所列目标环境可提供范围的 GLIBC、GLIBCXX 和 CXXABI；
- 与 SDK 一致的异常和 RTTI约定。

编译期头文件和运行期函数会检查 Module ABI 与 libstdc++ C++11 ABI。SONAME 不一致的
头文件和动态库不得组合使用。

## 每次发布必须通过

- ARM64 Release 构建及全部注册 CPU CTest；
- SONAME、Module ABI、libstdc++ ABI、AArch64 ELF 和 vtable 基线检查；
- 安装后外部工程配置、全公开头编译、链接和运行；
- 最终 SDK 根目录独立构建及 CPU CTest；
- 无构建目录 RPATH/RUNPATH、无源码绝对路径泄漏；
- 文件校验、符号链接清单和版本化归档校验；
- final 模式下源码树干净，HEAD 精确对应 `v<SDK_VERSION>` 标签。
