#include <QButtonGroup>
#include "common.h"
#include "overlaysettingsdlg.h"
#include "ui_overlaysettingsdlg.h"


OverlaySettingsDlg::OverlaySettingsDlg(QWidget *parent)
        : QDialog(parent)
{
	init();
}

OverlaySettingsDlg::OverlaySettingsDlg(const OverlaySettings &settings, QWidget *parent)
        : QDialog(parent),
          s(settings)
{
	init();
}

void OverlaySettingsDlg::accept()
{
	getDialogState();
	QDialog::accept();
}

void OverlaySettingsDlg::setOverlaySettings(const OverlaySettings &settings)
{
	s = settings;
	setDialogState();
}

void OverlaySettingsDlg::init()
{
	ui = new Ui::OverlaySettingsDlg();
	ui->setupUi(this);

	setDialogState();
}

void OverlaySettingsDlg::setDialogState()
{
	switch (s.type) {
	case OverlaySettings::Absolute:
		ui->overlayType->setCurrentIndex(0);
		ui->overlayTypeStack->setCurrentIndex(0);
		if (s.auto_max) {
			ui->autoMaximumButton->setChecked(true);
			ui->manualMaximum->clear();
		}
		else {
			ui->manualMaximumButton->setChecked(true);
			ui->manualMaximum->setText(doubleToString(s.max));
		}

		if (s.auto_min) {
			ui->autoMinimumButton->setChecked(true);
			ui->manualMinimum->clear();
		}
		else {
			ui->manualMinimumButton->setChecked(true);
			ui->manualMinimum->setText(doubleToString(s.min));
		}
		break;
	case OverlaySettings::Relative:
		ui->overlayType->setCurrentIndex(1);
		ui->overlayTypeStack->setCurrentIndex(1);

		if (s.auto_mean) {
			ui->autoMeanButton->setChecked(true);
			ui->manualMean->clear();
		}
		else {
			ui->manualMeanButton->setChecked(true);
			ui->manualMean->setText(doubleToString(s.mean));
		}

		if (s.auto_range) {
			ui->autoRangeButton->setChecked(true);
			ui->manualRange->clear();
		}
		else {
			ui->manualRangeButton->setChecked(true);
			ui->manualRange->setValue(s.range);
		}
		break;
	}

	ui->percent_absolute_scale_checkbox->setChecked(s.percent_absolute_scale);
}

void OverlaySettingsDlg::getDialogState()
{
	switch (ui->overlayType->currentIndex()) {
	case 0:
		s.type = OverlaySettings::Absolute;
		s.auto_min = ui->autoMinimumButton->isChecked();
		s.min = ui->manualMinimum->text().toDouble();

		s.auto_max = ui->autoMaximumButton->isChecked();
		s.max = ui->manualMaximum->text().toDouble();
		break;
	case 1:
		s.type = OverlaySettings::Relative;
		s.auto_mean = ui->autoMeanButton->isChecked();
		s.mean = ui->manualMean->text().toDouble();

		s.auto_range = ui->autoRangeButton->isChecked();
		s.range = ui->manualRange->value();
		break;
	}

	s.percent_absolute_scale = ui->percent_absolute_scale_checkbox->isChecked();
}
