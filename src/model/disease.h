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

#ifndef DISEASE_H
#define DISEASE_H

#include <list>
#include <vector>
#include <QString>
#include <QStringList>
#include <QScriptProgram>
#include <QScriptValue>
#include <QSqlDatabase>
#include "common.h"
#include "range.h"
#include <cmath>

class Disease;
class Model;
struct Vessel;
struct Parameter;
typedef std::vector<Disease> DiseaseList;

struct Parameter {
	QString name, desc;
	Range range;
	double value;

	Parameter(const QString &n, const Range &r, double v=0, const QString &d=QString::null)
	        :name(n), desc(d), range(r), value(v) {}

	bool operator== (const Parameter &o) const {
		return name == o.name &&
		       desc == o.desc &&
		       range == o.range &&
		       fabs(value - o.value) < 1e-16;
	}
};

class Disease
{
public:
	Disease(const Disease &other);

	Disease& operator =(const Disease &other);
	bool operator==(const Disease &other) const;

	QString name() const;
	QString description() const;
	QString script() const;
	bool isValid() const;

	QString parameterName(int n) const;
	QString parameterDesc(int n) const;
	QStringList parameterNameList() const;
	double parameterValue(const QString &param) const;
	double parameterValue(int n) const;
	void setParameter(const QString &param, double val);
	void setParameter(int n, double val);
	Range paramRange(int n) const;

	void processModel(Model &model);
	void save(); // inserts disease into the database

	int id() const;
	bool isReadOnly() const;
	int paramCount() const { return parameters.size(); }

	static Disease fromString(const QString &script);
	static DiseaseList allDiseases();
	static bool deleteDisease(int id);
	static void swapDisease(int first, int second);

protected:
	Disease(const QString &script);

	void processArtery(Model &m, QScriptValue &global, QScriptValue &f, int gen, int ves_no);
	void processVein(Model &m, QScriptValue &global, QScriptValue &f, int gen, int ves_no);
	void processVessel(Vessel &v, QScriptValue &global, QScriptValue &f, int gen, int ves_no);
	void processCapillary(Model &m, QScriptValue &global, QScriptValue &f, int ves_no);
	bool callScript(QScriptValue &f);

	static void deleteDisease(DiseaseList::iterator);

private:
	static void loadAllDiseases();

	QScriptProgram program;
	QString cached_name, cached_desc;
	std::vector<struct Parameter> parameters;
	int script_id;

	bool is_readonly;
};

#endif // DISEASE_H
