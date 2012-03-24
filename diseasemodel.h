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

#include <QAbstractItemModel>
#include "model/disease.h"

class DiseaseModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	enum DiseaseDataTypes {
		RangeMin = Qt::UserRole,
		RangeMax
	};

	DiseaseModel(QObject *parent);
	DiseaseModel(DiseaseList &diseases, QObject *parent);
	virtual ~DiseaseModel();

	virtual int columnCount(const QModelIndex &parent) const;
	virtual int rowCount(const QModelIndex &parent) const;

	virtual Qt::ItemFlags flags(const QModelIndex &index) const;

	virtual QVariant data(const QModelIndex &index, int role) const;
	virtual bool setData(const QModelIndex &index, const QVariant &value, int role);
	virtual QModelIndex index(int row, int column, const QModelIndex &parent) const;
	virtual QModelIndex parent(const QModelIndex &child) const;

	void addDisease(const Disease &);
	void removeDisease(const Disease &);
	void clear(); // remove all diseases

	DiseaseList diseases() const;
	std::vector<std::vector<Range> > diseaseParamRanges() const;
	void setDiseases(const DiseaseList &list);
	void setDiseases(const DiseaseList &list, std::vector<std::vector<Range> > param_ranges);

	// slew parameter for compromise model
	bool hasSlewParameter() const { return disease_index.isValid(); }
	Disease slewParameter(int &param_no) const;
	void setSlewParameter(const Disease &, int param_no);

	void load(QSqlDatabase db);
	void save(QSqlDatabase db) const;

public slots:
	/* called when user double clicks on a disease parameter in disease_view */
	void setCompromiseModelSlewParameter(const QModelIndex &index);

protected:
	void initDb(QSqlDatabase db) const;
	void loadVersion1Db(QSqlDatabase db);

private:
	std::vector<Disease> disease_list;
	std::vector<std::vector<Range> > disease_param_ranges;
	QModelIndex root_node, disease_index;

signals:
	/* signaled true when the model goes from state "rowCount()>0" to 0
	 * signaled false when a disease is added to empty model
	 */
	void emptiedModel(bool);
	void diseaseAdded(const QModelIndex &index);
};
