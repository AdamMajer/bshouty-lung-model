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

#ifndef COMMON_H
#define COMMON_H

#include <QString>

#ifdef _MSC_VER
#define _USE_MATH_DEFINES
long int lrint(double x);

#define fmax(a,b) (a<b?b:a)
#endif

#define PAH_ID  -20
#define PVOD_ID -10

extern QString doubleToString(double value);
extern QString doubleToString(double value, int n_decimals);
extern bool significantChange(double val1, double val2);
extern bool visibleChange(double val1, double val2);

const QLatin1String numeric_validator_rx("^[-]?\\d+(\\.\\d+)?$");

const QLatin1String settings_db("settings_db");
const QLatin1String app_name("BshoutyLungModel");

const QLatin1String settings_opencl_enabled("/settings/opencl_enabled"); // bool
const QLatin1String show_wizard_on_start("/settings/show_on_start"); // bool

// calibratino parameters
const QLatin1String rus_ratio("/settings/calibration/rus_ratio"); // double
const QLatin1String rm_ratio("/settings/calibration/rm_ratio"); // double
const QLatin1String rds_ratio("/settings/calibration/rds_ratio"); // double

const QLatin1String art_branching_ratio("/settings/calibration/art_branching"); // double
const QLatin1String art_length_ratio("/settings/calibration/art_length"); // double
const QLatin1String art_diam_ratio("/settings/calibration/art_diam"); // double
const QLatin1String vein_branching_ratio("/settings/calibration/veins_branching"); // double
const QLatin1String vein_length_ratio("/settings/calibration/veins_length"); // double
const QLatin1String vein_diam_ratio("/settings/calibration/veins_diam"); // double

const QLatin1String calibration_target_papm("/settings/calibration/target_papm"); // double

// allocate and free cacheline aligned (64-byte) and page aligned memory
void* allocateCachelineAligned(int size);
void* allocatePageAligned(int size);
void freeAligned(void *);

inline double sqr( double n )
{
	return n*n;
}

/* Globals */
class OpenCL;

extern OpenCL *cl;

#endif // COMMON_H
