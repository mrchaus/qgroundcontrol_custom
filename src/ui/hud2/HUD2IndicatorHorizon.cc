#include <QtGui>

#include "HUD2IndicatorHorizon.h"
#include "HUD2Math.h"

HUD2IndicatorHorizon::HUD2IndicatorHorizon(HUD2Data &huddata, QWidget *parent) :
    QWidget(parent),
    pitchline(&this->gap, this),
    crosshair(&this->gap, this),
    huddata(huddata)
{
    this->gap = 6;
    this->pitchcount = 5;
    this->degstep = 10;
    this->pen.setColor(Qt::green);
}

void HUD2IndicatorHorizon::updateGeometry(const QSize &size){
    int a = percent2pix_w(size, this->gap);

    // wings
    int x1 = size.width() / 2;
    int tmp = percent2pix_h(size, 1);
    hud2_clamp(tmp, 2, 10);
    pen.setWidth(tmp);
    hirizonleft.setLine(-x1, 0, -a, 0);
    horizonright.setLine(a, 0, x1, 0);
    
    // pitchlines
    pixstep = size.height() / pitchcount;
    pitchline.updateGeometry(size);

    // crosshair
    crosshair.updateGeometry(size);
}

/**
 * @brief drawpitchlines
 * @param painter
 * @param degstep
 * @param pixstep
 */
void HUD2IndicatorHorizon::drawpitchlines(QPainter *painter, qreal degstep, qreal pixstep){

    painter->save();
    int i = 0;
    while (i > -360){
        i -= degstep;
        painter->translate(0, -pixstep);
        pitchline.paint(painter, -i);
    }
    painter->restore();

    painter->save();
    i = 0;
    while (i < 360){
        i += degstep;
        painter->translate(0, pixstep);
        pitchline.paint(painter, -i);
    }
    painter->restore();
}

void HUD2IndicatorHorizon::paint(QPainter *painter){

    painter->save();
    painter->translate(painter->window().center());
    crosshair.paint(painter);

    // now perform complex transfomation of painter
    qreal alpha = rad2deg(-huddata.pitch);

    QTransform transform;
    QPoint center;

    center = painter->window().center();
    transform.translate(center.x(), center.y());
    qreal delta_y = alpha * (pixstep / degstep);
    qreal delta_x = tan(-huddata.roll) * delta_y;

    transform.translate(delta_x, delta_y);
    transform.rotate(rad2deg(huddata.roll));

    painter->setTransform(transform);

    // pitchlines
    this->drawpitchlines(painter, degstep, pixstep);

    // horizon lines
    painter->setPen(pen);
    painter->drawLine(hirizonleft);
    painter->drawLine(horizonright);

    painter->restore();



//    painter->resetTransform();
//    painter->save();
//    QPolygon polygon(1);
//    polygon[0] = QPoint(0, 0);
//    //polygon.putPoints(1, 2, 0,170, 180,190);
//    polygon.putPoints(1, 1, 0,170);
//    polygon.putPoints(2, 1, 180,190);

//    QBrush sky_brush = QBrush(Qt::blue);
//    painter->setBrush(sky_brush);
//    painter->setPen(QPen(Qt::blue));
//    painter->drawPolygon(polygon);

//    painter->restore();
}

void HUD2IndicatorHorizon::setColor(QColor color){
    pen.setColor(color);
}
