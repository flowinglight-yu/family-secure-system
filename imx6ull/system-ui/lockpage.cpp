#include "lockpage.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

/*
 * 模块说明：锁屏页实现。
 * 设计边界：页面内部只处理输入反馈和局部动画，把密码校验和页面切换继续交给 MainWindow。
 */
namespace {

/*
 * 刷新控件样式状态。
 * 说明：当动态属性变化后，通过 unpolish/polish 让 QSS 重新生效。
 */
void refreshWidgetStyle(QWidget *widget)
{
    if (widget == nullptr) {
        return;
    }

    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

} // namespace

LockPage::LockPage(QWidget *parent)
    : QWidget(parent),
      lockHintLabel(new QLabel(this))
{
    buildUi();
}

/* 清空输入缓存并同步圆点。 */
void LockPage::resetInput()
{
    enteredPin.clear();
    updateDots();
}

/* 更新提示文本和错误态样式。 */
void LockPage::setHintText(const QString &text, bool error)
{
    lockHintLabel->setText(text);
    lockHintLabel->setProperty("error", error);
    refreshWidgetStyle(lockHintLabel);
}

/* 错误提示时执行短时左右抖动。 */
void LockPage::playErrorAnimation()
{
    QPropertyAnimation *animation = new QPropertyAnimation(this, "pos", this);
    const QPoint origin = pos();
    animation->setDuration(140);
    animation->setStartValue(origin + QPoint(-8, 0));
    animation->setKeyValueAt(0.5, origin + QPoint(8, 0));
    animation->setEndValue(origin);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

/*
 * 锁屏页自适配。
 * 说明：统一调整键盘高度、根布局边距和卡片留白，保持与原主窗口中的适配节奏一致。
 */
void LockPage::applyAdaptiveMetrics(int spacing, int compactSpacing, int sideMargin, int keypadButtonHeight)
{
    auto setSpacingByName = [](QWidget *scope, const QString &name, int value) {
        if (scope == nullptr) {
            return;
        }
        if (QLayout *layout = scope->findChild<QLayout *>(name)) {
            layout->setSpacing(value);
        }
    };

    auto setMarginsByName = [](QWidget *scope, const QString &name, int left, int top, int right, int bottom) {
        if (scope == nullptr) {
            return;
        }
        if (QLayout *layout = scope->findChild<QLayout *>(name)) {
            layout->setContentsMargins(left, top, right, bottom);
        }
    };

    const QList<QPushButton *> keypadButtons = findChildren<QPushButton *>(QStringLiteral("KeypadButton"));
    for (int i = 0; i < keypadButtons.size(); ++i) {
        keypadButtons.at(i)->setFixedHeight(keypadButtonHeight);
    }

    setSpacingByName(this, QStringLiteral("LockRootLayout"), spacing);
    setSpacingByName(this, QStringLiteral("LockCardLayout"), spacing);
    setSpacingByName(this, QStringLiteral("LockDotLayout"), compactSpacing);
    setSpacingByName(this, QStringLiteral("LockKeypadLayout"), spacing);

    const int lockSide = qBound(60, width() / 4, 220);
    const int lockTop = qBound(20, height() / 7, 80);
    setMarginsByName(this, QStringLiteral("LockRootLayout"), lockSide, lockTop, lockSide, lockTop);
    setMarginsByName(this,
                     QStringLiteral("LockCardLayout"),
                     sideMargin + 10,
                     sideMargin + 6,
                     sideMargin + 10,
                     sideMargin + 6);
}

/* 数字键点击后追加输入，并更新圆点反馈。 */
void LockPage::onDigitButtonClicked()
{
    QPushButton *button = qobject_cast<QPushButton *>(sender());
    if (button == nullptr || enteredPin.size() >= 6) {
        return;
    }

    enteredPin.append(button->property("digit").toString());
    updateDots();
}

/* 清除输入并恢复默认提示文本。 */
void LockPage::onClearButtonClicked()
{
    resetInput();
    setHintText(QStringLiteral("请输入 4-6 位密码"));
}

/* 确认键只负责把当前输入提交给外层。 */
void LockPage::onConfirmButtonClicked()
{
    emit confirmRequested(enteredPin);
}

/* 锁屏页：标题 + 密码圆点 + 数字键盘。 */
void LockPage::buildUi()
{
    setObjectName(QStringLiteral("LockPage"));

    QVBoxLayout *lockLayout = new QVBoxLayout(this);
    lockLayout->setObjectName(QStringLiteral("LockRootLayout"));
    lockLayout->setContentsMargins(220, 80, 220, 80);
    lockLayout->setSpacing(12);

    /* 居中卡片承载标题、输入圆点、数字键盘和提示信息。 */
    QFrame *card = new QFrame(this);
    card->setObjectName(QStringLiteral("LockCard"));
    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setObjectName(QStringLiteral("LockCardLayout"));
    cardLayout->setContentsMargins(24, 20, 24, 20);
    cardLayout->setSpacing(12);

    QLabel *title = new QLabel(QStringLiteral("🔒 家庭安防系统"), card);
    title->setObjectName(QStringLiteral("LockTitle"));
    title->setAlignment(Qt::AlignCenter);

    /* 圆点区：根据当前输入长度显示已输入位数。 */
    QHBoxLayout *dotLayout = new QHBoxLayout();
    dotLayout->setObjectName(QStringLiteral("LockDotLayout"));
    dotLayout->setAlignment(Qt::AlignCenter);
    dotLayout->setSpacing(8);
    for (int i = 0; i < 6; ++i) {
        QLabel *dot = new QLabel(QStringLiteral("○"), card);
        dot->setObjectName(QStringLiteral("LockDot"));
        lockDots.append(dot);
        dotLayout->addWidget(dot);
    }

    /* 数字键盘区：提供数字输入、清除和确认三类操作。 */
    QGridLayout *keypad = new QGridLayout();
    keypad->setObjectName(QStringLiteral("LockKeypadLayout"));
    keypad->setHorizontalSpacing(10);
    keypad->setVerticalSpacing(10);

    int number = 1;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            QPushButton *button = new QPushButton(QString::number(number), card);
            button->setObjectName(QStringLiteral("KeypadButton"));
            button->setProperty("digit", number);
            connect(button, SIGNAL(clicked()), this, SLOT(onDigitButtonClicked()));
            keypad->addWidget(button, row, col);
            number++;
        }
    }

    /* 底行固定保留清除、0 和确认按钮。 */
    QPushButton *clearButton = new QPushButton(QStringLiteral("清除"), card);
    QPushButton *zeroButton = new QPushButton(QStringLiteral("0"), card);
    QPushButton *confirmButton = new QPushButton(QStringLiteral("确认"), card);
    clearButton->setObjectName(QStringLiteral("KeypadButton"));
    zeroButton->setObjectName(QStringLiteral("KeypadButton"));
    confirmButton->setObjectName(QStringLiteral("PrimaryButton"));
    zeroButton->setProperty("digit", 0);

    connect(clearButton, SIGNAL(clicked()), this, SLOT(onClearButtonClicked()));
    connect(zeroButton, SIGNAL(clicked()), this, SLOT(onDigitButtonClicked()));
    connect(confirmButton, SIGNAL(clicked()), this, SLOT(onConfirmButtonClicked()));

    keypad->addWidget(clearButton, 3, 0);
    keypad->addWidget(zeroButton, 3, 1);
    keypad->addWidget(confirmButton, 3, 2);

    /* 提示区：显示默认提示或由外层传入的错误信息。 */
    lockHintLabel->setObjectName(QStringLiteral("LockHint"));
    lockHintLabel->setAlignment(Qt::AlignCenter);
    lockHintLabel->setText(QStringLiteral("请输入 4-6 位密码"));

    cardLayout->addWidget(title);
    cardLayout->addLayout(dotLayout);
    cardLayout->addLayout(keypad);
    cardLayout->addWidget(lockHintLabel);

    lockLayout->addStretch();
    lockLayout->addWidget(card);
    lockLayout->addStretch();
}

/* 按输入长度统一刷新圆点文本。 */
void LockPage::updateDots()
{
    for (int i = 0; i < lockDots.size(); ++i) {
        lockDots.at(i)->setText(i < enteredPin.size() ? QStringLiteral("●") : QStringLiteral("○"));
    }
}