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

#include <QTextStream>
#include "calculationresultdlg.h"
#include "vesselview.h"
#include "ui_calculationresultdlg.h"
#include <map>

struct DeltaRStruct {
	int gen, idx;
	VesselView::Type type;

	DeltaRStruct() {}
	DeltaRStruct(int g, int i, VesselView::Type t) : gen(g), idx(i), type(t) {}
};

Q_DECLARE_METATYPE(DeltaRStruct)


/* NOTE: the passed Model& may disappear after contructor - do not store */
CalculationResultDlg::CalculationResultDlg(QWidget *parent, const Model &m)
        : QDialog(parent)
{
	ui = new Ui::CalculationResultDlg;
	ui->setupUi(this);

	// find 10 largest DeltaR valued vessels
	double av_delta_r = 0.0;
	double smallest_delta_r = 0.0;
	std::multimap<double, DeltaRStruct> values;
	for (int i=0; i<10; ++i)
		values.insert(std::pair<double,DeltaRStruct>(0.0, DeltaRStruct()));

	for (int gen=1; gen<=m.nGenerations(); ++gen) {
		int n_elem = m.nElements(gen);

		for (int i=0; i<n_elem; ++i) {
			const Vessel &art = m.artery(gen, i);
			const Vessel &vein = m.vein(gen, i);

			av_delta_r += art.last_delta_R;
			av_delta_r += vein.last_delta_R;

			if (smallest_delta_r < art.last_delta_R) {
				DeltaRStruct v(gen, i, VesselView::Artery);
				values.erase(values.begin());
				values.insert(std::pair<double,DeltaRStruct>(art.last_delta_R, v));
				smallest_delta_r = values.begin()->first;
			}

			if (smallest_delta_r < vein.last_delta_R) {
				DeltaRStruct v(gen, i, VesselView::Vein);
				values.erase(values.begin());
				values.insert(std::pair<double,DeltaRStruct>(vein.last_delta_R, v));
				smallest_delta_r = values.begin()->first;
			}
		}
	}
	int n_cap = m.numCapillaries();
	for (int i=0; i<n_cap; ++i) {
		const Capillary &c = m.capillary(i);

		av_delta_r += c.last_delta_R;

		if (smallest_delta_r < c.last_delta_R) {
			DeltaRStruct v(m.nGenerations(), i, VesselView::Capillary);
			values.erase(values.begin());
			values.insert(std::pair<double,DeltaRStruct>(c.last_delta_R, v));
			smallest_delta_r = values.begin()->first;
		}
	}

	// and add the vessels to the table
	ui->vesselTable->setRowCount(10);
	int table_row = 0;
	for (std::multimap<double, DeltaRStruct>::const_reverse_iterator i=values.rbegin(); i!=values.rend(); ++i) {
		QString label = VesselView::vesselToStringTitle(i->second.type, i->second.gen, i->second.idx);
		label.append(QString::fromLatin1(" Gen: %1").arg(i->second.gen));

		QTableWidgetItem *label_item = new QTableWidgetItem(label);
		QTableWidgetItem *value_item = new QTableWidgetItem(doubleToString(i->first*100, 3) +
		                                                    QChar::fromLatin1('%'));

		QVariant d = QVariant::fromValue(i->second);
		label_item->setData(Qt::UserRole, d);
		value_item->setData(Qt::UserRole, d);

		ui->vesselTable->setItem(table_row, 0, label_item);
		ui->vesselTable->setItem(table_row, 1, value_item);

		table_row++;
	}


	av_delta_r /= (2*m.nElements() + n_cap);

	ui->iterationCount->setText(QString::number(m.numIterations()));
	ui->avDeltaR->setText(doubleToString(av_delta_r * 100, 3) +
	                      QChar::fromLatin1('%'));
	ui->targetDeltaR->setText(doubleToString(m.getResult(Model::Tlrns_value) * 100, 3) +
	                          QChar::fromLatin1('%'));

	ui->vesselTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui->vesselTable->setSelectionBehavior(QAbstractItemView::SelectRows);

#if QT_VERSION >= 0x050000
	ui->vesselTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	ui->vesselTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
#else
	ui->vesselTable->horizontalHeader()->setResizeMode(0, QHeaderView::ResizeToContents);
	ui->vesselTable->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);
#endif
	ui->vesselTable->setAlternatingRowColors(true);
}

void CalculationResultDlg::on_vesselTable_itemDoubleClicked(QTableWidgetItem *item)
{
	if (item == 0)
		return;

	QVariant d = item->data(Qt::UserRole);
	if (d.isNull())
		return;
	DeltaRStruct data = d.value<DeltaRStruct>();

	emit highlightVessel(data.type, data.gen, data.idx);
}
