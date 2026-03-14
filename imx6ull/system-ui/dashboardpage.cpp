#include "dashboardpage.h"

#include "smokegaugewidget.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {

/*
 * 格式化界面显示数值。
 * 说明：保留限定小数位后去掉尾随 0，让 PPM 展示更紧凑。
 */
QString formatDisplayNumber(double value, int maxDecimals)
{
    QString text = QString::number(value, 'f', maxDecimals);
    while (text.contains('.') && (text.endsWith('0') || text.endsWith('.'))) {
        text.chop(1);
    }
    return text;
}

} // namespace

/*
 * 模块说明：首页仪表盘页面实现。
 * 设计边界：页面内部只处理四宫格展示与局部视觉状态，实时数据解析与阈值判断继续由 MainWindow 协调。
 */

DashboardPage::DashboardPage(QWidget *parent)
    : QWidget(parent),
      tempValueLabel(new QLabel(QStringLiteral("--.-°C"), this)),
      humiValueLabel(new QLabel(QStringLiteral("--%"), this)),
      tempSourceLabel(new QLabel(QStringLiteral("DHT11 传感器"), this)),
      tempOfflineMask(new QLabel(QStringLiteral("离线"), this)),
      smokeGauge(new SmokeGaugeWidget(this)),
      smokeValueLabel(new QLabel(QStringLiteral("--"), this)),
      smokeLevelLabel(new QLabel(QStringLiteral("安全"), this)),
      smokeWarnLabel(new QLabel(QStringLiteral("⚠"), this)),
      smokePpmLabel(new QLabel(QStringLiteral("PPM：--"), this)),
      smokeOfflineMask(new QLabel(QStringLiteral("离线"), this)),
      pirStateLabel(new QLabel(QStringLiteral("门外无人"), this)),
      pirLastSeenLabel(new QLabel(QStringLiteral("最后检测：--"), this)),
      pirOfflineMask(new QLabel(QStringLiteral("离线"), this)),
      espStateLabel(new QLabel(QStringLiteral("WIFI：离线"), this)),
      onetStateLabel(new QLabel(QStringLiteral("OneNet：离线"), this)),
      systemOfflineMask(new QLabel(QStringLiteral("离线"), this))
{
    buildUi();
}

/* 更新温湿度数值文本。 */
void DashboardPage::setTemperatureHumidity(int tempC, int humiPercent)
{
    tempValueLabel->setText(QStringLiteral("%1°C").arg(tempC));
    humiValueLabel->setText(QStringLiteral("%1%").arg(humiPercent));
}

/* 刷新温度来源说明。 */
void DashboardPage::setTemperatureSourceText(const QString &text)
{
    tempSourceLabel->setText(text);
}

/* 更新烟雾实时读数和仪表盘显示。 */
void DashboardPage::setSmokeReading(int smokeRaw, double smokePpm, int smokePercent)
{
    smokeValueLabel->setText(QStringLiteral("%1 raw").arg(smokeRaw));
    smokePpmLabel->setText(QStringLiteral("PPM：%1").arg(formatDisplayNumber(smokePpm, 2)));
    smokeGauge->setValue(smokePercent);
}

/* 刷新烟雾等级文案。 */
void DashboardPage::setSmokeLevelText(const QString &text)
{
    smokeLevelLabel->setText(text);
}

/* 控制烟雾警示图标闪烁显示。 */
void DashboardPage::setSmokeWarningVisible(bool visible)
{
    smokeWarnLabel->setVisible(visible);
}

/* 刷新 PIR 状态文案。 */
void DashboardPage::setPirStateText(const QString &text)
{
    pirStateLabel->setText(text);
}

/* 刷新 PIR 最后检测时间文案。 */
void DashboardPage::setPirLastSeenText(const QString &text)
{
    pirLastSeenLabel->setText(text);
}

/* 刷新 WiFi 状态文案。 */
void DashboardPage::setWifiStateText(const QString &text)
{
    espStateLabel->setText(text);
}

/* 刷新 OneNet 状态文案。 */
void DashboardPage::setOneNetStateText(const QString &text)
{
    onetStateLabel->setText(text);
}

/*
 * 首页离线状态控制。
 * 说明：离线时保留四宫格卡片骨架，只覆盖离线遮罩并隐藏烟雾仪表盘。
 */
void DashboardPage::setOfflineState(bool sensorOffline)
{
    const QList<QLabel *> masks = QList<QLabel *>()
            << tempOfflineMask
            << smokeOfflineMask
            << pirOfflineMask
            << systemOfflineMask;
    for (int i = 0; i < masks.size(); ++i) {
        masks.at(i)->setVisible(sensorOffline);
    }
    smokeGauge->setVisible(!sensorOffline);
}

/* 首页自适配：统一调整四宫格间距和烟雾仪表高度。 */
void DashboardPage::applyAdaptiveMetrics(int spacing, int smokeGaugeHeight)
{
    if (QGridLayout *dashboardGrid = findChild<QGridLayout *>(QStringLiteral("DashboardGridLayout"))) {
        dashboardGrid->setHorizontalSpacing(spacing);
        dashboardGrid->setVerticalSpacing(spacing);
    }

    smokeGauge->setFixedHeight(smokeGaugeHeight);
}

/* 仪表盘页：四宫格卡片（温湿度/烟雾/PIR/系统）。 */
void DashboardPage::buildUi()
{
    setObjectName(QStringLiteral("DashboardPage"));
    QGridLayout *grid = new QGridLayout(this);
    grid->setObjectName(QStringLiteral("DashboardGridLayout"));
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);

    /* 左上卡片：展示温度、湿度和数据来源。 */
    QFrame *cardTemp = new QFrame(this);
    cardTemp->setObjectName(QStringLiteral("Card"));
    cardTemp->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout *tempLayout = new QVBoxLayout(cardTemp);
    tempLayout->addWidget(new QLabel(QStringLiteral("温湿度监控"), cardTemp));
    tempValueLabel->setObjectName(QStringLiteral("ValueText"));
    humiValueLabel->setObjectName(QStringLiteral("ValueText"));
    tempLayout->addWidget(tempValueLabel);
    tempLayout->addWidget(humiValueLabel);
    tempSourceLabel->setObjectName(QStringLiteral("HintText"));
    tempLayout->addStretch();
    tempLayout->addWidget(tempSourceLabel);

    tempOfflineMask->setObjectName(QStringLiteral("OfflineMask"));
    tempOfflineMask->setAlignment(Qt::AlignCenter);
    tempOfflineMask->setVisible(false);
    tempLayout->addWidget(tempOfflineMask);

    /* 右上卡片：展示烟雾等级、仪表盘、raw 值和 PPM。 */
    QFrame *cardSmoke = new QFrame(this);
    cardSmoke->setObjectName(QStringLiteral("Card"));
    cardSmoke->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout *smokeLayout = new QVBoxLayout(cardSmoke);

    QLabel *smokeTitle = new QLabel(QStringLiteral("烟雾监控"), cardSmoke);
    smokeLayout->addWidget(smokeTitle);

    smokeValueLabel->setObjectName(QStringLiteral("ValueText"));
    smokeLevelLabel->setFont(smokeTitle->font());
    smokePpmLabel->setObjectName(QStringLiteral("ValueText"));
    smokeWarnLabel->setVisible(false);

    /* 等级行：标题下方直接显示烟雾安全等级和警示图标。 */
    QHBoxLayout *smokeStatusLayout = new QHBoxLayout();
    smokeStatusLayout->setContentsMargins(0, 0, 0, 0);
    smokeStatusLayout->setSpacing(6);
    smokeStatusLayout->addWidget(smokeLevelLabel);
    smokeStatusLayout->addWidget(smokeWarnLabel);
    smokeStatusLayout->addStretch();
    smokeLayout->addLayout(smokeStatusLayout);

    smokeGauge->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    smokeGauge->setFixedHeight(104);
    smokeLayout->addWidget(smokeGauge);
    smokeLayout->addStretch();

    /* 数值行：左侧 raw，右侧 PPM，形成一行对照。 */
    QHBoxLayout *smokeValueRowLayout = new QHBoxLayout();
    smokeValueRowLayout->setContentsMargins(0, 0, 0, 0);
    smokeValueRowLayout->setSpacing(10);
    smokeValueRowLayout->addWidget(smokeValueLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
    smokeValueRowLayout->addStretch();
    smokeValueRowLayout->addWidget(smokePpmLabel, 0, Qt::AlignRight | Qt::AlignVCenter);
    smokeLayout->addLayout(smokeValueRowLayout);

    smokeOfflineMask->setObjectName(QStringLiteral("OfflineMask"));
    smokeOfflineMask->setAlignment(Qt::AlignCenter);
    smokeOfflineMask->setFixedHeight(26);
    smokeOfflineMask->setVisible(false);
    smokeLayout->addWidget(smokeOfflineMask);

    /* 左下卡片：展示人体检测状态和最后检测时间。 */
    QFrame *cardPir = new QFrame(this);
    cardPir->setObjectName(QStringLiteral("Card"));
    cardPir->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout *pirLayout = new QVBoxLayout(cardPir);
    pirLayout->addWidget(new QLabel(QStringLiteral("门口人体检测"), cardPir));
    pirStateLabel->setObjectName(QStringLiteral("ValueText"));
    pirLayout->addWidget(pirStateLabel);
    pirLastSeenLabel->setObjectName(QStringLiteral("HintText"));
    pirLayout->addWidget(pirLastSeenLabel);
    pirLayout->addStretch();

    pirOfflineMask->setObjectName(QStringLiteral("OfflineMask"));
    pirOfflineMask->setAlignment(Qt::AlignCenter);
    pirOfflineMask->setVisible(false);
    pirLayout->addWidget(pirOfflineMask);

    /* 右下卡片：展示 WiFi、OneNet 和系统在线状态。 */
    QFrame *cardSystem = new QFrame(this);
    cardSystem->setObjectName(QStringLiteral("Card"));
    cardSystem->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout *sysLayout = new QVBoxLayout(cardSystem);
    sysLayout->addWidget(new QLabel(QStringLiteral("系统状态"), cardSystem));
    sysLayout->addWidget(espStateLabel);
    sysLayout->addWidget(onetStateLabel);
    sysLayout->addStretch();

    systemOfflineMask->setObjectName(QStringLiteral("OfflineMask"));
    systemOfflineMask->setAlignment(Qt::AlignCenter);
    systemOfflineMask->setVisible(false);
    sysLayout->addWidget(systemOfflineMask);

    grid->addWidget(cardTemp, 0, 0);
    grid->addWidget(cardSmoke, 0, 1);
    grid->addWidget(cardPir, 1, 0);
    grid->addWidget(cardSystem, 1, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);
}