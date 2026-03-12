// Copyright 2026 ModelViewer Project
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <QApplication>
#include "ui/MainWindow.h"

/// @brief ModelViewer 应用程序入口点
/// 
/// 这是 ModelViewer 3D 模型查看器的主函数，负责：
/// 1. 创建 Qt 应用程序实例
/// 2. 创建并显示主窗口
/// 3. 启动事件循环
/// 
/// 应用程序特性：
/// - 基于 Qt6 框架构建
/// - 支持跨平台运行（Windows、macOS、Linux）
/// - 使用 RHI 进行硬件加速渲染
/// - 支持多种 3D 模型格式
/// 
/// 命令行参数：
/// - argc：命令行参数个数
/// - argv：命令行参数数组
/// 
/// @param[in] argc 命令行参数个数
/// @param[in] argv 命令行参数数组
/// @return int 应用程序退出码，0 表示正常退出
/// 
/// @note 返回值传递给操作系统，用于判断程序执行状态
/// @note 异常情况下会返回非零值
int main(int argc, char *argv[])
{
  // 创建 Qt 应用程序实例
  // QApplication 管理 GUI 应用程序的生命周期和事件处理
  QApplication app(argc, argv);
  
  // 创建主窗口实例
  // MainWindow 包含完整的用户界面和渲染功能
  MainWindow window;
  
  // 显示主窗口
  // 使窗口可见并进入正常的事件处理状态
  window.show();
  
  // 启动应用程序事件循环
  // app.exec() 会阻塞直到应用程序退出
  // 处理所有用户输入、定时器事件、网络事件等
  // 返回值为应用程序的退出码
  return app.exec();
}
