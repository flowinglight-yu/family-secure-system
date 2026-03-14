#include "historypage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

/*
 * 模块说明：历史页实现。
 * 设计边界：页面只负责图表展示和窗口切换按钮，样本缓存与窗口过滤仍由 MainWindow 决定。
 */

HistoryPage::HistoryPage(QWidget *parent)
    : QWidget(parent),
      tempHumiChart(new TrendChartWidget(this)),
      smokeChart(new TrendChartWidget(this)),
      range1hButton(new QPushButton(QStringLiteral("最近1小时"), this)),
      range24hButton(new QPushButton(QStringLiteral("最近24小时"), this))
{
    buildUi();
    setWindowHours(1);
}

/* 同步窗口按钮勾选状态，不在页面内做数据重算。 */
void HistoryPage::setWindowHours(int hours)
{
    const bool isRecentHour = (hours <= 1);
    range1hButton->setChecked(isRecentHour);
    range24hButton->setChecked(!isRecentHour);
}

/* 注入温湿度曲线及其横轴配置。 */
void HistoryPage::setTempHumiSeries(const QVector<TrendChartWidget::Series> &series,
                                    const QStringList &axisLabels,
                                    int maxPoints)
{
    tempHumiChart->setMaxPoints(maxPoints);
    tempHumiChart->setXAxisLabels(axisLabels);
    tempHumiChart->setSeriesData(series);
}

/* 注入烟雾曲线及其横轴配置。 */
void HistoryPage::setSmokeSeries(const QVector<TrendChartWidget::Series> &series,
                                 const QStringList &axisLabels,
                                 int maxPoints)
{
    smokeChart->setMaxPoints(maxPoints);
    smokeChart->setXAxisLabels(axisLabels);
    smokeChart->setSeriesData(series);
}

/*
 * 历史页自适配。
 * 说明：统一调整图表区域间距、窗口按钮高度和标题行高度，保持双图布局整齐。
 */
void HistoryPage::applyAdaptiveMetrics(int spacing, int primaryButtonHeight)
{
    auto setSpacingByName = [](QWidget *scope, const QString &name, int value) {
        if (scope == nullptr) {
            return;
        }
        if (QLayout *layout = scope->findChild<QLayout *>(name)) {
            layout->setSpacing(value);
        }
    };

    setSpacingByName(this, QStringLiteral("HistoryRootLayout"), spacing);
    setSpacingByName(this, QStringLiteral("HistoryChartRowLayout"), spacing);

    range1hButton->setFixedHeight(primaryButtonHeight);
    range24hButton->setFixedHeight(primaryButtonHeight);

    const QList<QLabel *> historyHeaderLabels = findChildren<QLabel *>(QStringLiteral("HistoryHeaderLabel"));
    for (int i = 0; i < historyHeaderLabels.size(); ++i) {
        historyHeaderLabels.at(i)->setFixedHeight(primaryButtonHeight);
    }
}

/* 历史页：左侧温湿度历史，右侧烟雾历史。 */
void HistoryPage::buildUi()
{
    setObjectName(QStringLiteral("HistoryPage"));
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setObjectName(QStringLiteral("HistoryRootLayout"));
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    QWidget *chartArea = new QWidget(this);
    chartArea->setObjectName(QStringLiteral("HistoryChartArea"));
    QHBoxLayout *chartRow = new QHBoxLayout(chartArea);
    chartRow->setObjectName(QStringLiteral("HistoryChartRowLayout"));
    chartRow->setContentsMargins(0, 0, 0, 0);
    chartRow->setSpacing(8);

    /* 左侧图表卡片：温湿度历史和窗口切换按钮。 */
    QFrame *tempCard = new QFrame(chartArea);
    tempCard->setObjectName(QStringLiteral("Card"));
    QVBoxLayout *tempLayout = new QVBoxLayout(tempCard);
    tempLayout->setContentsMargins(10, 10, 10, 10);
    tempLayout->setSpacing(6);
    QHBoxLayout *rangeLayout = new QHBoxLayout();
    range1hButton->setObjectName(QStringLiteral("PrimaryButton"));
    range24hButton->setObjectName(QStringLiteral("PrimaryButton"));
    range1hButton->setCheckable(true);
    range24hButton->setCheckable(true);

    QLabel *tempHistoryTitleLabel = new QLabel(QStringLiteral("温湿度历史曲线"), tempCard);
    tempHistoryTitleLabel->setObjectName(QStringLiteral("HistoryHeaderLabel"));
    tempHistoryTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    rangeLayout->addWidget(tempHistoryTitleLabel);

    QLabel *tempLegendLabel = new QLabel(QStringLiteral("温度"), tempCard);
    tempLegendLabel->setStyleSheet(QStringLiteral("color:#38bdf8;font-weight:600;"));
    QLabel *humiLegendLabel = new QLabel(QStringLiteral("湿度"), tempCard);
    humiLegendLabel->setStyleSheet(QStringLiteral("color:#34d399;font-weight:600;"));
    rangeLayout->addWidget(tempLegendLabel);
    rangeLayout->addWidget(humiLegendLabel);
    rangeLayout->addStretch();
    rangeLayout->addWidget(range1hButton);
    rangeLayout->addWidget(range24hButton);
    tempLayout->addLayout(rangeLayout);

    tempHumiChart->setMinimumHeight(220);
    tempLayout->addWidget(tempHumiChart);

    /* 右侧图表卡片：烟雾浓度历史走势。 */
    QFrame *smokeCard = new QFrame(chartArea);
    smokeCard->setObjectName(QStringLiteral("Card"));
    QVBoxLayout *smokeLayout = new QVBoxLayout(smokeCard);
    smokeLayout->setContentsMargins(10, 10, 10, 10);
    smokeLayout->setSpacing(6);
    QHBoxLayout *smokeHeaderLayout = new QHBoxLayout();
    smokeHeaderLayout->setContentsMargins(0, 0, 0, 0);
    smokeHeaderLayout->setSpacing(6);
    QLabel *smokeHistoryTitleLabel = new QLabel(QStringLiteral("烟雾浓度历史"), smokeCard);
    smokeHistoryTitleLabel->setObjectName(QStringLiteral("HistoryHeaderLabel"));
    smokeHistoryTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    smokeHeaderLayout->addWidget(smokeHistoryTitleLabel);
    smokeHeaderLayout->addStretch();
    smokeLayout->addLayout(smokeHeaderLayout);

    smokeChart->setMinimumHeight(220);
    smokeLayout->addWidget(smokeChart);

    connect(range1hButton, SIGNAL(clicked()), this, SIGNAL(window1hRequested()));
    connect(range24hButton, SIGNAL(clicked()), this, SIGNAL(window24hRequested()));

    chartRow->addWidget(tempCard, 1);
    chartRow->addWidget(smokeCard, 1);

    layout->addWidget(chartArea);
}