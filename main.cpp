#include "capturewidget.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    // 1. 开启高 DPI 缩放
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication a(argc, argv);

    // 设置应用名称（用于生成同名 ini 文件）
    a.setApplicationName("ScreenCapture");

    // 隐藏不退出
    a.setQuitOnLastWindowClosed(false);

    // 初始化截图核心类 (内部已封装了托盘、热键、设置等所有功能)
    CaptureWidget w;

    return a.exec();
}