#include "trendchartwidget.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

/*
 * 模块说明：历史趋势图控件实现。
 * 绘制顺序：先画背景、坐标轴和辅助网格，再根据当前数据窗口内的最小值和最大值归一化绘制折线。
 */

/*
 * 构造一条历史曲线的数据描述对象。
 * 说明：把数值、颜色和名称打包，便于图表控件统一渲染多条曲线。
 */
TrendChartWidget::Series::Series(const QVector<int> &seriesValues,
                                 const QColor &seriesColor,
                                 const QString &seriesName)
    : values(seriesValues)
    , color(seriesColor)
    , name(seriesName)
{
}

/*
 * 构造趋势图控件。
 * 说明：初始化默认点数上限和坐标缩放参数，并设置基础最小高度。
 */
TrendChartWidget::TrendChartWidget(QWidget *parent)
    : QWidget(parent)
    , maxPoints(120)
{
    /* 给历史页预留基础显示高度 */
    setMinimumHeight(120);
}

/*
 * 设置整组曲线数据。
 * 说明：外部通常在切换历史窗口后一次性传入所有曲线，控件内部负责按上限裁剪。
 */
void TrendChartWidget::setSeriesData(const QVector<Series> &seriesList)
{
    /* 直接替换全量数据，常用于窗口切换后重算。 */
    series = seriesList;
    for (int i = 0; i < series.size(); ++i) {
        while (series[i].values.size() > maxPoints) {
            series[i].values.removeFirst();
        }
    }
    update();
}

/*
 * 设置 X 轴显示标签。
 * 说明：标签通常用于表达最近 1 小时或 24 小时窗口下的时间范围。
 */
void TrendChartWidget::setXAxisLabels(const QStringList &labels)
{
    xAxisLabels = labels;
    update();
}

/*
 * 设置曲线点数上限。
 * 说明：限制曲线点数可以控制绘制复杂度，避免历史窗口扩大时过度堆积数据点。
 */
void TrendChartWidget::setMaxPoints(int points)
{
    /* 至少保留 10 个点，避免缩放后无法成线 */
    maxPoints = qMax(10, points);
    for (int i = 0; i < series.size(); ++i) {
        while (series[i].values.size() > maxPoints) {
            series[i].values.removeFirst();
        }
    }
    update();
}

/*
 * 绘制历史趋势图。
 * 绘制顺序：背景、坐标轴、刻度文本和多条折线依次输出，共享同一坐标系。
 */
void TrendChartWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    /* 第一步：绘制背景并开启抗锯齿。 */
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor("#1f2438"));

    const QRect plotRect = rect().adjusted(46, 12, -12, -30);
    if (plotRect.width() <= 0 || plotRect.height() <= 0) {
        return;
    }

    /* 第二步：绘制坐标轴和辅助网格，给数值刻度留出固定边距。 */
    painter.setPen(QPen(QColor("#475569"), 1));
    painter.drawLine(plotRect.bottomLeft(), plotRect.bottomRight());
    painter.drawLine(plotRect.bottomLeft(), plotRect.topLeft());

    int minValue = 0;
    int maxValue = 0;
    bool hasData = false;
    for (int i = 0; i < series.size(); ++i) {
        if (series.at(i).values.isEmpty()) {
            continue;
        }

        for (int j = 0; j < series.at(i).values.size(); ++j) {
            const int value = series.at(i).values.at(j);
            if (!hasData) {
                minValue = value;
                maxValue = value;
                hasData = true;
            } else {
                minValue = qMin(minValue, value);
                maxValue = qMax(maxValue, value);
            }
        }
    }

    if (!hasData) {
        painter.setPen(QColor("#94a3b8"));
        painter.drawText(plotRect, Qt::AlignCenter, QStringLiteral("暂无数据"));
        return;
    }

    if (minValue == maxValue) {
        /* 平直数据给一个最小范围，避免除零 */
        maxValue = minValue + 1;
    }

    painter.setPen(QPen(QColor("#334155"), 1));
    const int yTickCount = 5;
    for (int i = 0; i < yTickCount; ++i) {
        const qreal ratio = static_cast<qreal>(i) / (yTickCount - 1);
        const int y = plotRect.bottom() - qRound(ratio * plotRect.height());
        painter.drawLine(plotRect.left(), y, plotRect.right(), y);

        const qreal rawValue = minValue + ratio * (maxValue - minValue);
        const QString tickText = QString::number(qRound(rawValue));
        painter.setPen(QColor("#cbd5f5"));
        painter.drawText(QRect(0, y - 10, plotRect.left() - 6, 20),
                         Qt::AlignRight | Qt::AlignVCenter,
                         tickText);
        painter.setPen(QPen(QColor("#334155"), 1));
    }

    /* 第三步：绘制 X 轴数值刻度，表达当前窗口范围的起点、中点和当前值。 */
    const QStringList labels = xAxisLabels.isEmpty()
            ? (QStringList() << QStringLiteral("0")
                             << QString::number(maxPoints / 2)
                             << QString::number(maxPoints))
            : xAxisLabels;
    const QList<int> labelPositions = QList<int>()
            << plotRect.left()
            << plotRect.center().x()
            << plotRect.right();
    for (int i = 0; i < qMin(labels.size(), labelPositions.size()); ++i) {
        const int x = labelPositions.at(i);
        painter.setPen(QPen(QColor("#475569"), 1));
        painter.drawLine(x, plotRect.bottom(), x, plotRect.bottom() + 4);
        painter.setPen(QColor("#cbd5f5"));
        painter.drawText(QRect(x - 24, plotRect.bottom() + 6, 48, 18),
                         Qt::AlignHCenter | Qt::AlignTop,
                         labels.at(i));
    }

    /* 第四步：逐条绘制曲线，所有曲线共用同一数值坐标系。 */
    for (int i = 0; i < series.size(); ++i) {
        const Series &entry = series.at(i);
        if (entry.values.size() < 2) {
            continue;
        }

        QPainterPath path;
        for (int j = 0; j < entry.values.size(); ++j) {
            const qreal x = plotRect.left() + static_cast<qreal>(j) * plotRect.width() / (entry.values.size() - 1);
            const qreal normalized = static_cast<qreal>(entry.values.at(j) - minValue) / (maxValue - minValue);
            const qreal y = plotRect.bottom() - normalized * plotRect.height();
            if (j == 0) {
                path.moveTo(x, y);
            } else {
                path.lineTo(x, y);
            }
        }

        const QColor lineColor = entry.color.isValid() ? entry.color : QColor("#00d4ff");
        painter.setPen(QPen(lineColor, 2));
        painter.drawPath(path);
    }
}
