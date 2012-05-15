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

#include "common.h"
#include "vesseldlg.h"
#include "ui_vesseldlg.h"
#include <QMap>
#include <QRegExpValidator>

VesselDlg::VesselDlg(VesselType type, Vessel &vessel, bool PtpGP_RO, QWidget *parent)
        :QDialog(parent), v(vessel)
{
	ui = new Ui::VesselDlg;
	ui->setupUi( this );

	ui->R->setText(doubleToString(v.R));
	ui->a->setText(doubleToString(v.a));
	ui->b->setText(doubleToString(v.b));
	ui->c->setText(doubleToString(v.c));
	ui->cl->setText(doubleToString(v.CL));
	ui->mv->setText(doubleToString(v.MV));
	ui->tone->setText(doubleToString(v.tone));

	QValidator *validator = new QRegExpValidator(QRegExp(numeric_validator_rx), this);
	ui->R->setValidator(validator);
	ui->a->setValidator(validator);
	ui->b->setValidator(validator);
	ui->c->setValidator(validator);
	ui->tone->setValidator(validator);

	ui->cl->setValidator(validator);
	ui->mv->setValidator(validator);

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

	ui->R->setToolTip("<p>Baseline resistance defined under standardized conditions of P<sub>tm</sub>=35cmH<sub>2</sub>O and P<sub>tp</sub>=0.</p>");
	ui->gp->setToolTip("<p>Gravitational pressure at vessel site.");
	ui->tone->setToolTip("<p>Smooth muscle tone (in mmHg) can be added to any vessel to simulate vasoconstriction.");
	ui->ptp->setToolTip("<p>Transpulmonary pressure at vessel site.</p>");

	ui->mv->setToolTip("<p>VM stands for Minimal Volume. It is the volume (as a percent of TLC) that the lung assumes at a transpulmonary pressure (Ptp) of 0.</p><p>Changing value here will override model-wide M<sub>V</sub> value.</p><p>Range: 0.01 - 0.25</p>");
	ui->cl->setToolTip("<p>Patient lung compliance in L/cm H2O</p><p>Changing value here will override model-wide C<sub>L</sub> value.</p><p>Range: 0.0001 - 1.0</p>");

	switch (type) {
	case Artery:
		ui->a->setToolTip("<p>Parameter that controls arterial vessels passive characteristics (intercept) based on a linear relationship between vessel cross-sectional area (Area) and transmural pressure (P<sub>tm</sub>) according to the following relationship:</p><p><center>Area=A+B&times;P<sub>tm</sub></p>");
		ui->b->setToolTip("<p>Parameter that controls arterial (slope) vessels cross-sectional area passive characteristics as a function of transmural pressure (P<sub>tm</sub>) according to the following relationships:</p><p><center>Area=A+B&times;P<sub>tm</sub></center></p>");
		ui->c->hide();
		ui->c_label->hide();
		break;
	case Vein:
		ui->b->setToolTip("<p>Parameter that controls venous vessels passive characteristics based on a curvilinear relationship between vessel cross-sectional area (Area) and transmural pressure (P<sub>tm</sub>) according to the following relationship:</p><p><center>Area=1/[1+B&times;e<sup>C&times;Ptm</sup>]</center></p>");
		ui->c->setToolTip(ui->b->toolTip());
		ui->a->hide();
		ui->a_label->hide();
		break;
	}
}

void VesselDlg::accept()
{
	double n;
	bool ok;

	QMap<QLineEdit*,double*> refs;

	refs[ui->R] = &v.R;
	refs[ui->a] = &v.a;
	refs[ui->b] = &v.b;
	refs[ui->c] = &v.c;
	refs[ui->mv] = &v.MV;
	refs[ui->cl] = &v.CL;
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
