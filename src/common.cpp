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

#include <QString>
#include <limits>
#include <math.h>
#include "common.h"

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

#ifdef _MSC_VER
long int lrint(double x)
{
	const double f = floor(x);
	const double c = ceil(x);
	return x-f < c-x ? (long int)(f+0.01) : (long int)(c+0.01);
}
#endif

QString doubleToString(double val)
{
	/* assume that data below 5 significant figures can be zeroed */
	if (fabs(val) < 1e-10)
		return QLatin1String("0.0");

	bool sig = val<0.0 ? true : false;
	double v = val;
	if (sig)
		v = -v;

	if (v < 0.0001)
		// use g notation, no need to do extra processing here
		return QString::number(val, 'g', 5);

	// decimal place of leading number, <0 value means numbers <1.
	double decimal_place = ceil(log10(v));

	v *= pow(10.0, 6.0 - decimal_place); // move decimal so there is 5 digits to left
	v = lrint(v);                        // round off
	v /= pow(10.0, 6.0 - decimal_place); // correct back to proper decimal

	// now value is properly rounded decimal. Print it with 5 sig figs
	int prec = decimal_place>4.9999 ? 0 : 5-(int)(decimal_place+0.1);

	QString ret = QString::number(val, 'f', prec);
	while (ret.endsWith('0') && --prec > 0) // cut off extra zeros
		ret.chop(1);

	return ret;

}

QString doubleToString(double value, int n_decimals)
{
	return QString::number(value, 'f', n_decimals);
}

bool significantChange(double old_val, double new_val)
{
	/* Returns true if old_val differs */
	double abs_max = std::max(fabs(old_val), fabs(new_val));

	return fabs(old_val - new_val) > std::numeric_limits<double>::epsilon() * abs_max;
}

bool visibleChange(double val1, double val2)
{
	return doubleToString(val1) != doubleToString(val2);
}

void* allocateCachelineAligned(int size)
{
#if defined(Q_OS_WIN)
	return _aligned_malloc(size, 256);
#elif defined(Q_OS_UNIX)
	void *memptr;

	return posix_memalign(&memptr, 256, size) == 0 ? memptr : NULL;
#endif
}

void* allocatePageAligned(int size)
{
#if defined(Q_OS_WIN)
	return _aligned_malloc(size, 4096);
#elif defined(Q_OS_UNIX)
	void *memptr;

	return posix_memalign(&memptr, getpagesize(), size) == 0 ? memptr : NULL;
#endif
}

void freeAligned(void *mem)
{
#if defined(Q_OS_WIN)
	_aligned_free(mem);
#elif defined(Q_OS_UNIX)
	free(mem);
#endif
}

QString animateDots(QString s)
{
	/*
	 * Animates dots in a QString
	 *   .... => *... => .*.. => ..*. => ...* => *...
	 */

	// NOTE: This is simple algorithm that *assues* that first dots
	//       are to be animated. If the first . found is not preceeded
	//       by a '*' and not followed by another . we return unmodified string.
	int len = s.length();
	QChar dot = QChar::fromLatin1('.');
	QChar o = QChar::fromLatin1('*');

	int pos = s.indexOf(dot);
	if (pos <= 0)
		return s;

	if (s.at(pos-1) == o) {
		s[pos-1] = dot;
		s[pos] = o;
	}
	else {
		// find 'o' and move it forward
		int o_pos = pos+1;
		while (o_pos<len && s[o_pos]==dot)
			o_pos++;

		if (o_pos == len || s[o_pos] != o) {
			// o not found, set first char as o
			s[pos] = o;
		}
		else if (o_pos+1==len || s[o_pos+1] != dot) {
			// last position, cycle back to beginning
			s[pos] = o;
			s[o_pos] = dot;
		}
		else {
			// move it forward by one
			s[o_pos] = dot;
			s[++o_pos] = o;
		}
	}

	return s;
}
