#include "capturewidget.h"
#include <QApplication>
#include <QScreen>
#include <QPainter>
#include <QMouseEvent>
#include <QClipboard>
#include <QFileDialog>
#include <QDateTime>
#include <QMessageBox>
#include <QSettings>
#include <QDialog>
#include <QFormLayout>
#include <QComboBox>
#include <QDialogButtonBox>
#include <qmath.h>
#include <windows.h>

CaptureWidget::CaptureWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setMouseTracking(true);

    m_state = Init;
    m_currentMode = Mode_None;

    // 1. 初始化工具栏
    m_toolBar = new ToolBarWidget(this);
    m_toolBar->hide();
    connect(m_toolBar, &ToolBarWidget::actionTriggered, this, &CaptureWidget::onToolBarAction);

    // ================= 【关键修复】：顺序调整 =================
    // 2. 必须先构建系统托盘和菜单，确保 m_actionCapture 被实例化
    m_trayIcon = new QSystemTrayIcon(QIcon(":/logo.png"), this);
    m_trayIcon->setToolTip("截屏工具");

    m_trayMenu = new QMenu(this);
    m_actionCapture = m_trayMenu->addAction("开始截图");
    connect(m_actionCapture, &QAction::triggered, this, &CaptureWidget::startCapture);

    m_trayMenu->addSeparator();
    connect(m_trayMenu->addAction("设置热键..."), &QAction::triggered, this, &CaptureWidget::showHotkeyDialog);
    connect(m_trayMenu->addAction("关于"), &QAction::triggered, this, &CaptureWidget::showAboutDialog);

    m_trayMenu->addSeparator();
    connect(m_trayMenu->addAction("退出"), &QAction::triggered, this, &CaptureWidget::forceQuit);

    m_trayIcon->setContextMenu(m_trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason){
        if (reason == QSystemTrayIcon::DoubleClick) startCapture();
    });
    m_trayIcon->show();

    // 3. 然后再加载配置并注册热键（此时 registerGlobalHotkey 内部修改菜单文字就不会闪退了）
    loadConfig();
    qApp->installNativeEventFilter(this);
    registerGlobalHotkey();
}

// ================= 【增强功能】：主动生成 INI 文件 =================
void CaptureWidget::loadConfig()
{
    QString iniPath = qApp->applicationDirPath() + "/" + qApp->applicationName() + ".ini";
    QSettings settings(iniPath, QSettings::IniFormat);

    // 如果没有找到配置项，主动写入默认值并立即生成物理文件
    if (!settings.contains("Hotkey/Modifiers") || !settings.contains("Hotkey/VirtualKey")) {
        settings.setValue("Hotkey/Modifiers", MOD_ALT);
        settings.setValue("Hotkey/VirtualKey", 0x43); // 'C' 的虚拟键码
        settings.sync(); // 强制将配置立刻写入磁盘生成 .ini 文件
    }

    m_hotkeyMod = settings.value("Hotkey/Modifiers").toInt();
    m_hotkeyVk = settings.value("Hotkey/VirtualKey").toInt();
}

CaptureWidget::~CaptureWidget()
{
    // 退出时清理系统钩子
    UnregisterHotKey(NULL, 100);
    qApp->removeNativeEventFilter(this);
}

// ================= 配置与热键系统 =================

void CaptureWidget::registerGlobalHotkey()
{
    UnregisterHotKey(NULL, 100); // 先注销旧的
    bool ok = RegisterHotKey(NULL, 100, m_hotkeyMod, m_hotkeyVk);

    if (ok) {
        // 更新菜单提示
        QString modStr = (m_hotkeyMod & MOD_CONTROL) ? "Ctrl+" : "";
        modStr += (m_hotkeyMod & MOD_ALT) ? "Alt+" : "";
        modStr += (m_hotkeyMod & MOD_SHIFT) ? "Shift+" : "";
        m_actionCapture->setText(QString("开始截图 (%1%2)").arg(modStr).arg((char)m_hotkeyVk));
    } else {
        QMessageBox::warning(nullptr, "热键冲突", "快捷键被占用，请在托盘右键重新设置！");
    }
}

// 拦截系统底层的热键消息
bool CaptureWidget::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
    Q_UNUSED(eventType)
    Q_UNUSED(result)
    MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_HOTKEY && msg->wParam == 100) {
        startCapture();
        return true;
    }
    return false;
}

// ================= 对话框 =================

void CaptureWidget::showHotkeyDialog()
{
    QDialog dlg;
    dlg.setWindowTitle("快捷键设置");
    dlg.setFixedSize(250, 120);
    QFormLayout *layout = new QFormLayout(&dlg);

    // 修饰键下拉框
    QComboBox *modCombo = new QComboBox(&dlg);
    modCombo->addItem("Alt", MOD_ALT);
    modCombo->addItem("Ctrl", MOD_CONTROL);
    modCombo->addItem("Shift", MOD_SHIFT);
    modCombo->addItem("Ctrl + Alt", MOD_CONTROL | MOD_ALT);
    modCombo->setCurrentIndex(modCombo->findData(m_hotkeyMod));

    // 字母键下拉框
    QComboBox *keyCombo = new QComboBox(&dlg);
    for (char c = 'A'; c <= 'Z'; ++c) {
        keyCombo->addItem(QString(c), (int)c);
    }
    keyCombo->setCurrentIndex(keyCombo->findData(m_hotkeyVk));

    layout->addRow("组合键：", modCombo);
    layout->addRow("字母键：", keyCombo);

    QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(btnBox);
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        // 保存并生效
        m_hotkeyMod = modCombo->currentData().toInt();
        m_hotkeyVk = keyCombo->currentData().toInt();
        registerGlobalHotkey();

        QString iniPath = qApp->applicationDirPath() + "/" + qApp->applicationName() + ".ini";
        QSettings settings(iniPath, QSettings::IniFormat);
        settings.setValue("Hotkey/Modifiers", m_hotkeyMod);
        settings.setValue("Hotkey/VirtualKey", m_hotkeyVk);
    }
}

void CaptureWidget::showAboutDialog()
{
    QMessageBox::about(nullptr, "关于截屏工具",
                       "<h2>截屏工具 v1.0 作者：jamesmeng</h2>"
                       "<p>WeChat: 893126</p>"
                       "<p>基于 Qt 5 C++ 研发的跨平台截屏工具。</p>"
                       "<p>核心特性：</p>"
                       "<ul>"
                       "<li>完美兼容高分辨率(High DPI)与多屏幕</li>"
                       "<li>支持系统级无级截获、托盘常驻</li>"
                       "<li>包含框选、画笔、红框、箭头等多级标注工具</li>"
                       "</ul>");
}

// ================= 截图核心流程与绘制 =================
// （以下代码为您上一版调试完毕的业务代码，直接保留即可）

void CaptureWidget::startCapture()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        m_bgPixmap = screen->grabWindow(0);
        setGeometry(screen->geometry());
    }
    m_state = Init;
    m_currentMode = Mode_None;
    m_selectedRect = QRect();
    m_toolBar->hide();
    m_lines.clear(); m_currentLine.clear();
    m_rects.clear(); m_currentRect = QRect();
    m_ellipses.clear(); m_currentEllipse = QRect();
    m_arrows.clear(); m_currentArrow = qMakePair(QPoint(), QPoint());

    showFullScreen();
    activateWindow();
}

void CaptureWidget::forceQuit()
{
    qApp->quit();
}

// ... 请在此处原样粘贴之前的 onToolBarAction, updateToolBarPosition, mousePressEvent, mouseMoveEvent(带LeftButton判断版), mouseReleaseEvent, mouseDoubleClickEvent, keyPressEvent, paintEvent, getRect, drawArrow 等函数 ...

// ================= UI 响应与状态机 =================

void CaptureWidget::onToolBarAction(const QString &action)
{
    if (action == "rect") m_currentMode = Mode_Rect;
    else if (action == "ellipse") m_currentMode = Mode_Ellipse;
    else if (action == "arrow") m_currentMode = Mode_Arrow;
    else if (action == "pen") m_currentMode = Mode_Pen;
    else if (action == "cancel") {
        hide(); m_toolBar->hide();
    }
    else if (action == "save") {
        m_toolBar->hide(); // 隐藏工具栏防穿帮
        QString fileName = QFileDialog::getSaveFileName(this, "保存截图",
                                                        "Screenshot_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".png",
                                                        "Images (*.png *.jpg)");
        if (!fileName.isEmpty()) {
            this->grab(m_selectedRect).save(fileName); // 完美抓取
            hide();
        } else {
            m_toolBar->show(); // 取消保存则恢复工具栏
        }
    }
}

void CaptureWidget::updateToolBarPosition()
{
    if (m_selectedRect.isNull()) return;
    int margin = 5;
    int x = m_selectedRect.right() - m_toolBar->width();
    int y = m_selectedRect.bottom() + margin;

    // 防遮挡边缘检测
    if (y + m_toolBar->height() > this->height()) {
        y = m_selectedRect.bottom() - m_toolBar->height() - margin;
    }
    if (x < 0) x = margin;

    m_toolBar->move(x, y);
    m_toolBar->show();
}

// ================= 鼠标事件 (框选与绘图) =================

void CaptureWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        hide(); m_toolBar->hide(); return;
    }

    if (event->button() == Qt::LeftButton) {
        if (m_state == Init) {
            // 开始屏幕框选
            m_state = Selecting;
            m_startPoint = event->pos();
            m_endPoint = event->pos();
        } else if (m_state == Selected && m_selectedRect.contains(event->pos())) {
            // 开始在选区内绘图
            m_drawStartPoint = event->pos();
            if (m_currentMode == Mode_Pen) m_currentLine.append(event->pos());
            else if (m_currentMode == Mode_Rect) m_currentRect = QRect(m_drawStartPoint, m_drawStartPoint);
            else if (m_currentMode == Mode_Ellipse) m_currentEllipse = QRect(m_drawStartPoint, m_drawStartPoint);
            else if (m_currentMode == Mode_Arrow) m_currentArrow = qMakePair(m_drawStartPoint, m_drawStartPoint);
        }
    }
}

void CaptureWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_state == Selecting) {
        // 框选模式下，必然是按着左键拖拽的
        m_endPoint = event->pos();
        update();
    }
    else if (m_state == Selected && m_selectedRect.contains(event->pos())) {

        // 【核心修复】：必须确保鼠标左键是按下状态，才更新绘图形状！
        // 否则开启 setMouseTracking(true) 后，鼠标悬停也会乱画
        if (event->buttons() & Qt::LeftButton) {

            if (m_currentMode == Mode_Pen && !m_currentLine.isEmpty()) {
                m_currentLine.append(event->pos());
            } else if (m_currentMode == Mode_Rect) {
                m_currentRect = getRect(m_drawStartPoint, event->pos());
            } else if (m_currentMode == Mode_Ellipse) {
                m_currentEllipse = getRect(m_drawStartPoint, event->pos());
            } else if (m_currentMode == Mode_Arrow) {
                m_currentArrow.second = event->pos();
            }
            update();
        }
    }
}

void CaptureWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_state == Selecting) {
            // 结束屏幕框选
            m_selectedRect = getRect(m_startPoint, m_endPoint);
            if (m_selectedRect.width() > 10 && m_selectedRect.height() > 10) {
                m_state = Selected;
                updateToolBarPosition(); // 弹出工具栏
                m_currentMode = Mode_Rect; // 默认给个矩形模式
            } else {
                m_state = Init; // 太小判定为误触
            }
        } else if (m_state == Selected) {
            // 结束绘图，把当前图形固化到 List 里
            if (m_currentMode == Mode_Pen && !m_currentLine.isEmpty()) {
                m_lines.append(m_currentLine); m_currentLine.clear();
            } else if (m_currentMode == Mode_Rect && !m_currentRect.isNull()) {
                m_rects.append(m_currentRect); m_currentRect = QRect();
            } else if (m_currentMode == Mode_Ellipse && !m_currentEllipse.isNull()) {
                m_ellipses.append(m_currentEllipse); m_currentEllipse = QRect();
            } else if (m_currentMode == Mode_Arrow && m_currentArrow.first != m_currentArrow.second) {
                m_arrows.append(m_currentArrow); m_currentArrow = qMakePair(QPoint(), QPoint());
            }
        }
        update();
    }
}

void CaptureWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_state == Selected && m_selectedRect.contains(event->pos())) {
        m_toolBar->hide(); // 先隐藏工具栏，防止被截取
        QPixmap resultPixmap = this->grab(m_selectedRect); // 所见即所得神仙级截取
        QApplication::clipboard()->setPixmap(resultPixmap);
        hide();
    }
}

void CaptureWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hide(); m_toolBar->hide();
    }
}

// ================= 核心渲染引擎 =================

void CaptureWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing); // 抗锯齿开启

    // 1. 底图铺满 (无视物理/逻辑分辨率差异的绝对解法)
    painter.drawPixmap(rect(), m_bgPixmap);

    // 2. 半透明遮罩
    painter.fillRect(rect(), QColor(0, 0, 0, 120));

    // 计算当前该高亮的区域
    QRect currentRect;
    if (m_state == Selecting) currentRect = getRect(m_startPoint, m_endPoint);
    else if (m_state == Selected) currentRect = m_selectedRect;

    // 3. 掏空选区
    if (!currentRect.isNull()) {
        // 利用裁剪区域，把原图在选区里重新铺一遍
        painter.setClipRect(currentRect);
        painter.drawPixmap(rect(), m_bgPixmap);
        painter.setClipping(false);

        // 蓝框
        painter.setPen(QPen(QColor(30, 144, 255), 2));
        painter.drawRect(currentRect);
    }

    // 4. 绘制所有标注 (限制在选区内)
    painter.setClipRect(currentRect);
    painter.setPen(QPen(Qt::red, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    // 画笔
    for (const auto &line : m_lines) {
        for (int i = 0; i < line.size() - 1; ++i) painter.drawLine(line[i], line[i + 1]);
    }
    for (int i = 0; i < m_currentLine.size() - 1; ++i) painter.drawLine(m_currentLine[i], m_currentLine[i + 1]);

    // 矩形
    for (const QRect &r : m_rects) painter.drawRect(r);
    if (!m_currentRect.isNull()) painter.drawRect(m_currentRect);

    // 圆形
    for (const QRect &r : m_ellipses) painter.drawEllipse(r);
    if (!m_currentEllipse.isNull()) painter.drawEllipse(m_currentEllipse);

    // 箭头
    for (const auto &arr : m_arrows) drawArrow(&painter, arr.first, arr.second);
    if (m_currentArrow.first != m_currentArrow.second) drawArrow(&painter, m_currentArrow.first, m_currentArrow.second);
}

// ================= 工具函数 =================

QRect CaptureWidget::getRect(const QPoint &beginPoint, const QPoint &endPoint)
{
    return QRect(qMin(beginPoint.x(), endPoint.x()), qMin(beginPoint.y(), endPoint.y()),
                 qAbs(beginPoint.x() - endPoint.x()), qAbs(beginPoint.y() - endPoint.y()));
}

void CaptureWidget::drawArrow(QPainter *painter, QPoint start, QPoint end)
{
    painter->drawLine(start, end);
    // 简单箭头头部算法
    double angle = std::atan2(-end.y() + start.y(), end.x() - start.x());
    double arrowSize = 10.0;
    QPointF p1 = end - QPointF(sin(angle + M_PI / 3) * arrowSize, cos(angle + M_PI / 3) * arrowSize);
    QPointF p2 = end - QPointF(sin(angle + M_PI - M_PI / 3) * arrowSize, cos(angle + M_PI - M_PI / 3) * arrowSize);
    painter->setBrush(Qt::red);
    painter->drawPolygon(QPolygonF() << end << p1 << p2);
    painter->setBrush(Qt::NoBrush);
}