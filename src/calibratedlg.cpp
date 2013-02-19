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
#include <QTimerEvent>
#include "calibratedlg.h"
#include "common.h"
#include "dbsettings.h"
#include "ui_calibratedlg.h"
#include <limits>

#include <QDebug>

CalibrateDlg::CalibrateDlg(QWidget *parent)
        : QDialog(parent),
          base_model(Model::Middle, Model::SegmentedVesselFlow)
{
	ui = new Ui::CalibrateDlg;
	ui->setupUi(this);

	struct CalibrationValue value;

	value.type = PA_diam;
	value.input = base_model.getResult(Model::PA_Diam_value);
	calibration_values.push_back(value);

	value.type = PV_diam;
	value.input = base_model.getResult(Model::PV_Diam_value);
	calibration_values.push_back(value);

	value.type = Krc;
	value.input = base_model.getKrc();
	calibration_values.push_back(value);

	value.type = CV_diam;
	value.input = base_model.getResult(Model::CV_Diam_value);
	calibration_values.push_back(value);

	// loads values from base model
	ui->tolerance->setText(doubleToString(base_model.getResult(Model::Tlrns_value)));

	ui->patHt->setText(doubleToString(base_model.getResult(Model::Pat_Ht_value)));
	ui->patWt->setText(doubleToString(base_model.getResult(Model::Pat_Wt_value)));
	ui->lungHt->setText(doubleToString(base_model.getResult(Model::Lung_Ht_value)));
	ui->Vm->setText(doubleToString(base_model.getResult(Model::Vm_value)));
	ui->Vrv->setText(doubleToString(base_model.getResult(Model::Vrv_value)));
	ui->Vfrc->setText(doubleToString(base_model.getResult(Model::Vfrc_value)));
	ui->Vtlc->setText(doubleToString(base_model.getResult(Model::Vtlc_value)));

	ui->Hct->setText(doubleToString(base_model.getResult(Model::Hct_value)));
	ui->PA_EVL->setText(doubleToString(base_model.getResult(Model::PA_EVL_value)));
	ui->PV_EVL->setText(doubleToString(base_model.getResult(Model::PV_EVL_value)));

	ui->CO->setText(doubleToString(base_model.getResult(Model::CO_value)));
	ui->LAP->setText(doubleToString(base_model.getResult(Model::LAP_value)));
	ui->Pal->setText(doubleToString(base_model.getResult(Model::Pal_value)));
	ui->Ppl->setText(doubleToString(base_model.getResult(Model::Ppl_value)));
	ui->target_PAPm->setText(doubleToString(base_model.getResult(Model::PAP_value)));
	ui->cv_target_PAPm->setText(doubleToString(DbSettings::value(calibration_cv_target_papm, 105.0).toDouble()));

	// saved or default ratios
	config_values.insert(ui->target_rus, QPair<QLatin1String,double>(rus_ratio, 42));
	config_values.insert(ui->target_rm,  QPair<QLatin1String,double>(rm_ratio, 16));
	config_values.insert(ui->target_rds, QPair<QLatin1String,double>(rds_ratio, 42));

	connect(ui->tolerance, SIGNAL(textEdited(const QString &)), SLOT(valueEditorFinished(const QString &)));
	connect(ui->patHt, SIGNAL(textEdited(const QString &)), SLOT(valueEditorFinished(const QString &)));
	connect(ui->patWt, SIGNAL(textEdited(const QString &)), SLOT(valueEditorFinished(const QString &)));
	connect(ui->lungHt, SIGNAL(textEdited(const QString &)), SLOT(valueEditorFinished(const QString &)));
	connect(ui->Vm, SIGNAL(textEdited(const QString &)), SLOT(valueEditorFinished(const QString &)));
	connect(ui->Vrv, SIGNAL(textEdited(const QString &)), SLOT(valueEditorFinished(const QString &)));
	connect(ui->Vfrc, SIGNAL(textEdited(const QString &)), SLOT(valueEditorFinished(const QString &)));
	connect(ui->Vtlc, SIGNAL(textEdited(const QString &)), SLOT(valueEditorFinished(const QString &)));

	connect(ui->CO, SIGNAL(textEdited(const QString &)), SLOT(valueEditorFinished(const QString &)));
	connect(ui->LAP, SIGNAL(textEdited(const QString &)), SLOT(valueEditorFinished(const QString &)));
	connect(ui->Pal, SIGNAL(textEdited(const QString &)), SLOT(valueEditorFinished(const QString &)));
	connect(ui->Ppl, SIGNAL(textEdited(const QString &)), SLOT(valueEditorFinished(const QString &)));

	connect(ui->Hct, SIGNAL(textEdited(const QString &)), SLOT(valueEditorFinished(const QString &)));
	connect(ui->PA_EVL, SIGNAL(textEdited(const QString &)), SLOT(valueEditorFinished(const QString &)));
	connect(ui->PV_EVL, SIGNAL(textEdited(const QString &)), SLOT(valueEditorFinished(const QString &)));

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
//	                << Model::MV_value
//	                << Model::CL_value
	                << Model::CO_value
	                << Model::LAP_value
	                << Model::Pal_value
	                << Model::PAP_value
	                << Model::PA_Diam_value
	                << Model::PV_Diam_value
	                << Model::Krc
	                << Model::Hct_value
	                << Model::PA_EVL_value
	                << Model::CV_Diam_value
	                << Model::PV_EVL_value
	                << Model::Vm_value
	                << Model::Vrv_value
	                << Model::Vfrc_value
	                << Model::Vtlc_value;

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
	DbSettings::setValue(calibration_cv_target_papm, ui->cv_target_PAPm->text().toDouble());
	QDialog::accept();
}

void CalibrateDlg::on_calculateButton_clicked()
{
	tlrns = 0.1;
	double target_total = 0.0;
	calibration_loop_no = 0;

	for (std::list<CalibrationValue>::iterator i=calibration_values.begin(); i!=calibration_values.end(); i++) {
		switch (i->type) {
		case PA_diam:
			i->target = ui->target_rus->text().toDouble() / 100.0;
			target_total += i->target;
			break;
		case PV_diam:
			i->target = ui->target_rds->text().toDouble() / 100.0;
			target_total += i->target;
			break;
		case Krc:
			i->target = ui->target_rm->text().toDouble() / 100.0;
			target_total += i->target;
			break;
		case CV_diam:
			i->target = ui->cv_target_PAPm->text().toDouble();
			break;
		}
	}

	if (fabs(target_total - 1.0) > 8*std::numeric_limits<double>::epsilon()) {
		QMessageBox::critical(this, "Invalid resistance ratios",
		                      "Target resistance distribution ratios must add up to 100%");
		return;
	}

	cv_calibration_step = 0;
	setCursor(Qt::WaitCursor);

	ui->calculateButton->setDisabled(true);
	ui->status->setText(QLatin1String("Calibrating model ...."));

	delete model_runner;
	resetBaseModel();
	model_runner = new AsyncRangeModelHelper(base_model, this);
	connect(model_runner, SIGNAL(calculationComplete()), SLOT(calculationComplete()));
	model_runner->beginCalculation();

	activity_indication_timer.start(1000, this);
}

void CalibrateDlg::on_resetButton_clicked()
{
	bool fReset = QMessageBox::question(this, "Reset calibration values to default?",
	                    "Do you wish to reset all calibration values to factory defaults?",
	                    QMessageBox::Yes, QMessageBox::No | QMessageBox::Default) == QMessageBox::Yes;

	if (!fReset)
		return;

	DbSettings::clearCache();
	QSqlQuery q(QSqlDatabase::database(settings_db));
	q.exec("DELETE FROM settings WHERE key LIKE '/settings/calibration/%'");
	reject();
}

void CalibrateDlg::calculationComplete()
{
	// calibration loop
	QString status_msg = QLatin1String("%3   [ calibration loop: %2, iteration count: %1 ]");
	ModelCalcList results = model_runner->output();
	const int n_iter = results.first().first;

	calibration_loop_no++;
	status_msg = status_msg
	             .arg(n_iter < 50 ? QString::number(n_iter) : QString("NOT converging"))
	             .arg(calibration_loop_no);

	const Model *m = results.first().second;

	const double pap = m->getResult(Model::PAP_value);
	const double pvr = m->getResult(Model::TotalR_value);
	const double rus = m->getResult(Model::Rus_value);
	const double rm = m->getResult(Model::Rm_value);
	const double rds = m->getResult(Model::Rds_value);

	double pa_diam = base_model.getResult(Model::PA_Diam_value);
	double pv_diam = base_model.getResult(Model::PV_Diam_value);
	double krc = base_model.getKrc();
	double cv_diam = base_model.getResult(Model::CV_Diam_value);

	// display numbers
	ui->pa_diameter->setText(doubleToString(pa_diam, 9));
	ui->pv_diameter->setText(doubleToString(pv_diam, 9));
	ui->cv_diam->setText(doubleToString(cv_diam * 10000, 9));
	ui->krc->setText(doubleToString(krc, 9));

	ui->pap->setText(doubleToString(pap));
	ui->rus->setText(doubleToString(rus/pvr*100.0, 2));
	ui->rm->setText(doubleToString(rm/pvr*100.0, 2));
	ui->rds->setText(doubleToString(rds/pvr*100.0, 2));

	ui->n_gen->setText(QString::number(16));

	bool calibration_complete = true;

	// first, get the PAPm correct
	double target_pap = ui->target_PAPm->text().toDouble();
	if (target_pap < 1.0)
		target_pap = 15.0;

	if (cv_calibration_step <= 0 &&
	    calibration_complete &&
	    fabs(pap-target_pap)/target_pap > tlrns) {

		double diff = 1.0 + (target_pap-pap)/target_pap/2.0;
		pa_diam /= sqrt(diff);
		pv_diam /= sqrt(diff);
		krc *= diff;

		calibration_complete = false;
		cv_calibration_step = 0;
	}

	// next, target Corner Vessel target diameter
	if (calibration_complete) {
		for(std::list<CalibrationValue>::reverse_iterator i=calibration_values.rbegin();
		    i!=calibration_values.rend();
		    i++) {

			if (i->type != CV_diam)
				continue;

			i->current_value = pap;
			i->input = cv_diam;
			i->is_modified = false;
			break;
		}

		calibration_complete = cornerVesselCorrection();

		for(std::list<CalibrationValue>::reverse_iterator i=calibration_values.rbegin();
		    i!=calibration_values.rend();
		    i++) {

			if (i->type != CV_diam)
				continue;

			cv_diam = i->input;
			calibration_complete = calibration_complete && !i->is_modified;
			break;
		}
	}

	// finally, adjust for the ratio between variables
	if (calibration_complete) {
		for (std::list<CalibrationValue>::iterator i=calibration_values.begin();
		     i!=calibration_values.end();
		     ++i) {

			CalibrationValue &value = *i;

			switch (value.type) {
			case PA_diam:
				value.input = pa_diam;
				value.current_value = rus/pvr;
				break;
			case PV_diam:
				value.input = pv_diam;
				value.current_value = rds/pvr;
				break;
			case Krc:
				value.input = krc;
				value.current_value = rm/pvr;
				break;
			case CV_diam:
				continue;
			}

			value.is_modified = false;
			CalibrationValue new_value = correctVariable(value);

			switch (value.type) {
			case PA_diam:
				pa_diam = new_value.input;
				break;
			case PV_diam:
				pv_diam = new_value.input;
				break;
			case Krc:
				krc = new_value.input;
				break;
			case CV_diam:
				continue;
			}

			*i = new_value;

			calibration_complete = calibration_complete && !value.is_modified;
		}

		if (!calibration_complete)
			cv_calibration_step = 0;
	}

	if (calibration_complete) {
		if (m->getResult(Model::Tlrns_value) <= tlrns)
			tlrns /= 2.0;
		else {
			ui->calculateButton->setDisabled(false);
			ui->saveButton->setDisabled(false);
			ui->status->setText(status_msg.arg(QLatin1String("Calibration complete.")));
			activity_indication_timer.stop();

			QApplication::beep();

			setCursor(Qt::ArrowCursor);
			return;
		}
	} // if (calibration_complete)

	ui->status->setText(status_msg.arg("Calculating ...."));

	resetBaseModel();
	base_model.setKrFactors(krc);
	base_model.setData(Model::PA_Diam_value, pa_diam);
	base_model.setData(Model::PV_Diam_value, pv_diam);
	base_model.setData(Model::CV_Diam_value, cv_diam);
	model_runner->deleteLater();
	model_runner = new AsyncRangeModelHelper(base_model, this);
	connect(model_runner, SIGNAL(calculationComplete()),
	        SLOT(calculationComplete()));
	model_runner->beginCalculation();

}

void CalibrateDlg::valueEditorFinished(const QString &val_str)
{
	QMap<Model::DataType, QLineEdit*> v;
	QMap<Model::DataType, double> current_values;
	QLineEdit *editor = qobject_cast<QLineEdit*>(sender());

	v.insert(Model::Tlrns_value, ui->tolerance);
	v.insert(Model::Pat_Ht_value, ui->patHt);
	v.insert(Model::Pat_Wt_value, ui->patWt);
	v.insert(Model::Lung_Ht_value, ui->lungHt);
	v.insert(Model::Vm_value, ui->Vm);
	v.insert(Model::Vrv_value, ui->Vrv);
	v.insert(Model::Vfrc_value, ui->Vfrc);
	v.insert(Model::Vtlc_value, ui->Vtlc);

	v.insert(Model::CO_value, ui->CO);
	v.insert(Model::LAP_value, ui->LAP);
	v.insert(Model::Pal_value, ui->Pal);
	v.insert(Model::Ppl_value, ui->Ppl);

	v.insert(Model::Hct_value, ui->Hct);
	v.insert(Model::PA_EVL_value, ui->PA_EVL);
	v.insert(Model::PV_EVL_value, ui->PV_EVL);

	// save current values
	bool fOk;
	const double new_val = val_str.toDouble(&fOk);
	if (!fOk)
		return;

	Model::DataType edited_type = (Model::DataType)-1;
	for (QMap<Model::DataType,QLineEdit*>::const_iterator i=v.begin(); i!=v.end(); ++i) {
		current_values[i.key()] = base_model.getResult(i.key());
		if (i.value() == editor)
			edited_type = i.key();
	}
	// update changed value
	if (edited_type == -1)
		return; // should not happen!
	if (!visibleChange(base_model.getResult(edited_type), new_val))
		return;
	base_model.setData(edited_type, new_val);
	current_values[edited_type] = new_val;

	// update values that were modified
	for (QMap<Model::DataType,QLineEdit*>::const_iterator i=v.begin(); i!=v.end(); ++i) {
		const double cur_val = current_values[i.key()];
		const double new_val = base_model.getResult(i.key());
		if (visibleChange(cur_val, new_val))
			i.value()->setText(doubleToString(new_val));
	}
}

void CalibrateDlg::on_gender_currentIndexChanged(int idx)
{
	switch (idx) {
	case 0: // Male
		base_model.setGender(Model::Male);
		break;
	case 1: // Female
		base_model.setGender(Model::Female);
		break;
	}

	updateCalculatedModelValues();
}

void CalibrateDlg::updateCalculatedModelValues()
{
	QMap<Model::DataType, QLineEdit*> v;

	v.insert(Model::Tlrns_value, ui->tolerance);
	v.insert(Model::Pat_Ht_value, ui->patHt);
	v.insert(Model::Pat_Wt_value, ui->patWt);
	v.insert(Model::Lung_Ht_value, ui->lungHt);
	v.insert(Model::Vm_value, ui->Vm);
	v.insert(Model::Vrv_value, ui->Vrv);
	v.insert(Model::Vfrc_value, ui->Vfrc);
	v.insert(Model::Vtlc_value, ui->Vtlc);

	v.insert(Model::CO_value, ui->CO);
	v.insert(Model::LAP_value, ui->LAP);
	v.insert(Model::Pal_value, ui->Pal);
	v.insert(Model::Ppl_value, ui->Ppl);

	v.insert(Model::Hct_value, ui->Hct);
	v.insert(Model::PA_EVL_value, ui->PA_EVL);
	v.insert(Model::PV_EVL_value, ui->PV_EVL);

	for (QMap<Model::DataType,QLineEdit*>::const_iterator i=v.begin(); i!=v.end(); ++i) {
		const double cur_val = i.value()->text().toDouble();
		const double new_val = base_model.getResult(i.key());
		if (visibleChange(cur_val, new_val))
			i.value()->setText(doubleToString(new_val));
	}
}

struct CalibrationValue CalibrateDlg::correctVariable(const CalibrationValue &value) const
{
	CalibrationValue v = value;

	if (fabs(value.target - value.current_value)/value.target > tlrns) {
		const double diff_percent = 1.0 + (value.target - value.current_value)/value.target/2.0;

		switch (value.type) {
		case PA_diam:
		case PV_diam:
			v.input /= sqrt(diff_percent);
			break;
		default:
			v.input *= diff_percent;
			break;
		}

		v.is_modified = true;
	}

	return v;
}

bool CalibrateDlg::cornerVesselCorrection()
{
	// Corner vessel correction is a multi-step process.
	//   1. close capillaries, recalculate
	//   2. adjust cv_diam until target PAPm is met
	//   3. reopen capillaries and recalculate

	qDebug() << "CV:" << cv_calibration_step;
	if (cv_calibration_step == -1) {
		// calibration is complete
		return true;
	}

	if (cv_calibration_step == 0) {
		// close all capillary vessels in the model
		setCapillaryState(Capillary_Closed);
		cv_calibration_step = 1;
		return false;
	}

	/* capillaries closed and recalculated, get closer to target */

	// find corner vessel calibration values, they should be closer to back
	std::list<CalibrationValue>::reverse_iterator cv_iter;
	for (cv_iter=calibration_values.rbegin(); cv_iter!=calibration_values.rend(); cv_iter++) {
		if (cv_iter->type == CV_diam)
			break;
	}
	if (cv_iter == calibration_values.rend())
		return true;

	CalibrationValue &cv = *cv_iter;
	if (fabs(cv.target - cv.current_value)/cv.target > tlrns) {
		const double diff_percent = 1.0 + (cv.target - cv.current_value)/cv.target/2.0;
		cv.input /= sqrt(diff_percent);
		cv.is_modified = true;

		qDebug() << "calibration adjustment:" << diff_percent;
	}
	else {
		// reopen capillaries
		qDebug() << "calibration finished";
		setCapillaryState(Capillary_Auto);
		cv_calibration_step = -1;
	}

	// even if calibration is complete, we need to recalculate for open
	// capillaries before continuing
	return false;
}

void CalibrateDlg::setCapillaryState(CapillaryState state)
{
	int n_caps = base_model.numCapillaries();
	for (int i=0; i<n_caps; i++) {
		Capillary c = base_model.capillary(i);
		if (c.open_state == state)
			continue;

		c.open_state = state;
		base_model.setCapillary(i, c);
	}
}

void CalibrateDlg::resetBaseModel()
{
	QMap<Model::DataType, QLineEdit*> v;

	v.insert(Model::Tlrns_value, ui->tolerance);
	v.insert(Model::Pat_Ht_value, ui->patHt);
	v.insert(Model::Pat_Wt_value, ui->patWt);
	v.insert(Model::Lung_Ht_value, ui->lungHt);
	v.insert(Model::Vm_value, ui->Vm);
	v.insert(Model::Vrv_value, ui->Vrv);
	v.insert(Model::Vfrc_value, ui->Vfrc);
	v.insert(Model::Vtlc_value, ui->Vtlc);

	v.insert(Model::CO_value, ui->CO);
	v.insert(Model::LAP_value, ui->LAP);
	v.insert(Model::Pal_value, ui->Pal);
	v.insert(Model::Ppl_value, ui->Ppl);

	v.insert(Model::Hct_value, ui->Hct);
	v.insert(Model::PA_EVL_value, ui->PA_EVL);
	v.insert(Model::PV_EVL_value, ui->PV_EVL);

	for (QMap<Model::DataType,QLineEdit*>::const_iterator i=v.begin(); i!=v.end(); ++i) {
		base_model.setData(i.key(), i.value()->text().toDouble());
	}
}

void CalibrateDlg::timerEvent(QTimerEvent *ev)
{
	if (ev->timerId() == activity_indication_timer.timerId())
		ui->status->setText(animateDots(ui->status->text()));
}
