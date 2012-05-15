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

#include <QBrush>
#include "diseasemodel.h"
#include <algorithm>
#include <stdexcept>
#include <QSqlError>
#include <QSqlQuery>

#define HEADER_ID 0xFFFF

static quint32 diseaseId(unsigned disease_no, unsigned param_no)
{
	return disease_no | (param_no << 16);
}

static unsigned diseaseNo(quint32 id)
{
	return id & 0xFFFF;
}

static unsigned diseaseParamNo(quint32 id)
{
	return id >> 16;
}


DiseaseModel::DiseaseModel(QObject *parent)
        : QAbstractItemModel(parent)
{

}

DiseaseModel::DiseaseModel(DiseaseList &diseases, QObject *parent)
        : QAbstractItemModel(parent)
{
	disease_list.reserve(diseases.size());
	disease_param_ranges.reserve(diseases.size());

	for(DiseaseList::const_iterator i=diseases.begin(); i!=diseases.end(); ++i) {
		disease_list.push_back(*i);

		const int n_params = i->paramCount();
		std::vector<Range> params;
		params.reserve(n_params);

		for (int param_no=0; param_no<n_params; ++param_no)
			params.push_back(Range(QString::number(i->parameterValue(param_no), 'f')));

		disease_param_ranges.push_back(params);
	}
}

DiseaseModel::~DiseaseModel()
{

}

int DiseaseModel::columnCount(const QModelIndex &parent) const
{
	if (parent == root_node || parent.parent() == root_node)
		return 2;

	return 0;
}

int DiseaseModel::rowCount(const QModelIndex &parent) const
{
	if (parent == root_node)
		return disease_list.size();
	else if(diseaseParamNo(parent.internalId()) == HEADER_ID) {
		unsigned disease_no = diseaseNo(parent.internalId());

		if (disease_no < disease_list.size())
			return disease_list[disease_no].paramCount();

		return 0;
	}

	return 0;
}

Qt::ItemFlags DiseaseModel::flags(const QModelIndex &index) const
{
	if (index.parent() != root_node && index.column() == 1)
		return Qt::ItemIsEditable | Qt::ItemIsEnabled;

	return Qt::ItemIsEnabled;
}

QVariant DiseaseModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

	switch (role) {
	case Qt::BackgroundRole:
		if (diseaseParamNo(index.internalId()) == HEADER_ID)
			return QBrush(QColor(0xa0, 0xa0, 0xff));
		if (disease_index.isValid() &&
		    disease_index.internalId() == index.internalId())
			return QBrush(QColor(0xff, 0xff, 0xa0));
		break;
	case Qt::DisplayRole:
	case Qt::EditRole: {
		const int param_no = diseaseParamNo(index.internalId());
		if (param_no == HEADER_ID) {
			// disease headers
			if (index.column() == 0)
				return disease_list[index.row()].name();
		}
		else {
			// disease parameters
			int disease_no = diseaseNo(index.internalId());
			const Disease &d = disease_list[disease_no];

			if (index.column() == 0)
				return d.parameterName(param_no);
			return disease_param_ranges.at(disease_no).at(param_no).toString();
		}
		break;
	}
	case RangeMin: {
		int dis_param = diseaseParamNo(index.internalId());
		unsigned dis_no = diseaseNo(index.internalId());

		if (dis_no < disease_list.size())
			return disease_list[dis_no].paramRange(dis_param).min();
	}
	case RangeMax: {
		int dis_param = diseaseParamNo(index.internalId());
		unsigned dis_no = diseaseNo(index.internalId());

		if (dis_no < disease_list.size())
			return disease_list[dis_no].paramRange(dis_param).max();
	}
	}
	return QVariant();
}

bool DiseaseModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if (index.parent() == root_node)
		QAbstractItemModel::setData(index, value, role);

	switch (role) {
	case Qt::EditRole: {
		const int disease_no = diseaseNo(index.internalId());
		const int param_no = diseaseParamNo(index.internalId());
		Range range(value.toString());
		disease_list[disease_no].setParameter(index.row(), range.firstValue());
		disease_param_ranges[disease_no][param_no] = range;
		emit dataChanged(index, index);
		return true;
	} // Qt::EditRole
	} // switch

	return false;
}

QModelIndex DiseaseModel::index(int row, int column, const QModelIndex &parent) const
{
	if (parent == root_node) {
		if ((column == 0 || column == 1) && row < (int)disease_list.size())
			return createIndex(row, column, diseaseId(row, HEADER_ID));
	}
	else if (parent.parent() == root_node) {
		if (column >=0 && column <= 1)
			return createIndex(row, column, diseaseId(parent.row(), row));
	}

	return QModelIndex();
}

QModelIndex DiseaseModel::parent(const QModelIndex &child) const
{
	if (!child.isValid())
		return QModelIndex();

	if (child == root_node)
		return QModelIndex();

	if (diseaseParamNo(child.internalId()) == HEADER_ID)
		return root_node;

	return index(diseaseNo(child.internalId()), 0, root_node);
}

void DiseaseModel::addDisease(const Disease &d)
{
	int size = disease_list.size();

	beginInsertRows(root_node, size, size);
	disease_list.push_back(d);

	const int param_count = d.paramCount();
	std::vector<Range> params;
	params.reserve(param_count);
	for (int i=0; i<param_count; ++i)
		params.push_back(Range(QString::number(d.parameterValue(i), 'f')));

	disease_param_ranges.push_back(params);
	endInsertRows();

	emit diseaseAdded(index(size, 0, root_node));

	if (size == 0)
		emit emptiedModel(false);
}

void DiseaseModel::removeDisease(const Disease &d)
{
	const int len = disease_list.size();

	for (int row=0; row<len; row++) {
		if (disease_list[row].id()== d.id()) {
			beginRemoveRows(root_node, row, row);
			disease_list.erase(disease_list.begin() + row);
			disease_param_ranges.erase(disease_param_ranges.begin() + row);

			if (disease_index.isValid()) {
				int dis_no = diseaseNo(disease_index.internalId());
				int param_no = diseaseParamNo(disease_index.internalId());

				if (dis_no == row)
					disease_index = QModelIndex();
				else if(dis_no > row)
					disease_index = createIndex(param_no, 0,
					                            diseaseId(dis_no-1, param_no));
			}
			endRemoveRows();


			if (len == 1)
				emit emptiedModel(true);
			return;
		}
	}
}

void DiseaseModel::clear()
{
	beginResetModel();
	disease_list.clear();
	disease_index = QModelIndex();
	disease_param_ranges.clear();
	endResetModel();
}

DiseaseList DiseaseModel::diseases() const
{
	DiseaseList ret;

	for(std::vector<Disease>::const_iterator i=disease_list.begin();
	    i!=disease_list.end();
	    ++i)
		ret.push_back(*i);

	return ret;
}

std::vector<std::vector<Range> > DiseaseModel::diseaseParamRanges() const
{
	return disease_param_ranges;
}

void DiseaseModel::setDiseases(const DiseaseList &list)
{
	beginResetModel();
	disease_list.clear();
	disease_param_ranges.clear();
	for (DiseaseList::const_iterator i=list.begin(); i!=list.end(); ++i) {
		disease_list.push_back(*i);

		std::vector<Range> param_ranges;
		const int param_count = i->paramCount();
		for (int param_no=0; param_no<param_count; ++param_no)
			param_ranges.push_back(Range(QString::number(i->parameterValue(param_no), 'f')));
		disease_param_ranges.push_back(param_ranges);
	}
	disease_index = QModelIndex();
	endResetModel();
}

void DiseaseModel::setDiseases(const DiseaseList &list, std::vector<std::vector<Range> >param_ranges)
{
	beginResetModel();
	disease_list.clear();
	for (DiseaseList::const_iterator i=list.begin(); i!=list.end(); ++i)
		disease_list.push_back(*i);
	disease_param_ranges = param_ranges;
	disease_index = QModelIndex();
	endResetModel();
}

Disease DiseaseModel::slewParameter(int &param_no) const
{
	if (disease_index.isValid()) {
		param_no = diseaseParamNo(disease_index.internalId());
		return disease_list[diseaseNo(disease_index.internalId())];
	}

	throw std::runtime_error("Logic error: DiseaseModel::slewParameter() called when none was set.");
}

void DiseaseModel::setSlewParameter(const Disease &d, int param_no)
{
	std::vector<Disease>::const_iterator i=disease_list.begin();
	while (i!=disease_list.end()) {
		if (i->id() == d.id())
			break;
		++i;
	}
	if (i == disease_list.end())
		return;

	const int disease_no = i - disease_list.begin();
	const int nparams = (int)i->paramCount();

	if (param_no >= nparams)
		return;

	setCompromiseModelSlewParameter(index(param_no, 0, index(disease_no, 0, root_node)));
}

void DiseaseModel::load(QSqlDatabase db)
{
	QSqlQuery query(db);
	query.exec("SELECT version FROM disease_model_version");

	if (!query.next())
		throw std::runtime_error("Corrupted database file - cannot read");

	const int version = query.value(0).toInt();
	switch (version) {
	case 1:
		loadVersion1Db(db);
		break;
	default:
		throw std::runtime_error("Invalid database file - cannot read");
	}
}

void DiseaseModel::save(QSqlDatabase db) const
{
	initDb(db);

	// Save diseases and its parameters
	db.exec("DELETE FROM disease_model_params");
	db.exec("DELETE FROM disease_model");

	const int disease_count = disease_list.size();
	QSqlQuery param_query(db), disease_query(db);
	param_query.prepare("INSERT INTO disease_model_params "
	                    "(disease_id, param_no, selected_param, param_range) "
	                    "VALUES (?,?,?,?)");
	disease_query.prepare("INSERT INTO disease_model (disease_id, disease_script) VALUES (?,?)");

	for (int disease_no=0; disease_no<disease_count; ++disease_no) {
		const std::vector<Range> &param_ranges = disease_param_ranges.at(disease_no);
		const int param_count = param_ranges.size();

		disease_query.addBindValue(disease_no);
		disease_query.addBindValue(disease_list.at(disease_no).script());
		if (!disease_query.exec())
			throw std::runtime_error(disease_query.lastError().text().toUtf8().constData());

		for (int param_no=0; param_no<param_count; ++param_no) {
			const bool is_slew_param = diseaseId(disease_no, param_no) == disease_index.internalId();

			param_query.addBindValue(disease_no);
			param_query.addBindValue(param_no);
			param_query.addBindValue(is_slew_param);
			param_query.addBindValue(param_ranges.at(param_no).toString());
			if (!param_query.exec())
				throw std::runtime_error(param_query.lastError().text().toUtf8().constData());
		}
	}

}

void DiseaseModel::setCompromiseModelSlewParameter(const QModelIndex &idx)
{
	if (idx.column() != 0 || diseaseParamNo(idx.internalId()) == HEADER_ID)
		return;

	// all indexes that pass here are in first column
	// we ignore other column double clicks as those can be edit clicks.
	const QModelIndex old_disease_index = disease_index;
	disease_index = idx;

	if (old_disease_index.isValid())
		emit dataChanged(old_disease_index,
		                 index(old_disease_index.row(), 1, old_disease_index.parent()));
	emit dataChanged(idx, index(idx.row(), 1, idx.parent()));
}

void DiseaseModel::initDb(QSqlDatabase db) const
{
	const int cur_version = 1;
	QStringList db_sql[cur_version] =
	{
	        QStringList() <<
	        "CREATE TABLE disease_model_params (disease_id INTEGER NOT NULL, "
	             "param_no INTEGER NOT NULL, selected_param INTEGER NOT NULL, "
	             "param_range NOT NULL)" <<
	        "CREATE TABLE disease_model (disease_id INTEGER NOT NULL, disease_script NOT NULL)" <<
	        "CREATE TABLE disease_model_version (version INTEGER NOT NULL)" <<
	        "INSERT INTO disease_model_version VALUES(1)"
	};

	QSqlQuery q = db.exec("SELECT version FROM disease_model_version");
	int ver = 0;
	if (q.next())
		ver = q.value(0).toInt();

	while (ver < cur_version) {
		foreach (QString query, db_sql[ver]) {
			db.exec(query);

			if (db.lastError().type() != QSqlError::NoError)
				throw std::runtime_error(db.lastError().text().toUtf8().constData());
		}

		ver++;
	}
}

void DiseaseModel::loadVersion1Db(QSqlDatabase db)
{
	beginResetModel();
	disease_list.clear();
	disease_param_ranges.clear();
	disease_index = QModelIndex();

	QSqlQuery disease_query(db), param_query(db);
	if (!disease_query.exec("SELECT disease_id, disease_script FROM disease_model ORDER BY disease_id ASC"))
		throw std::runtime_error(disease_query.lastError().text().toUtf8().constData());
	param_query.prepare("SELECT selected_param, param_range FROM disease_model_params WHERE disease_id=? ORDER BY param_no ASC");

	while (disease_query.next()) {
		const int disease_id = disease_query.value(0).toInt();
		Disease disease(disease_query.value(1).toString());
		std::vector<Range> params;

		param_query.addBindValue(disease_id);
		if (!param_query.exec())
			throw std::runtime_error(disease_query.lastError().text().toUtf8().constData());

		while(param_query.next()) {
			if (params.size() >= (size_t)disease.paramCount())
				throw std::runtime_error("Too many disease parameters. Modified database.");

			Range r(param_query.value(1).toString());
			if (!r.isValid())
				throw std::runtime_error("Invalid parameter range in saved DB");

			if (param_query.value(0).toBool())
				disease_index = createIndex(params.size(), 0, diseaseId(disease_id, params.size()));
			params.push_back(r);
		}

		if (params.size() != (size_t)disease.paramCount())
			throw std::runtime_error("Number of disease parameters too small in saved file");

		disease_list.push_back(disease);
		disease_param_ranges.push_back(params);
	}

	endResetModel();
}
