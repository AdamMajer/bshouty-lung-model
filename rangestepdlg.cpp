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
#include "rangestepdlg.h"
#include "ui_rangestepdlg.h"
#include <math.h>

/* Only handles values in 0.01 step increments!!
 * on controls, 1 == 0.01 value.
 */

static const QLatin1Char step_sep(',');
static const QLatin1Char range_sep('-');

RangeStepDlg::RangeStepDlg(const Range &range, QWidget *parent)
        : QDialog(parent), r(range)
{
	ui = new Ui::RangeStepDlg;
	ui->setupUi(this);

	setMaxRange(range);
	setRange(range);

	connect(ui->stepSlider, SIGNAL(valueChanged(int)), SLOT(updateCalculatedValues()));
	connect(ui->minSlider, SIGNAL(valueChanged(int)), SLOT(updateCalculatedValues()));
	connect(ui->maxSlider, SIGNAL(valueChanged(int)), SLOT(updateCalculatedValues()));

	connect(ui->minSlider, SIGNAL(valueChanged(int)), SLOT(fixMaxSlider(int)));
	connect(ui->maxSlider, SIGNAL(valueChanged(int)), SLOT(fixMinSlider(int)));
}

RangeStepDlg::~RangeStepDlg()
{
	delete ui;
}

void RangeStepDlg::setMaxRange(const Range &m)
{
	// set control ranges
	ui->min->setMinimum(m.min());
	ui->min->setMaximum(m.max());

	ui->max->setMinimum(m.min());
	ui->max->setMaximum(m.max());

	ui->step->setMinimum(m.step());
	ui->step->setMaximum(20);

	ui->minSlider->setMaximum(lrint(m.max()*100));
	ui->minSlider->setMinimum(lrint(m.min()*100));
	ui->maxSlider->setMaximum(lrint(m.max()*100));
	ui->maxSlider->setMinimum(lrint(m.min()*100));

	int min_step = lrint(m.step()*100);
	if (min_step < 1)
		min_step = 1;
	ui->minSlider->setTickInterval(min_step);
	ui->maxSlider->setTickInterval(min_step);

	ui->stepSlider->setMaximum(2000);
	ui->stepSlider->setMinimum(min_step);
	ui->stepSlider->setSingleStep(min_step);
	ui->stepSlider->setSingleStep(min_step * 100);
}

void RangeStepDlg::setRange(const Range &value)
{
	r = value;

	r.setMin(qMax(r.min(), ui->minSlider->minimum()/100.0));
	r.setMax(qMin(r.max(), ui->maxSlider->maximum()/100.0));
	r.setStep(qMax(0.01, r.step()));

	ui->minSlider->blockSignals(true);
	ui->maxSlider->blockSignals(true);
	ui->stepSlider->blockSignals(true);

	ui->minSlider->setValue(lrint(r.min()*100));
	ui->maxSlider->setValue((int)(r.max()*100));
	ui->stepSlider->setValue(lrint(r.step()*100));

	ui->minSlider->blockSignals(false);
	ui->maxSlider->blockSignals(false);
	ui->stepSlider->blockSignals(false);

	updateCalculatedValues();
}

const Range& RangeStepDlg::range() const
{
	return r;
}

void RangeStepDlg::fixMinSlider(int max_slider_value)
{
	if (ui->minSlider->value() > max_slider_value)
		ui->minSlider->setValue(max_slider_value);
}

void RangeStepDlg::fixMaxSlider(int min_slider_value)
{
	if (ui->maxSlider->value() < min_slider_value)
		ui->maxSlider->setValue(min_slider_value);
}

void RangeStepDlg::updateCalculatedValues()
{
	r.setMin(ui->minSlider->value()/100.0);
	r.setMax(ui->maxSlider->value()/100.0);
	r.setStep(ui->stepSlider->value()/100.0);

	if (fabs(ui->min->value()-r.min()) > 1e-6)
		ui->min->setValue(r.min());
	if (fabs(ui->max->value()-r.max()) > 1e-6)
		ui->max->setValue(r.max());
	if (fabs(ui->step->value()-r.step()) > 1e-6)
		ui->step->setValue(r.step());

	// display calculated sequence
	QList<double> seq = r.sequence();

	QStringList seq_values;
	int n=0;
	for (QList<double>::const_iterator i=seq.begin(); i!=seq.end(); ++i, ++n) {
		seq_values << doubleToString(*i);
		if (n > 20) {
			seq_values << "...";
			break;
		}
	}

	ui->sequence->setText(seq_values.join(", "));
}

void RangeStepDlg::on_min_valueChanged(double val)
{
	ui->minSlider->setValue(val*100);
}

void RangeStepDlg::on_max_valueChanged(double val)
{
	ui->maxSlider->setValue(val*100);
}

void RangeStepDlg::on_step_valueChanged(double val)
{
	ui->stepSlider->setValue(val*100);
}
