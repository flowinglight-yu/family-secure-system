#include "settingspage.h"

#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>

/*
 * 模块说明：设置页实现。
 * 设计边界：页面内部只负责表单控件和事件转发，配置保存、亮度写系统节点和密码校验继续由 MainWindow 负责。
 */

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent),
      wifiInfoLabel(new QLabel(QStringLiteral("WiFi：--\nIP：--\n信号：--"), this)),
      cloudApiInfoLabel(new QLabel(QStringLiteral("OneNet API：初始化中..."), this)),
      tempThresholdEdit(new QLineEdit(this)),
      smokeThresholdEdit(new QLineEdit(this)),
      applyThresholdButton(new QPushButton(QStringLiteral("应用阈值"), this)),
      brightnessSlider(new QSlider(Qt::Horizontal, this)),
      oldPasswordEdit(new QLineEdit(this)),
      newPasswordEdit(new QLineEdit(this)),
      confirmPasswordEdit(new QLineEdit(this)),
      changePasswordButton(new QPushButton(QStringLiteral("修改开机密码"), this)),
      versionInfoLabel(new QLabel(QStringLiteral("版本：v1.0.0\n平台：i.MX6ULL"), this))
{
    buildUi();
}

/* 回填阈值输入框，确保构建后能接收 MainWindow 加载的配置。 */
void SettingsPage::setThresholdValues(int tempThresholdC, int smokeThresholdRaw)
{
    tempThresholdEdit->setText(QString::number(tempThresholdC));
    smokeThresholdEdit->setText(QString::number(smokeThresholdRaw));
}

/* 同步亮度滑杆值。 */
void SettingsPage::setBrightnessValue(int value)
{
    brightnessSlider->setValue(value);
}

/* 更新网络信息卡片文本。 */
void SettingsPage::setWifiInfoText(const QString &text)
{
    wifiInfoLabel->setText(text);
}

/* 更新云端 API 运行时摘要。 */
void SettingsPage::setCloudApiInfoText(const QString &text)
{
    cloudApiInfoLabel->setText(text);
}

/* 密码修改成功后清空输入框，避免密码继续停留在界面上。 */
void SettingsPage::clearPasswordInputs()
{
    oldPasswordEdit->clear();
    newPasswordEdit->clear();
    confirmPasswordEdit->clear();
}

/* 从输入框读取温度阈值。 */
int SettingsPage::tempThresholdValue(bool *ok) const
{
    return tempThresholdEdit->text().toInt(ok);
}

/* 从输入框读取烟雾阈值。 */
int SettingsPage::smokeThresholdValue(bool *ok) const
{
    return smokeThresholdEdit->text().toInt(ok);
}

/* 返回旧密码输入内容。 */
QString SettingsPage::oldPassword() const
{
    return oldPasswordEdit->text();
}

/* 返回新密码输入内容。 */
QString SettingsPage::newPassword() const
{
    return newPasswordEdit->text().trimmed();
}

/* 返回确认密码输入内容。 */
QString SettingsPage::confirmPassword() const
{
    return confirmPasswordEdit->text().trimmed();
}

/*
 * 设置页自适配。
 * 说明：统一调整双列卡片间距和两个主操作按钮高度，保持和主壳其他页面一致。
 */
void SettingsPage::applyAdaptiveMetrics(int spacing, int primaryButtonHeight)
{
    if (QGridLayout *settingsGrid = findChild<QGridLayout *>(QStringLiteral("SettingsGridLayout"))) {
        settingsGrid->setHorizontalSpacing(spacing);
        settingsGrid->setVerticalSpacing(spacing);
    }

    applyThresholdButton->setFixedHeight(primaryButtonHeight);
    changePasswordButton->setFixedHeight(primaryButtonHeight);
}

/* 设置页：滚动容器 + 双列卡片布局。 */
void SettingsPage::buildUi()
{
    setObjectName(QStringLiteral("SettingsPage"));
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setObjectName(QStringLiteral("SettingsRootLayout"));
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    /* 外层滚动区：避免固定分辨率下设置项被截断。 */
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setObjectName(QStringLiteral("SettingsScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    /* 双列网格：承载网络、阈值、API、密码、亮度和系统信息卡片。 */
    QWidget *content = new QWidget(scrollArea);
    content->setObjectName(QStringLiteral("SettingsContent"));
    QGridLayout *grid = new QGridLayout(content);
    grid->setObjectName(QStringLiteral("SettingsGridLayout"));
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);

    /* 网络信息卡片：显示网卡名、IP 和信号。 */
    QFrame *networkCard = new QFrame(content);
    networkCard->setObjectName(QStringLiteral("Card"));
    QVBoxLayout *networkLayout = new QVBoxLayout(networkCard);
    networkLayout->setContentsMargins(10, 10, 10, 10);
    networkLayout->setSpacing(6);
    networkLayout->addWidget(new QLabel(QStringLiteral("网络信息"), networkCard));
    networkLayout->addWidget(wifiInfoLabel);

    /* API 信息卡片：显示 OneNet 运行时参数和状态文本。 */
    QFrame *apiCard = new QFrame(content);
    apiCard->setObjectName(QStringLiteral("Card"));
    QVBoxLayout *apiLayout = new QVBoxLayout(apiCard);
    apiLayout->setContentsMargins(10, 10, 10, 10);
    apiLayout->setSpacing(6);
    apiLayout->addWidget(new QLabel(QStringLiteral("OneNet API 参数（硬编码）"), apiCard));
    cloudApiInfoLabel->setWordWrap(true);
    apiLayout->addWidget(cloudApiInfoLabel);

    /* 阈值卡片：配置温度阈值和烟雾阈值。 */
    QFrame *thresholdCard = new QFrame(content);
    thresholdCard->setObjectName(QStringLiteral("Card"));
    QFormLayout *thresholdLayout = new QFormLayout(thresholdCard);
    thresholdLayout->setContentsMargins(10, 10, 10, 10);
    thresholdLayout->setHorizontalSpacing(8);
    thresholdLayout->setVerticalSpacing(6);
    thresholdLayout->addRow(QStringLiteral("温度阈值(°C)"), tempThresholdEdit);
    thresholdLayout->addRow(QStringLiteral("烟雾阈值(raw)"), smokeThresholdEdit);
    thresholdLayout->addWidget(applyThresholdButton);

    /* 亮度卡片：通过滑杆调节系统背光。 */
    QFrame *displayCard = new QFrame(content);
    displayCard->setObjectName(QStringLiteral("Card"));
    QFormLayout *displayLayout = new QFormLayout(displayCard);
    displayLayout->setContentsMargins(10, 10, 10, 10);
    displayLayout->setHorizontalSpacing(8);
    displayLayout->setVerticalSpacing(6);
    brightnessSlider->setRange(20, 100);
    brightnessSlider->setValue(80);
    displayLayout->addRow(QStringLiteral("屏幕亮度"), brightnessSlider);

    /* 密码卡片：修改锁屏密码。 */
    QFrame *passwordCard = new QFrame(content);
    passwordCard->setObjectName(QStringLiteral("Card"));
    QFormLayout *passwordLayout = new QFormLayout(passwordCard);
    passwordLayout->setContentsMargins(10, 10, 10, 10);
    passwordLayout->setHorizontalSpacing(8);
    passwordLayout->setVerticalSpacing(6);
    oldPasswordEdit->setEchoMode(QLineEdit::Password);
    newPasswordEdit->setEchoMode(QLineEdit::Password);
    confirmPasswordEdit->setEchoMode(QLineEdit::Password);
    newPasswordEdit->setMaxLength(6);
    confirmPasswordEdit->setMaxLength(6);
    newPasswordEdit->setPlaceholderText(QStringLiteral("4-6位数字密码"));
    confirmPasswordEdit->setPlaceholderText(QStringLiteral("再次输入4-6位数字密码"));
    passwordLayout->addRow(QStringLiteral("旧密码"), oldPasswordEdit);
    passwordLayout->addRow(QStringLiteral("新密码"), newPasswordEdit);
    passwordLayout->addRow(QStringLiteral("确认密码"), confirmPasswordEdit);
    passwordLayout->addWidget(changePasswordButton);

    /* 系统信息卡片：显示版本和平台说明。 */
    QFrame *systemCard = new QFrame(content);
    systemCard->setObjectName(QStringLiteral("Card"));
    QVBoxLayout *systemLayout = new QVBoxLayout(systemCard);
    systemLayout->setContentsMargins(10, 10, 10, 10);
    systemLayout->setSpacing(6);
    systemLayout->addWidget(new QLabel(QStringLiteral("系统信息"), systemCard));
    systemLayout->addWidget(versionInfoLabel);

    connect(applyThresholdButton, SIGNAL(clicked()), this, SIGNAL(applyThresholdRequested()));
    connect(brightnessSlider, SIGNAL(valueChanged(int)), this, SIGNAL(brightnessChanged(int)));
    connect(changePasswordButton, SIGNAL(clicked()), this, SIGNAL(changePasswordRequested()));

    grid->addWidget(networkCard, 0, 0);
    grid->addWidget(thresholdCard, 0, 1);
    grid->addWidget(apiCard, 1, 0);
    grid->addWidget(passwordCard, 1, 1);
    grid->addWidget(displayCard, 2, 0);
    grid->addWidget(systemCard, 2, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    scrollArea->setWidget(content);
    layout->addWidget(scrollArea);
}