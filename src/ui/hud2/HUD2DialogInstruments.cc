#include <QSettings>

#include "HUD2DialogInstruments.h"
#include "ui_HUD2DialogInstruments.h"

HUD2DialogInstruments::HUD2DialogInstruments(HUD2FormHorizon *horizon_form,
                                             HUD2IndicatorRoll *roll,
                                             HUD2Ribbon *speedometer,
                                             HUD2Ribbon *altimeter,
                                             HUD2Ribbon *compass,
                                             HUD2IndicatorFps *fps,
                                             QWidget *parent) :
    QDialog(parent),
    ui(new Ui::HUD2DialogInstruments)
{
    QSettings settings;
    settings.beginGroup("QGC_HUD2");
    int current_index = settings.value("DIALOG_INSTRUMENTS_LAST_TAB", 0).toInt();
    settings.endGroup();

    ui->setupUi(this);

    Q_UNUSED(roll);

    HUD2FormRibbon *speed_form = new HUD2FormRibbon(speedometer, this);
    HUD2FormRibbon *climb_form = new HUD2FormRibbon(altimeter, this);
    HUD2FormRibbon *compass_form = new HUD2FormRibbon(compass, this);
    HUD2FormFps *fps_form = new HUD2FormFps(fps, this);

    ui->tabWidget->addTab(speed_form, "speed");
    ui->tabWidget->addTab(climb_form, "alt");
    ui->tabWidget->addTab(compass_form, "compass");
    ui->tabWidget->addTab(horizon_form, "horizon");
    ui->tabWidget->addTab(fps_form, "fps");

    ui->tabWidget->setCurrentIndex(current_index);
}

HUD2DialogInstruments::~HUD2DialogInstruments()
{
    int index = ui->tabWidget->currentIndex();

    QSettings settings;
    settings.beginGroup("QGC_HUD2");
    settings.setValue("DIALOG_INSTRUMENTS_LAST_TAB", index);
    settings.endGroup();

    delete ui;
}
