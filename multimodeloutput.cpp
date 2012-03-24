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

	headers << Model::CO_value
	        << Model::LAP_value
	        << Model::Pal_value
	        << Model::Ppl_value
	        << Model::PAP_value
	        << Model::TotalR_value
	        << Model::Rus_value
	        << Model::Rm_value
	        << Model::Rds_value
	        << Model::Rus_value;

	ui->modelData->horizontalHeader()->hide();
	ui->modelData->verticalHeader()->hide();

	connect(ui->copyButton, SIGNAL(clicked()), SLOT(copyToClipboard()));
	connect(ui->saveButton, SIGNAL(clicked()), SLOT(saveToFile()));
}

MultiModelOutput::~MultiModelOutput()
{
	delete ui;
}

void MultiModelOutput::setHeaders(QList<Model::DataType> hdrs)
{
	headers = hdrs;
	updateView();
}

void MultiModelOutput::setModels(QList<Model *> data_models)
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
	int n_cols = headers.size();

	ui->tableWidget->clear();
	ui->tableWidget->setColumnCount(n_cols);
	ui->tableWidget->setRowCount(models.size());

	QStringList labels;

	if (models.size() < 1)
		return;

	foreach (Model::DataType data, headers) {
		switch (data) {
		case Model::Lung_Ht_value:
			labels << QLatin1String("Lung Height");
			break;
		case Model::LAP_value:
			labels << QLatin1String("LAP");
			break;
		case Model::Pal_value:
			labels << QLatin1String("Pal");
			break;
		case Model::Ppl_value:
			labels << QLatin1String("Ppl");
			break;
		case Model::Ptp_value:
			labels << QLatin1String("Ptp");
			break;
		case Model::PAP_value:
			labels << QLatin1String("PAP");
			break;
		case Model::Rus_value:
			labels << QLatin1String("Rus");
			break;
		case Model::Rds_value:
			labels << QLatin1String("Rds");
			break;
		case Model::Rm_value:
			labels << QLatin1String("Rm");
			break;
		case Model::Rt_value:
			labels << QLatin1String("Rt");
			break;
		case Model::PciA_value:
			labels << QLatin1String("PciA");
			break;
		case Model::PcoA_value:
			labels << QLatin1String("PcoA");
			break;
		case Model::Tlrns_value:
			labels << QLatin1String("Tolarance");
			break;
		case Model::MV_value:
			labels << QLatin1String("MV");
			break;
		case Model::CL_value:
			labels << QLatin1String("CL");
			break;
		case Model::Pat_Ht_value:
			labels << QLatin1String("Patient Height");
			break;
		case Model::Pat_Wt_value:
			labels << QLatin1String("Patient Weight");
			break;
		case Model::TotalR_value:
			labels << QLatin1String("Total R");
			break;
		case Model::CO_value:
			labels << QLatin1String("CO");
			break;
		case Model::DiseaseParam:
			break;
		}
	}
	ui->tableWidget->setHorizontalHeaderLabels(labels);

	int row = 0;
	foreach (Model *model, models) {
		for (int col=0; col<n_cols; ++col) {
			QString value = doubleToString(model->getResult(headers[col]));
			ui->tableWidget->setItem(row, col, new QTableWidgetItem(value));
		}

		++row;
	}

	// fill in static data - assume first model is has same information
	// as the rest
	Model *m = models.first();

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

	QList<QPair<Model::DataType,QString> > types = QList<QPair<Model::DataType,QString> >()
	                << QPair<Model::DataType, QString>(Model::CL_value, "CL")
	                << QPair<Model::DataType, QString>(Model::MV_value, "Vm")
	                << QPair<Model::DataType, QString>(Model::Lung_Ht_value, "Lung Height")
	                << QPair<Model::DataType, QString>(Model::Pat_Wt_value, "Patient Weight")
	                << QPair<Model::DataType, QString>(Model::Pat_Ht_value, "Patient Height");

	for (QList<QPair<Model::DataType,QString> >::const_iterator i=types.begin(); i!=types.end(); ++i) {
		QList<double> values;
		values.reserve(models.count());

		foreach (Model *model, models)
			values.append(model->getResult(i->first));

		insertValue(i->second, values);
	}


	// Add disease parameters
	const DiseaseList diseases = models.first()->diseases();
	const int n_disesases = diseases.size();
	for (int disease_no=0; disease_no<n_disesases; ++disease_no) {
		const Disease &d = diseases.at(disease_no);
		const QString disease_name = d.name();
		const int n_params = d.paramCount();

		for (int param_no=0; param_no<n_params; ++param_no) {
			const QString param_name = d.parameterName(param_no);
			const QString label = disease_name + "." + param_name;
			QList<double> values;

			values.reserve(models.size());
			foreach (Model *m, models)
				values.push_back(m->diseases().at(disease_no).parameterValue(param_no));

			insertValue(label, values);
		}
	}
}

void MultiModelOutput::insertValue(QString header, QList<double> values)
{
	/* insert value either at row 3 of modelData or column 1 of tableWidget,
	 * depending on spread of values
	 */

	double val = values.first();

	foreach (double v, values)
		if (fabs(val - v) > 1e-6) {
			// list in table
			ui->tableWidget->insertColumn(0);
			ui->tableWidget->setHorizontalHeaderItem(0,
			              new QTableWidgetItem(header));

			int row = 0;
			foreach (double v, values) {
				ui->tableWidget->setItem(row++, 0,
				      new QTableWidgetItem(doubleToString(v)));
			}

			return;
		}

	// append to left
	//ui->modelData->setRowCount(ui->modelData->rowCount() + 1);
	ui->modelData->insertRow(2);
	ui->modelData->setItem(2, 0, new QTableWidgetItem(header));
	ui->modelData->setItem(2, 1, new QTableWidgetItem(doubleToString(val)));
}

void MultiModelOutput::itemDoubleClicked(int row)
{
	qDebug("row: %d", row);
	if (row<0 || row>=models.size())
		return;

	emit doubleClicked(models[row]);
}
