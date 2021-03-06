/*
 *   Bshouty Lung Model - Pulmonary Circulation Simulation
 *    Copyright (c) 1989-2014 Zoheir Bshouty, MD, PhD, FRCPC
 *    Copyright (c) 2011-2014 Adam Majer
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

#include "common.h"
#include "vesseldlg.h"
#include "ui_vesseldlg.h"
#include <QMap>
#include <QRegExpValidator>

VesselDlg::VesselDlg(Vessel::Type type,
                     Vessel &vessel,
                     const Vessel &calc_values,
                     const std::vector<double> &vessel_dims,
                     bool PtpGP_RO,
                     QWidget *parent)
        :QDialog(parent), v(vessel)
{
	Q_UNUSED(type);

	ui = new Ui::VesselDlg;
	ui->setupUi( this );

	ui->D->setText(doubleToString(v.D));
	ui->gamma->setText(doubleToString(v.gamma));
	ui->phi->setText(doubleToString(v.phi));
	ui->c->setText(doubleToString(v.c));
	ui->tone->setText(doubleToString(v.tone));

	QValidator *validator = new QRegExpValidator(QRegExp(numeric_validator_rx), this);
	ui->D->setValidator(validator);
	ui->gamma->setValidator(validator);
	ui->phi->setValidator(validator);
	ui->c->setValidator(validator);
	ui->tone->setValidator(validator);

	if (PtpGP_RO) {
		ui->gp->setText("*** Calculated ****");
		ui->ptp->setText("*** Calculated ****");

		ui->gp->setDisabled(true);
		ui->ptp->setDisabled(true);
	}
	else {
		ui->gp->setText(doubleToString(v.GP));
		ui->ptp->setText(doubleToString(v.Ptp));
		ui->gp->setValidator(validator);
		ui->ptp->setValidator(validator);
	}

	ui->D->setToolTip("<p>Baseline vessel diameter defined under standardized conditions of P<sub>tm</sub>=35cmH<sub>2</sub>O and P<sub>tp</sub>=0.</p>");
	ui->gp->setToolTip("<p>Gravitational pressure at vessel site.");
	ui->tone->setToolTip("<p>Smooth muscle tone (in mmHg) can be added to any vessel to simulate vasoconstriction.");
	ui->ptp->setToolTip("<p>Transpulmonary pressure at vessel site.</p>");

	// set read-only calculated values
#define SET_DATA(val) ui->c##val->setText(doubleToString(calc_values.val))
	SET_DATA(R);
	SET_DATA(Dmin);
	SET_DATA(D_calc);
	SET_DATA(Dmax);
	SET_DATA(volume);
	SET_DATA(length);
	SET_DATA(vessel_ratio);
	SET_DATA(viscosity_factor);
	SET_DATA(gamma);
	SET_DATA(phi);
	SET_DATA(c);
	SET_DATA(tone);
	SET_DATA(perivascular_press_a);
	SET_DATA(perivascular_press_b);
	SET_DATA(perivascular_press_c);
	SET_DATA(pressure_in);
	SET_DATA(pressure_out);
	SET_DATA(flow);
#undef SET_DATA

	ui->vessel->setDrawData(vessel_dims);
}

void VesselDlg::accept()
{
	double n;
	bool ok;

	QMap<QLineEdit*,double*> refs;

	refs[ui->D] = &v.D;
	refs[ui->gamma] = &v.gamma;
	refs[ui->phi] = &v.phi;
	refs[ui->c] = &v.c;
	refs[ui->gp] = &v.GP;
	refs[ui->ptp] = &v.Ptp;
	refs[ui->tone] = &v.tone;

	foreach (QLineEdit *editor, refs.keys()) {
		if (!editor->isEnabled())
			continue;

		n = editor->text().toDouble(&ok);
		if (ok)
			*(refs[editor]) = n;
	}

	QDialog::accept();
}
