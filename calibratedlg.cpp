/*
 *   Bshouty Lung Model - Pulmonary Circulation Simulation
 *    Copyright (c) 1989-2012 Zoheir Bshouty, MD, PhD, FRCPC
 *    Copyright (c) 2011-2012 Adam Majer
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QList>
#include <QMessageBox>
#include <QSqlQuery>
#include "calibratedlg.h"
#include "common.h"
#include "dbsettings.h"
#include "ui_calibratedlg.h"
#include <limits>

const int CalibrateDlg::n_gen_progression[] = {5, 6, 15, 16, 0};

CalibrateDlg::CalibrateDlg(QWidget *parent)
        : QDialog(parent),
          gen_pos(0),
          base_model(Model::Middle, Model::SingleLung, n_gen_progression[gen_pos])
{
	ui = new Ui::CalibrateDlg;
	ui->setupUi(this);

	struct StepValue value;

	value.type = Kra;
	value.input = Model::getKra();
	calibration_values.push_back(value);

	value.type = Krv;
	value.input = Model::getKrv();
	calibration_values.push_back(value);

	value.type = Krc;
	value.input = Model::getKrc();
	calibration_values.push_back(value);

	// loads values from base model
	ui->tolerance->setText(doubleToString(base_model.getResult(Model::Tlrns_value)));

	ui->patHt->setText(doubleToString(base_model.getResult(Model::Pat_Ht_value)));
	ui->patWt->setText(doubleToString(base_model.getResult(Model::Pat_Wt_value)));
	ui->lungHt->setText(doubleToString(base_model.getResult(Model::Lung_Ht_value)));
	ui->MV->setText(doubleToString(base_model.getResult(Model::MV_value)));
	ui->CL->setText(doubleToString(base_model.getResult(Model::CL_value)));

	ui->CO->setText(doubleToString(base_model.getResult(Model::CO_value)));
	ui->LAP->setText(doubleToString(base_model.getResult(Model::LAP_value)));
	ui->Pal->setText(doubleToString(base_model.getResult(Model::Pal_value)));
	ui->Ppl->setText(doubleToString(base_model.getResult(Model::Ppl_value)));
	ui->target_PAPm->setText(doubleToString(base_model.getResult(Model::PAP_value)));

	// saved or default ratios
	config_values.insert(ui->target_rus, QPair<QLatin1String,double>(rus_ratio, 42));
	config_values.insert(ui->target_rm,  QPair<QLatin1String,double>(rm_ratio, 16));
	config_values.insert(ui->target_rds, QPair<QLatin1String,double>(rds_ratio, 42));

	config_values.insert(ui->arteries_branching, QPair<QLatin1String,double>(art_branching_ratio, 3.36));
	config_values.insert(ui->arteries_length, QPair<QLatin1String,double>(art_length_ratio, 1.49));
	config_values.insert(ui->arteries_diameter, QPair<QLatin1String,double>(art_diam_ratio, 1.56));
	config_values.insert(ui->veins_branching, QPair<QLatin1String,double>(vein_branching_ratio, 3.33));
	config_values.insert(ui->veins_length, QPair<QLatin1String,double>(vein_length_ratio, 1.50));
	config_values.insert(ui->veins_diameter, QPair<QLatin1String,double>(vein_diam_ratio, 1.58));


	for (ConfigValuesMap::const_iterator i=config_values.begin();
	     i!=config_values.end();
	     ++i) {

		QVariant value = DbSettings::value(i.value().first, i.value().second);
		i.key()->setText(doubleToString(value.toDouble()));
	}

	// start calibration loop
	model_runner = new AsyncRangeModelHelper(base_model, this);
	connect(model_runner, SIGNAL(calculationComplete()), SLOT(calculationComplete()));

	ui->saveButton->setDisabled(true);
//	model_runner->beginCalculation();

	ui->status->setText(QString::null);
}

CalibrateDlg::~CalibrateDlg()
{
	model_runner->waitForCompletion();
	delete model_runner;
	delete ui;
}

void CalibrateDlg::accept()
{
	// Save calibration
	QList<Model::DataType> save_values = QList<Model::DataType>()
	                << Model::Tlrns_value
	                << Model::Pat_Ht_value
	                << Model::Pat_Wt_value
	                << Model::MV_value
	                << Model::CL_value
	                << Model::CO_value
	                << Model::LAP_value
	                << Model::Pal_value
	                << Model::PAP_value
	                << Model::Kra
	                << Model::Krv
	                << Model::Krc;

	foreach (Model::DataType data_type, save_values) {
		DbSettings::setValue(Model::calibrationPath(data_type),
		                     base_model.getResult(data_type));
	}

	for (ConfigValuesMap::const_iterator i=config_values.begin();
	     i != config_values.end();
	     i++) {

		DbSettings::setValue(i.value().first, i.key()->text().toDouble());
	}

	DbSettings::setValue(calibration_target_papm, ui->target_PAPm->text().toDouble());
	QDialog::accept();
}

void CalibrateDlg::on_calculateButton_clicked()
{
	Model::setCalibrationRatios(ui->arteries_branching->text().toDouble(),
	                            ui->arteries_length->text().toDouble(),
	                            ui->arteries_diameter->text().toDouble(),

	                            ui->veins_branching->text().toDouble(),
	                            ui->veins_length->text().toDouble(),
	                            ui->veins_diameter->text().toDouble());

	tlrns = 0.1;
	double target_total = 0.0;
	for (std::list<StepValue>::iterator i=calibration_values.begin(); i!=calibration_values.end(); i++) {
		switch (i->type) {
		case Kra:
			i->target = ui->target_rus->text().toDouble() / 100.0;
			target_total += i->target;
			break;
		case Krv:
			i->target = ui->target_rds->text().toDouble() / 100.0;
			target_total += i->target;
			break;
		case Krc:
			i->target = ui->target_rm->text().toDouble() / 100.0;
			target_total += i->target;
			break;
		}
	}

	if (fabs(target_total - 1.0) > 8*std::numeric_limits<double>::epsilon()) {
		QMessageBox::critical(this, "Invalid resistance ratios",
		                      "Target resistance distribution ratios must add up to 100%");
		return;
	}

	setCursor(Qt::WaitCursor);

	ui->calculateButton->setDisabled(true);
	ui->status->setText(QLatin1String("Calibrating model ...."));

	delete model_runner;
	resetBaseModel();
	model_runner = new AsyncRangeModelHelper(base_model, this);
	connect(model_runner, SIGNAL(calculationComplete()), SLOT(calculationComplete()));
	model_runner->beginCalculation();
}

void CalibrateDlg::on_resetButton_clicked()
{
	bool fReset = QMessageBox::question(this, "Reset calibration values to default?",
	                    "Do you wish to reset all calibration values to factory defaults?",
	                    QMessageBox::Yes, QMessageBox::No | QMessageBox::Default) == QMessageBox::Yes;

	if (!fReset)
		return;

	QSqlQuery q(QSqlDatabase::database(settings_db));
	q.exec("DELETE FROM settings WHERE key LIKE '/settings/calibration/%'");
	reject();
}

void CalibrateDlg::calculationComplete()
{
	// calibration loop
	const QString status_msg = QLatin1String("Calculating....  [ last iteration count: %1 ]");
	ModelCalcList results = model_runner->output();
	const int n_iter = results.first().first;

	ui->status->setText(status_msg.arg(n_iter < 50 ? QString::number(n_iter) : QString("NOT converging")));
	const Model *m = results.first().second;

	const double pap = m->getResult(Model::PAP_value);
	const double pvr = m->getResult(Model::TotalR_value);
	const double rus = m->getResult(Model::Rus_value);
	const double rm = m->getResult(Model::Rm_value);
	const double rds = m->getResult(Model::Rds_value);

	double kra = Model::getKra();
	double krv = Model::getKrv();
	double krc = Model::getKrc();

	// display numbers
	ui->kra->setText(doubleToString(kra, 9));
	ui->krv->setText(doubleToString(krv, 9));
	ui->krc->setText(doubleToString(krc, 9));

	ui->pap->setText(doubleToString(pap));
	ui->rus->setText(doubleToString(rus/pvr*100.0, 2));
	ui->rm->setText(doubleToString(rm/pvr*100.0, 2));
	ui->rds->setText(doubleToString(rds/pvr*100.0, 2));

	ui->n_gen->setText(QString::number(n_gen_progression[gen_pos]));

	bool calibration_complete = true;

	// when ratio correct, get the PAP correct
	double target_pap = ui->target_PAPm->text().toDouble();
	if (target_pap < 1.0)
		target_pap = 15.0;

	if (calibration_complete && fabs(pap-target_pap)/target_pap > tlrns) {
		double diff = 1.0 + (target_pap-pap)/target_pap/2.0;
		kra *= diff;
		krv *= diff;
		krc *= diff;

		calibration_complete = false;
	}

	// first adjust for the ratio between variables
	if (calibration_complete)
	for (std::list<StepValue>::iterator i=calibration_values.begin();
	     i!=calibration_values.end();
	     ++i) {

		StepValue &value = *i;

		switch (value.type) {
		case Kra:
			value.input = kra;
			value.current_value = rus/pvr;
			break;
		case Krv:
			value.input = krv;
			value.current_value = rds/pvr;
			break;
		case Krc:
			value.input = krc;
			value.current_value = rm/pvr;
			break;
		}

		value.is_modified = false;
		StepValue new_value = correctVariable(value);

		switch (value.type) {
		case Kra:
			kra = new_value.input;
			break;
		case Krv:
			krv = new_value.input;
			break;
		case Krc:
			krc = new_value.input;
			break;
		}

		*i = new_value;

		calibration_complete = calibration_complete && !value.is_modified;
	}

	if (calibration_complete) {
		if (m->getResult(Model::Tlrns_value) <= tlrns)
			tlrns /= 2.0;
		else {
			gen_pos++;

			switch (n_gen_progression[gen_pos]) {
			case 5:
			case 6:
			case 15:
			case 16:
				tlrns = 0.001;
				break;
			case 0:
				ui->calculateButton->setDisabled(false);
				ui->saveButton->setDisabled(false);
				ui->status->setText(QLatin1String("Calibration complete."));
				QApplication::beep();
				gen_pos=0;

				setCursor(Qt::ArrowCursor);
				return;
			default:
				qFatal("Should not happen");
			}
		}
	}

	Model::setKrFactors(kra, krv, krc);

	resetBaseModel();
	model_runner->deleteLater();
	model_runner = new AsyncRangeModelHelper(base_model, this);
	connect(model_runner, SIGNAL(calculationComplete()),
	        SLOT(calculationComplete()));
	model_runner->beginCalculation();

}

struct StepValue CalibrateDlg::correctVariable(const StepValue &value) const
{
	StepValue v = value;

	if (fabs(value.target - value.current_value)/value.target > tlrns) {
		v.input *= 1.0 + (value.target - value.current_value)/value.target/2.0;
		v.is_modified = true;
	}

	return v;
}

void CalibrateDlg::resetBaseModel()
{
	switch (n_gen_progression[gen_pos]) {
	case 5:
	case 15:
		base_model = Model(Model::Middle,
		                   Model::SingleLung,
		                   n_gen_progression[gen_pos]);
		break;
	case 6:
	case 16:
		base_model = Model(Model::Middle,
		                   Model::DoubleLung,
		                   n_gen_progression[gen_pos]);
		break;
	}

	QMap<Model::DataType, QLineEdit*> v;

	v.insert(Model::Tlrns_value, ui->tolerance);
	v.insert(Model::Pat_Ht_value, ui->patHt);
	v.insert(Model::Pat_Wt_value, ui->patWt);
	v.insert(Model::Lung_Ht_value, ui->lungHt);
	v.insert(Model::MV_value, ui->MV);
	v.insert(Model::CL_value, ui->CL);

	v.insert(Model::CO_value, ui->CO);
	v.insert(Model::LAP_value, ui->LAP);
	v.insert(Model::Pal_value, ui->Pal);
	v.insert(Model::Ppl_value, ui->Ppl);

	for (QMap<Model::DataType,QLineEdit*>::const_iterator i=v.begin(); i!=v.end(); ++i) {
		base_model.setData(i.key(), i.value()->text().toDouble());
	}
}
