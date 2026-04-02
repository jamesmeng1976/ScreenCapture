#ifndef TOOLBARWIDGET_H
#define TOOLBARWIDGET_H

#include <QWidget>
#include <QHBoxLayout>
#include <QToolButton>

class ToolBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit ToolBarWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::ToolTip); // 悬浮且不抢焦点
        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(5);

        createButton("矩形", "rect");
        createButton("圆形", "ellipse");
        createButton("箭头", "arrow");
        createButton("画笔", "pen");
        layout->addSpacing(10);
        createButton("另存为", "save");
        createButton("取消", "cancel");

        setStyleSheet("ToolBarWidget { background-color: #f0f0f0; border: 1px solid #ccc; border-radius: 4px; } "
                      "QToolButton { padding: 4px; } "
                      "QToolButton:checked { background-color: #d0d0d0; }");
    }

signals:
    void actionTriggered(const QString &action);

private:
    void createButton(const QString &text, const QString &name) {
        QToolButton *btn = new QToolButton(this);
        btn->setText(text);
        // btn->setCheckable(true); // 如果你想做互斥按钮，可以开启
        connect(btn, &QToolButton::clicked, [this, name]() { emit actionTriggered(name); });
        layout()->addWidget(btn);
    }
};

#endif // TOOLBARWIDGET_H