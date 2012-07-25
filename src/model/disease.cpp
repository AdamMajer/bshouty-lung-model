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

#include <QScriptEngine>
#include <QScriptValueIterator>
#include <QSqlQuery>
#include "common.h"
#include "disease.h"
#include "model.h"
#include <QDebug>

static bool diseases_loaded;
static int int_disease_id;
static DiseaseList all_diseases;

Disease::Disease(const QString &s)
        : program(s)
{
	if (!diseases_loaded)
		loadAllDiseases();

	for (DiseaseList::const_iterator i=all_diseases.begin(); i!=all_diseases.end(); ++i) {
		if (i->script() == s) {
			*this = *i;
			return;
		}
	}

	is_readonly = false;

	/* Parse the script and extract name, description, and paramters */
	QScriptEngine e;
	QScriptValue prog = e.evaluate(program);
	QScriptValue v = prog.property("name");
	if (v.isFunction()) {
		v = v.call();
		if (v.isString())
			cached_name = v.toString();
		else if (v.isArray()) {
			cached_name = v.property(0).toString();
			cached_desc = v.property(1).toString();
			is_readonly = v.property(2).toBoolean();
		}
	}

	v = prog.property("parameters");
	if (v.isFunction() && (v=v.call()).isArray()) {
		QScriptValueIterator params(v);

		while (params.hasNext()) {
			params.next();

			if (params.flags() & QScriptValue::SkipInEnumeration)
				continue;
			QStringList v;
			QScriptValue val = params.value();

			if (val.isArray()) {
				QScriptValueIterator options(val);
				while (options.hasNext()) {
					options.next();

					if (options.flags() & QScriptValue::SkipInEnumeration)
						continue;
					v << options.value().toString();
				}
			}
			else
				v << params.value().toString();

			while (v.size() < 4)
				v.append(QString());

			parameters.push_back(Parameter(v[0], Range(v[1]), v[2].toDouble(), v[3]));
		}
	}


	script_id = ++int_disease_id;
	all_diseases.push_back(*this);
}

Disease::Disease(const Disease &o)
        : program(o.program),
          cached_name(o.cached_name), cached_desc(o.cached_desc),
          parameters(o.parameters),
          script_id(o.script_id),
          is_readonly(o.is_readonly)
{

}

Disease& Disease::operator =(const Disease &o)
{
	program = o.program;
	cached_name = o.cached_name;
	cached_desc = o.cached_desc;
	parameters = o.parameters;
	script_id = o.script_id;
	is_readonly = o.is_readonly;

	return *this;
}

bool Disease::operator ==(const Disease &o) const
{
	return o.program == program &&
	       o.parameters == parameters &&
	       o.script_id == script_id;
}

QString Disease::name() const
{
	return cached_name;
}

QString Disease::script() const
{
	return program.sourceCode();
}

bool Disease::isValid() const
{
	return !name().isEmpty();
}

QString Disease::parameterName(int n) const
{
	if (static_cast<unsigned>(n)>=parameters.size())
		return QString();

	return parameters.at(n).name;
}

QStringList Disease::parameterNameList() const
{
	QStringList ret;
	unsigned size = parameters.size();

	for (unsigned i=0; i<size; ++i)
		ret << parameters.at(i).name;

	return ret;
}

double Disease::parameterValue(const QString &param) const
{
	unsigned size = parameters.size();
	for (unsigned i=0; i<size; ++i)
		if (parameters.at(i).name == param)
			return parameters.at(i).value;

	return 0.0;
}

double Disease::parameterValue(int n) const
{
	if (static_cast<unsigned>(n) < parameters.size())
		return parameters.at(n).value;

	return 0.0;
}

void Disease::setParameter(const QString &param, double val)
{
	const unsigned size = parameters.size();
	for (unsigned i=0; i<size; ++i)
		if (parameters.at(i).name == param)
			parameters.at(i).value = val;
}

void Disease::setParameter(int n, double val)
{
	if ((unsigned)n < parameters.size())
		parameters.at(n).value = val;
}

Range Disease::paramRange(int n) const
{
	if ((unsigned)n < parameters.size())
		return parameters.at(n).range;

	return Range(QString::null);
}

#define SET_GLOBAL_PROPERTY(a) global.setProperty("lung_"#a, model.getResult(Model::a##_value))
void Disease::processModel(Model &model)
{
	QScriptEngine e;
	QScriptValue p = e.evaluate(program);

	QScriptValue global = e.globalObject();
	QScriptValue artery_function = p.property("artery", QScriptValue::ResolveLocal);
	QScriptValue vein_function = p.property("vein", QScriptValue::ResolveLocal);
	QScriptValue cap_function = p.property("cap", QScriptValue::ResolveLocal);

	// Assign model's properties to the script
	SET_GLOBAL_PROPERTY(Lung_Ht);
	SET_GLOBAL_PROPERTY(Flow);
	SET_GLOBAL_PROPERTY(LAP);
	SET_GLOBAL_PROPERTY(Pal);
	SET_GLOBAL_PROPERTY(Ppl);
	SET_GLOBAL_PROPERTY(Ptp);
	SET_GLOBAL_PROPERTY(PAP);
	SET_GLOBAL_PROPERTY(Tlrns);
	SET_GLOBAL_PROPERTY(Pat_Ht);
	SET_GLOBAL_PROPERTY(Pat_Wt);
	global.setProperty("n_gen", model.nGenerations());

	// Process vessels
	if (artery_function.isFunction()) {
		int n_gens = model.nGenerations();
		for (int i=1; i<=n_gens; ++i) {
			int n_art = model.nElements(i);
			global.setProperty("n_vessels", n_art);
			for (int j=0; j<n_art; ++j)
				processArtery(model, global, artery_function, i, j);
		}
	}

	if (vein_function.isFunction()) {
		int n_gens = model.nGenerations();
		for (int i=1; i<=n_gens; ++i) {
			int n_veins = model.nElements(i);
			global.setProperty("n_vessels", n_veins);
			for (int j=0; j<n_veins; ++j)
				processVein(model, global, vein_function, i, j);
		}
	}

	if (cap_function.isFunction()) {
		int n_caps = model.numCapillaries();
		global.setProperty("n_vessels", n_caps);
		for (int i=0; i<n_caps; ++i)
			processCapillary(model, global, cap_function, i);
	}
}
#undef SET_GLOBAL_PROPERTY

void Disease::save()
{
	QSqlQuery q(QSqlDatabase::database(settings_db));
	qDebug("save: %d", script_id);
	q.prepare("INSERT INTO diseases (id, name, script) VALUES (?,?,?)");
	q.addBindValue(script_id);
	q.addBindValue(name());
	q.addBindValue(script());
	q.exec();
}

int Disease::id() const
{
	return script_id;
}

bool Disease::isReadOnly() const
{
	return is_readonly || script_id<0;
}

DiseaseList Disease::allDiseases()
{
	if (!diseases_loaded)
		loadAllDiseases();

	return all_diseases;
}

bool Disease::deleteDisease(int id)
{
	for (DiseaseList::iterator i=all_diseases.begin(); i!=all_diseases.end(); ++i) {
		if (i->id() == id) {
			if (i->isReadOnly() && id<0)
				return false;

			deleteDisease(i);
			return true;
		}
	}

	return false;
}

void Disease::processArtery(Model &m, QScriptValue &global, QScriptValue &f, int gen, int ves_no)
{
	Vessel v = m.artery(gen, ves_no);
	processVessel(v, global, f, gen, ves_no);

	if (v == m.artery(gen, ves_no))
		return;
	m.setArtery(gen, ves_no, v, false);
}

void Disease::processVein(Model &m, QScriptValue &global, QScriptValue &f, int gen, int ves_no)
{
	Vessel v = m.vein(gen, ves_no);
	processVessel(v, global, f, gen, ves_no);

	if (v == m.vein(gen, ves_no))
		return;
	m.setVein(gen, ves_no, v, false);
}

#define SET_PROPERTY(a) global.setProperty(#a, v.a)
#define GET_PROPERTY(a) v.a = global.property(#a).toNumber()
void Disease::processVessel(Vessel &v, QScriptValue &global, QScriptValue &f, int gen, int ves_no)
{
	global.setProperty("gen", gen);
	global.setProperty("vessel_idx", ves_no);
	SET_PROPERTY(D);
	SET_PROPERTY(a);
	SET_PROPERTY(b);
	SET_PROPERTY(c);
	SET_PROPERTY(tone);
	SET_PROPERTY(GP);
	SET_PROPERTY(Ppl);
	SET_PROPERTY(Ptp);
	SET_PROPERTY(perivascular_press_a);
	SET_PROPERTY(perivascular_press_b);
	SET_PROPERTY(perivascular_press_c);

	if (!callScript(f))
		return;

	GET_PROPERTY(a);
	GET_PROPERTY(b);
	GET_PROPERTY(c);
	GET_PROPERTY(tone);
	GET_PROPERTY(GP);
	GET_PROPERTY(Ppl);
	GET_PROPERTY(Ptp);
	GET_PROPERTY(perivascular_press_a);
	GET_PROPERTY(perivascular_press_b);
	GET_PROPERTY(perivascular_press_c);
	GET_PROPERTY(D);
}

void Disease::processCapillary(Model &m, QScriptValue &global, QScriptValue &f, int ves_no)
{
	Capillary v = m.capillary(ves_no);

	global.setProperty("vessel_idx", ves_no);
	SET_PROPERTY(R);
	SET_PROPERTY(Alpha);
	SET_PROPERTY(Ho);

	if (!callScript(f))
		return;

	GET_PROPERTY(Alpha);
	GET_PROPERTY(Ho);
	GET_PROPERTY(R);

	if (v == m.capillary(ves_no))
		return;
	m.setCapillary(ves_no, v, false);
}

bool Disease::callScript(QScriptValue &f)
{
	QScriptValueList params;
	const unsigned n = parameters.size();
	for (unsigned i=0; i<n; ++i)
		params.append(parameters.at(i).value);

	QScriptValue ret = f.call(QScriptValue(), params);
	if (ret.isError() || !ret.toBool())
		return false;

	return true;
}

void Disease::deleteDisease(DiseaseList::iterator i)
{
	QSqlQuery q(QSqlDatabase::database(settings_db));
	q.exec("DELETE FROM diseases WHERE id=" + QString::number(i->script_id));

	// FIXME: Remvoe when sqlite finally supports droping rows due to foreign key constraints
	q.exec("DELETE FROM disease_parameters WHERE disease_id=" + QString::number(i->script_id));
	all_diseases.erase(i);
}

void Disease::loadAllDiseases()
{
	if (diseases_loaded)
		return;

	diseases_loaded = true;

	QSqlDatabase db = QSqlDatabase::database(settings_db);
	QSqlQuery q(db), params(db);
	q.exec("SELECT id, name, script FROM diseases ORDER BY id");
	params.prepare("SELECT param_name, value FROM disease_parameters "
	               "WHERE disease_id=? AND value IS NOT NULL");
	bool failed_ids = false;
	int max_id = 0;
	while (q.next()) {
		/* Parse script and pushes default values to back of the list */
		Disease(q.value(2).toString());

		/* Fetch the object that was stored on the list, and modify it */
		Disease &d = all_diseases.back();
		bool ok;

		d.script_id = q.value(0).toInt(&ok);
		max_id = qMax(d.script_id, max_id);
		if (!ok) {
			d.script_id = -1;
			failed_ids = true;
			continue;
		}

		params.addBindValue(d.script_id);
		params.exec();
		while (params.next())
			d.setParameter(params.value(0).toString(),
			               params.value(1).toDouble());
	}

	int_disease_id = max_id;

	if (failed_ids)
		for (DiseaseList::iterator i=all_diseases.begin(); i!=all_diseases.end(); i++)
			if (i->script_id == -1)
				i->script_id = ++int_disease_id;
}
