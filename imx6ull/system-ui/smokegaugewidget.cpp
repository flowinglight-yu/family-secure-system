#include "smokegaugewidget.h"

#include <QPainter>

/*
 * 模块说明：烟雾圆弧仪表绘制实现。
 * 绘制顺序：先画底环，再根据当前数值画彩色进度弧，最后在中心绘制百分比文字。
 */

/*
 * 构造烟雾仪表控件。
 * 说明：初始化时只设置默认百分比，具体数值由外部传感器更新驱动。
 */
SmokeGaugeWidget::SmokeGaugeWidget(QWidget *parent)
    : QWidget(parent),
      valuePercent(0)
{
}

/*
 * 更新烟雾仪表的百分比值。
 * 说明：输入先做 0~100 限幅，再触发控件重绘。
 */
void SmokeGaugeWidget::setValue(int percent)
{
    /* 限幅，避免异常数据破坏绘制 */
    valuePercent = qBound(0, percent, 100);
    update();
}

/*
 * 绘制烟雾圆弧仪表。
 * 绘制顺序：底环、数值弧线、中心百分比文字依次完成，颜色按危险等级切换。
 */
void SmokeGaugeWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    /* 第一步：在可用区域内计算一个居中的正方形绘制区域。 */
    const int side = qMin(width(), height());
    const QRectF rect((width() - side) / 2.0 + 10.0,
                      (height() - side) / 2.0 + 10.0,
                      side - 20.0,
                      side - 20.0);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);

    /* 第二步：绘制固定灰色底环，表示完整量程。 */
    QPen basePen(QColor("#334155"), 10.0);
    painter.setPen(basePen);
    painter.drawArc(rect, 225 * 16, -270 * 16);

    /* 第三步：按百分比绘制彩色数值环，并根据阈值切换颜色。 */
    QColor levelColor("#2ecc71");
    if (valuePercent > 75) {
        levelColor = QColor("#ff3333");
    } else if (valuePercent > 50) {
        levelColor = QColor("#ff6b35");
    }

    QPen valuePen(levelColor, 10.0);
    painter.setPen(valuePen);
    painter.drawArc(rect, 225 * 16, -(270 * valuePercent / 100) * 16);

    /* 第四步：在中心区域绘制当前百分比文本。 */
    painter.setPen(QColor("#e6e8f2"));
    QFont font = painter.font();
    font.setPixelSize(20);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(this->rect(), Qt::AlignCenter, QString::number(valuePercent) + "%");
}
