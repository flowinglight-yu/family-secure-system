#ifndef HISTORYPAGE_H
#define HISTORYPAGE_H

/*
 * 模块说明：历史页声明。
 * 职责：负责历史图表布局、窗口按钮状态和图表数据注入，不持有历史缓存与筛选逻辑。
 */

#include "trendchartwidget.h"

#include <QStringList>
#include <QWidget>

class QPushButton;

class HistoryPage : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryPage(QWidget *parent = nullptr);

    /* 同步当前历史窗口按钮状态。 */
    void setWindowHours(int hours);
    /* 更新温湿度图表的数据、坐标标签和点数上限。 */
    void setTempHumiSeries(const QVector<TrendChartWidget::Series> &series,
                           const QStringList &axisLabels,
                           int maxPoints);
    /* 更新烟雾图表的数据、坐标标签和点数上限。 */
    void setSmokeSeries(const QVector<TrendChartWidget::Series> &series,
                        const QStringList &axisLabels,
                        int maxPoints);
    /* 根据窗口尺寸同步布局间距、按钮高度和标题行高度。 */
    void applyAdaptiveMetrics(int spacing, int primaryButtonHeight);

signals:
    /* 请求切换到最近 1 小时窗口。 */
    void window1hRequested();
    /* 请求切换到最近 24 小时窗口。 */
    void window24hRequested();

private:
    /* 构建历史页布局与按钮信号连接。 */
    void buildUi();

private:
    /* 温湿度趋势图。 */
    TrendChartWidget *tempHumiChart;
    /* 烟雾趋势图。 */
    TrendChartWidget *smokeChart;
    /* 最近 1 小时按钮。 */
    QPushButton *range1hButton;
    /* 最近 24 小时按钮。 */
    QPushButton *range24hButton;
};

#endif // HISTORYPAGE_H