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

#include <QClipboard>
#include <QFileDialog>
#include <QTableWidget>
#include "common.h"
#include "multimodeloutput.h"
#include "ui_multimodeloutput.h"


MultiModelOutput::MultiModelOutput(QWidget *parent)
        : QDialog(parent)
{
	ui = new Ui::MultiModelOutput;
	ui->setupUi(this);

	connect(ui->tableWidget, SIGNAL(cellDoubleClicked(int,int)),
	        SLOT(itemDoubleClicked(int)));

	ui->modelData->horizontalHeader()->hide();
	ui->modelData->verticalHeader()->hide();

	connect(ui->copyButton, SIGNAL(clicked()), SLOT(copyToClipboard()));
	connect(ui->saveButton, SIGNAL(clicked()), SLOT(saveToFile()));
}

MultiModelOutput::~MultiModelOutput()
{
	delete ui;
}

void MultiModelOutput::setModels(QList<QPair<int, Model *> > data_models)
{
	models = data_models;
	updateView();
}

QString MultiModelOutput::toCSV(QChar sep) const
{
	/* Copies to clipboard using CSV format */
	const QLatin1Char quote('"');
	QStringList line, lines;

	// common model data
	for (int col=0; col<ui->modelData->columnCount(); ++col) {
		for (int row=0; row<ui->modelData->rowCount(); ++row)
			line << quote + ui->modelData->item(row, col)->text() + quote;

		lines << line.join(QString(sep));
		line.clear();
	}

	lines << QString() << QString();
	// headers
	for (int i=0; i<ui->tableWidget->columnCount(); ++i)
		line << quote + ui->tableWidget->horizontalHeaderItem(i)->text() + quote;
	lines << line.join(QString(sep));
	line.clear();

	// values
	for (int row=0; row<ui->tableWidget->rowCount(); ++row) {
		for(int col=0; col<ui->tableWidget->columnCount(); ++col)
			line << quote + ui->tableWidget->item(row, col)->text() + quote;

		lines << line.join(QString(sep));
		line.clear();
	}

	return lines.join(QString('\n'));
}

void MultiModelOutput::copyToClipboard() const
{
	QClipboard *clip = QApplication::clipboard();
	if (clip)
		clip->setText(toCSV());
}

void MultiModelOutput::saveToFile()
{
	QString filename = QFileDialog::getSaveFileName(this,
	                                                "Save CSV output...",
	                                                QString(),
	                                                "Text File (*.csv)");
	if (filename.isEmpty())
		return;

	QFile file(filename);
	if (file.open(QIODevice::WriteOnly))
		file.write(toCSV(',').toUtf8());
}

void MultiModelOutput::updateView()
{
	ui->tableWidget->clear();
	ui->tableWidget->setRowCount(models.size());
	if (models.size() < 1)
		return;

	// fill in static data - assume first model is has same information
	// as the rest
	Model *m = models.first().second;

	ui->modelData->setColumnCount(2);
	ui->modelData->setRowCount(2);

	ui->modelData->setItem(0, 0, new QTableWidgetItem(QLatin1String("# of Generations")));
	ui->modelData->setItem(0, 1, new QTableWidgetItem(QString::number(m->nGenerations())));

	ui->modelData->setItem(1, 0, new QTableWidgetItem(QLatin1String("Transducer pos.")));
	QString trans_pos;
	switch (m->transducerPos()) {
	case Model::Top:
		trans_pos = QLatin1String("Top");
		break;
	case Model::Middle:
		trans_pos = QLatin1String("Middle");
		break;
	case Model::Bottom:
		trans_pos = QLatin1String("Bottom");
		break;
	}
	ui->modelData->setItem(1, 1, new QTableWidgetItem(trans_pos));

	ui->modelData->setItem(2, 0, new QTableWidgetItem(QLatin1String("Tolerance")));
	ui->modelData->setItem(2, 1, new QTableWidgetItem(doubleToString(m->getResult(Model::Tlrns_value))));


	// add iterator count
	{
		QList<QString> values;
		bool all_the_same = true;
		values.reserve(models.count());

		for (ModelCalcList::const_iterator i=models.begin(); i!=models.end(); ++i) {
			switch (i->first) {
			case -2:
				values.append(QLatin1String("Target PAPm impossible"));
				break;
			default:
				values.append(QString::number(i->first));
				break;
			}

			all_the_same = all_the_same && values.back() == values.front();
		}

		if (all_the_same)
			values = QList<QString>() << values.first();
		insertValue("# Iterations", values);
	}

	// add calculated values
	QList<QPair<Model::DataType,QString> > types = QList<QPair<Model::DataType,QString> >()
	                << QPair<Model::DataType, QString>(Model::Rds_value, "Rds")
	                << QPair<Model::DataType, QString>(Model::Rm_value, "Rm")
	                << QPair<Model::DataType, QString>(Model::Rus_value, "Rus")
	                << QPair<Model::DataType, QString>(Model::TotalR_value, "PVR")
	                << QPair<Model::DataType, QString>(Model::PAP_value, "PAPm")

	                << QPair<Model::DataType, QString>(Model::CO_value, "CO")
	                << QPair<Model::DataType, QString>(Model::LAP_value, "LAP")
	                << QPair<Model::DataType, QString>(Model::Pal_value, "Pal")
	                << QPair<Model::DataType, QString>(Model::Ppl_value, "Ppl")
//	                << QPair<Model::DataType, QString>(Model::CL_value, "CL")
	                << QPair<Model::DataType, QString>(Model::PV_Diam_value, "PV Diameter")
	                << QPair<Model::DataType, QString>(Model::PV_EVL_value, "PV EVL")
	                << QPair<Model::DataType, QString>(Model::PA_Diam_value, "PA Diameter")
	                << QPair<Model::DataType, QString>(Model::PA_EVL_value, "PA EVL")

	                << QPair<Model::DataType, QString>(Model::Vtlc_value, "Vtlc")
	                << QPair<Model::DataType, QString>(Model::Vfrc_value, "Vfrc")
	                << QPair<Model::DataType, QString>(Model::Vrv_value, "Vrv")
	                << QPair<Model::DataType, QString>(Model::Vm_value, "Vm")

	                << QPair<Model::DataType, QString>(Model::Hct_value, "Hct")

	                << QPair<Model::DataType, QString>(Model::Lung_Ht_value, "Lung Height")
	                << QPair<Model::DataType, QString>(Model::Pat_Wt_value, "Patient Weight")
	                << QPair<Model::DataType, QString>(Model::Pat_Ht_value, "Patient Height");

	for (QList<QPair<Model::DataType,QString> >::const_iterator i=types.begin(); i!=types.end(); ++i) {
		QList<QString> values;
		bool all_the_same = true;
		values.reserve(models.count());

		for (ModelCalcList::const_iterator m=models.begin(); m!=models.end(); ++m) {
			values.append(doubleToString(m->second->getResult(i->first)));
			all_the_same = all_the_same && values.front() == values.back();
		}

		if (all_the_same)
			values = QList<QString>() << values.front();
		insertValue(i->second, values);
	}


	// Add disease parameters
	const DiseaseList diseases = models.first().second->diseases();
	const int n_disesases = diseases.size();
	for (int disease_no=0; disease_no<n_disesases; ++disease_no) {
		const Disease &d = diseases.at(disease_no);
		const QString disease_name = d.name();
		const int n_params = d.paramCount();

		for (int param_no=0; param_no<n_params; ++param_no) {
			const QString param_name = d.parameterName(param_no);
			const QString label = disease_name + "." + param_name;
			QList<QString> values;
			bool all_the_same = true;

			values.reserve(models.size());
			for (ModelCalcList::const_iterator m=models.begin(); m!=models.end(); ++m) {
				double val = m->second->diseases().at(disease_no).parameterValue(param_no);
				values.push_back(doubleToString(val));

				all_the_same = all_the_same && values.back() == values.front();
			}

			if (all_the_same)
				values = QList<QString>() << values.front();
			insertValue(label, values);
		}
	}
}

void MultiModelOutput::insertValue(QString header, const QList<QString> & values)
{
	/* insert value either at row 3 of modelData or column 1 of tableWidget,
	 * depending on spread of values
	 */

	if (values.size() == 1) { // append to left
		ui->modelData->insertRow(2);
		ui->modelData->setItem(2, 0, new QTableWidgetItem(header));
		ui->modelData->setItem(2, 1, new QTableWidgetItem(values.first()));
	}
	else {
		// list in table
		ui->tableWidget->insertColumn(0);
		ui->tableWidget->setHorizontalHeaderItem(0, new QTableWidgetItem(header));

		int row = 0;
		foreach (const QString &value, values)
			ui->tableWidget->setItem(row++, 0, new QTableWidgetItem(value));
	}
}

void MultiModelOutput::itemDoubleClicked(int row)
{
	qDebug("row: %d", row);
	if (row<0 || row>=models.size())
		return;

	emit doubleClicked(models[row].second, models[row].first);
}
