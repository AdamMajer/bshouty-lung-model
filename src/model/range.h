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

#ifndef RANGE_H
#define RANGE_H

#include <QString>

class Range
{
public:
	Range(const Range&);
	Range(const QString &range);

	Range& operator=(const Range&);
	bool operator==(const Range&) const;

	QString toString() const;
	void setString(const QString &str);

	static bool parseString(const QString &range,
	                        double &min, double &max, double &step);
	static bool isRange(const QString &range);

	double min() const { return v_min; }
	double max() const { return v_max; }
	double step() const { return v_step; }
	double firstValue() const { return min(); }

	void setMin(double min) { v_min = min; }
	void setMax(double max) { v_max = max; }
	void setStep(double step) { v_step = step; }

	QList<double> sequence() const;
	int sequenceCount() const;

	bool isValid() const { return valid; }
	bool contains(const Range &other) const;

private:
	double v_min, v_max, v_step;
	bool valid;
};

#endif // RANGE_H
