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

#include "diseaselistdlg.h"
#include "scripteditdlg.h"
#include "ui_diseaselistdlg.h"
#include "sample_function.h"
#include <QMessageBox>
#include <QSqlDatabase>

DiseaseListDlg::DiseaseListDlg(QWidget *parent)
        : QDialog(parent)
{
	ui = new Ui::DiseaseListDlg;
	ui->setupUi(this);

	const DiseaseList &dl = Disease::allDiseases();
	for (DiseaseList::const_iterator i=dl.begin(); i!=dl.end(); ++i)
		addDisease(*i);
}

DiseaseListDlg::~DiseaseListDlg()
{
	delete ui;
}

void DiseaseListDlg::on_diseaseList_currentRowChanged(int row)
{
	bool editable = row != -1;
	bool deletable = editable;

	if (editable) {
		QListWidgetItem *item = ui->diseaseList->item(row);
		if (item) {
			int id = item->data(Qt::UserRole).toInt();
			Disease d = diseaseFromId(id);
			editable = !d.isReadOnly();
			deletable = d.id() >= 0;
		}
	}

	ui->editButton->setEnabled(row != -1);
	if (editable)
		ui->editButton->setText("Edit...");
	else
		ui->editButton->setText("View...");
	ui->removeButton->setEnabled(deletable);
}

void DiseaseListDlg::on_editButton_clicked()
{
	if (ui->diseaseList->selectedItems().isEmpty())
		return;

	QListWidgetItem *sel = ui->diseaseList->selectedItems().first();
	int id = sel->data(Qt::UserRole).toInt();
	Disease disease = diseaseFromId(id);
	ScriptEditDlg dlg(disease.script(), this);

	dlg.setReadOnly(disease.isReadOnly());

	if (dlg.exec() == QDialog::Accepted && dlg.script() != disease.script()) {
		Disease d(dlg.script());

		if (d.isValid()) {
			Disease::deleteDisease(disease.id());
			d.save();
			sel->setText(d.name());
			sel->setData(Qt::UserRole, d.id());
		}
	}
}

void DiseaseListDlg::on_newButton_clicked()
{
	ScriptEditDlg dlg(sample_function_js, this);
	if (dlg.exec() == QDialog::Accepted) {
		Disease d(dlg.script());
		d.save();

		addDisease(d);
	}
}

void DiseaseListDlg::on_removeButton_clicked()
{
	if (ui->diseaseList->selectedItems().isEmpty())
		return;

	QListWidgetItem *sel = ui->diseaseList->selectedItems().first();
	int id = sel->data(Qt::UserRole).toInt();
	{
		const Disease &d = diseaseFromId(id);
		if (d.isReadOnly() &&
		    QMessageBox::question(this, "Remove read only disease",
		        "This disease is marked as 'protected' by its author. Delete it anyway?",
		        QMessageBox::Yes, QMessageBox::No | QMessageBox::Default) != QMessageBox::Yes) {

			return;
		}
	}

	if (Disease::deleteDisease(id))
		delete sel;
}

void DiseaseListDlg::addDisease(const Disease &d)
{
	QListWidgetItem *item = new QListWidgetItem(d.name());
	item->setData(Qt::UserRole, d.id());
	ui->diseaseList->addItem(item);
}

Disease DiseaseListDlg::diseaseFromId(int id)
{
	const DiseaseList &diseases = Disease::allDiseases();

	for (DiseaseList::const_iterator i=diseases.begin(); i!=diseases.end(); ++i)
		if (i->id() == id)
			return *i;

	throw "Error";
}
