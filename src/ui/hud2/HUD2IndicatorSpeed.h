#ifndef HUD2INDICATORSPEED_H
#define HUD2INDICATORSPEED_H

#include <QWidget>

#include "HUD2Data.h"
#include "HUD2Ribbon.h"

class HUD2IndicatorSpeed : public HUD2Ribbon
{
    Q_OBJECT
public:
    explicit HUD2IndicatorSpeed(const float *value, QWidget *parent);
};



#endif // HUD2INDICATORSPEED_H
