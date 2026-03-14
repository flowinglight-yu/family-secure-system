#ifndef SMOKEGAUGEWIDGET_H
#define SMOKEGAUGEWIDGET_H

/*
 * 模块说明：烟雾圆弧仪表控件。
 * 职责：把 0~100 的烟雾百分比映射成圆弧长度和颜色等级，用于仪表盘页面直观展示。
 */

#include <QWidget>

class SmokeGaugeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SmokeGaugeWidget(QWidget *parent = nullptr);
    /* 设置烟雾百分比（0~100）并触发重绘 */
    void setValue(int percent);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    /* 当前显示值，单位 % */
    int valuePercent;
};

#endif // SMOKEGAUGEWIDGET_H
