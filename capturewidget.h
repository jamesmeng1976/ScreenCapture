#ifndef CAPTUREWIDGET_H
#define CAPTUREWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QList>
#include <QPair>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAbstractNativeEventFilter>
#include "ToolBarWidget.h"

// 多继承：兼具窗口功能与截获底层 Windows 消息的功能
class CaptureWidget : public QWidget, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit CaptureWidget(QWidget *parent = nullptr);
    ~CaptureWidget() override;

    void startCapture();
    void forceQuit();

protected:
    // QWidget 原生事件
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

    // QAbstractNativeEventFilter 底层事件 (拦截热键)
    // 注意：Qt 5 用 long *，Qt 6 用 qintptr *
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;

private slots:
    void onToolBarAction(const QString &action);
    void showHotkeyDialog(); // 新增：显示热键设置
    void showAboutDialog();  // 新增：显示关于窗口

private:
    // 状态机
    enum CaptureState { Init, Selecting, Selected };
    enum DrawMode { Mode_None, Mode_Pen, Mode_Rect, Mode_Ellipse, Mode_Arrow };

    CaptureState m_state;
    DrawMode m_currentMode;

    QPixmap m_bgPixmap;
    QPoint m_startPoint;
    QPoint m_endPoint;
    QRect m_selectedRect;

    // 工具栏
    ToolBarWidget *m_toolBar;
    void updateToolBarPosition();

    // 托盘与菜单
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    QAction *m_actionCapture;

    // 热键与配置
    int m_hotkeyMod;
    int m_hotkeyVk;
    void loadConfig(); // 加载 ini
    void registerGlobalHotkey(); // 注册当前热键

    // --- 绘图数据结构 ---
    QPoint m_drawStartPoint;
    QList<QList<QPoint>> m_lines;
    QList<QPoint> m_currentLine;
    QList<QRect> m_rects;
    QRect m_currentRect;
    QList<QRect> m_ellipses;
    QRect m_currentEllipse;
    QList<QPair<QPoint, QPoint>> m_arrows;
    QPair<QPoint, QPoint> m_currentArrow;

    // 工具函数
    QRect getRect(const QPoint &beginPoint, const QPoint &endPoint);
    void drawArrow(QPainter *painter, QPoint start, QPoint end);
};

#endif // CAPTUREWIDGET_H