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

#include <QDialog>
#include <QLineEdit>
#include "model/asyncrangemodelhelper.h"

namespace Ui {
	class CalibrateDlg;
}

struct CalibrationValue;
typedef QMap<QLineEdit*, QPair<QLatin1String, double> > ConfigValuesMap;

class CalibrateDlg : public QDialog
{
	Q_OBJECT

public:
	enum ValueType {Kra, Krv, Krc};

	CalibrateDlg(QWidget *parent);
	~CalibrateDlg();

	virtual void accept();

public slots:
	void on_calculateButton_clicked();
	void on_resetButton_clicked();

protected:
	struct CalibrationValue correctVariable(const struct CalibrationValue & value) const;
	void resetBaseModel();

protected slots:
	void calculationComplete();

private:
	Ui::CalibrateDlg *ui;

	std::list<CalibrationValue> calibration_values;
	AsyncRangeModelHelper *model_runner;
	double tlrns;

	const static int n_gen_progression[];
	int gen_pos;

	Model base_model;

	// setup values - <control, <settings_path, default_value>>
	ConfigValuesMap config_values;
};

struct CalibrationValue {
	CalibrateDlg::ValueType type;
	double input; // value modified
	bool is_modified;

	double current_value;
	double target;
};

