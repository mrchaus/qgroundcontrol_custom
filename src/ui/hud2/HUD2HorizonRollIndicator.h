#ifndef HUD2HORIZONROLLINDICATOR_H
#define HUD2HORIZONROLLINDICATOR_H

#include <QWidget>
#include <QPen>

#include "HUD2Data.h"

class HUD2HorizonRollIndicator : public QWidget
{
    Q_OBJECT
public:
    explicit HUD2HorizonRollIndicator(HUD2data *huddata, QWidget *parent = 0);
    void paint(QPainter *painter, QColor color);

signals:
    void geometryChanged(const QSize *size);

public slots:
    void setColor(QColor color);
    void updateGeometry(const QSize *size);

private:
    QPen thickPen;
    QPen thinPen;
    QPen arrowPen;
    QLine thickLines[5];
    QLine thinLines[12];
    QLine arrowLines[2];

    HUD2data *huddata;
};

#endif // HUD2HORIZONROLLINDICATOR_H
