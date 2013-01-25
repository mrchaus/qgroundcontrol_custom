#ifndef HUD2RENDERGL_H
#define HUD2RENDERGL_H

#include <QGLWidget>
#include "HUD2Data.h"
#include "HUD2Drawer.h"

class HUD2Drawer;
QT_BEGIN_NAMESPACE
class QPaintEvent;
class QWidget;
QT_END_NAMESPACE

class HUD2RenderGL : public QGLWidget
{
    Q_OBJECT

public:
    HUD2RenderGL(HUD2Data &huddata, QWidget *parent);

signals:
    void geometryChanged(const QSize *size);

public slots:
    void paint(void);

protected:
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event);

private:
    HUD2Drawer hudpainter;
};

#endif /* HUD2RENDERGL_H */
