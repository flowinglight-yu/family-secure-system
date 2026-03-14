#ifndef TRENDCHARTWIDGET_H
#define TRENDCHARTWIDGET_H

/*
 * 模块说明：轻量级历史趋势图控件。
 * 职责：把一组或多组整数样本归一化后绘制成折线，并显示基础坐标轴数值。
 */

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class TrendChartWidget : public QWidget
{
    Q_OBJECT

public:
    struct Series
    {
        Series(const QVector<int> &seriesValues = QVector<int>(),
               const QColor &seriesColor = QColor(),
               const QString &seriesName = QString());

        QVector<int> values;
        QColor color;
        QString name;
    };

    explicit TrendChartWidget(QWidget *parent = nullptr);
    /* 批量设置一组或多组曲线数据。 */
    void setSeriesData(const QVector<Series> &seriesList);
    /* 设置 X 轴标签，通常用于显示时间窗口的起点、中点和当前时刻。 */
    void setXAxisLabels(const QStringList &labels);
    /* 设置最多保留点数 */
    void setMaxPoints(int points);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    /* 当前控件要绘制的所有曲线。 */
    QVector<Series> series;
    /* X 轴显示标签。 */
    QStringList xAxisLabels;
    /* 点数上限 */
    int maxPoints;
};

#endif // TRENDCHARTWIDGET_H
