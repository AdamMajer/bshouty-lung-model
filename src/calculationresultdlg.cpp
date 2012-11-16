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
	for (std::map<double, DeltaRStruct>::const_reverse_iterator i=values.rbegin(); i!=values.rend(); ++i) {
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

	ui->vesselTable->horizontalHeader()->setResizeMode(0, QHeaderView::ResizeToContents);
	ui->vesselTable->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);
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
