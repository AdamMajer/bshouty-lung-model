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

#include <QStringList>
#include <cmath>
#include "common.h"
#include "range.h"
#include <cmath>
#include <limits>

static const QLatin1Char step_sep(';');
static const QLatin1String range_sep(" to ");

Range::Range(const Range &other)
{
	v_min = other.v_min;
	v_max = other.v_max;
	v_step = other.v_step;
	valid = other.valid;
}

Range::Range(const QString &range)
{
	v_min = -20000;
	v_max = 20000;
	v_step = 0.01;
	valid = parseString(range, v_min, v_max, v_step);
}

Range& Range::operator =(const Range &other)
{
	v_min = other.v_min;
	v_max = other.v_max;
	v_step = other.v_step;
	valid = other.valid;

	return *this;
}

bool Range::operator ==(const Range &other) const
{
	return valid &&
	       other.valid &&
	       fabs(v_min-other.v_min) < 1e-16 &&
	       fabs(v_max-other.v_max) < 1e-16 &&
	       fabs(v_step-other.v_step) < 1e-16;
}

QString Range::toString() const
{
	const QList<double> &seq = sequence();

	if (seq.size() == 1)
		return doubleToString(seq.first());

	return doubleToString(v_min) + range_sep + doubleToString(v_max) +
	       step_sep + doubleToString(v_step);
}

void Range::setString(const QString &str)
{
	parseString(str, v_min, v_max, v_step);
}

bool Range::parseString(const QString &range, double &min, double &max, double &step)
{
	QStringList s = range.split(step_sep);
	if (s.size() > 2)
		return false;

	if (s.size() < 2)
		s.append("1"); // min step is 1, if none is present

	QStringList r = s.first().split(range_sep);
	if (r.size() > 2)
		return false;

	if (r.size() < 2)
		r.append(r.first()); // no range selected

	bool ok1, ok2, ok3;

	min = r.first().toDouble(&ok1);
	max = r.last().toDouble(&ok2);
	step = s.last().toDouble(&ok3);

	return ok1 && ok2 && ok3;
}

bool Range::isRange(const QString &range)
{
	QStringList s = range.split(step_sep);
	return s.size() == 2 && s.first().split(range_sep).size() == 2;
}

QList<double> Range::sequence() const
{
	QList<double> ret;
	double v = v_min;

	ret << v;
	while ((v+=v_step) < 0.001 + v_max)
		ret << v;

	return ret;
}

int Range::sequenceCount() const
{
	return lrint(floor((v_max - v_min) / v_step)) + 1;
}

bool Range::contains(const Range &other) const
{
	// avoid comparison with 0
	const double v_min_scaling = fabs(v_min) > std::numeric_limits<double>::min() ? fabs(v_min) : std::numeric_limits<double>::min();
	const double v_max_scaling = fabs(v_max) > std::numeric_limits<double>::min() ? fabs(v_max) : std::numeric_limits<double>::min();

	return other.v_min-v_min >= -std::numeric_limits<double>::epsilon()*v_min_scaling &&
	       other.v_max-v_max <=  std::numeric_limits<double>::epsilon()*v_max_scaling;
}
