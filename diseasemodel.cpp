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

	for(DiseaseList::const_iterator i=diseases.begin(); i!=diseases.end(); ++i)
		disease_list.push_back(*i);
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
		if (disease_index.row() == index.row() &&
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
			else
				return d.parameterValue(param_no);
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
	case Qt::EditRole:
		disease_list[index.parent().row()].setParameter(index.row(), value.toDouble());
		emit dataChanged(index, index);
		return true;
	}

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

void DiseaseModel::setDiseases(const DiseaseList &list)
{
	beginResetModel();
	disease_list.clear();
	for (DiseaseList::const_iterator i=list.begin(); i!=list.end(); ++i)
		disease_list.push_back(*i);
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
