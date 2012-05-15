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

#include <QMessageBox>
#include <QScriptEngine>
#include "scripteditdlg.h"
#include "scripthighlighter.h"
#include "ui_scripteditdlg.h"

ScriptEditDlg::ScriptEditDlg(const QString &txt, QWidget *parent)
        : QDialog(parent)
{
	ui = new Ui::ScriptEditDlg;
	ui->setupUi(this);

	ui->text->setText(txt);
	new ScriptHighlighter(ui->text);
}

ScriptEditDlg::~ScriptEditDlg()
{
	delete ui;
}

void ScriptEditDlg::setReadOnly(bool fReadOnly)
{
	ui->text->setReadOnly(fReadOnly);
}

void ScriptEditDlg::accept()
{
	// check if script is valid
	QScriptEngine e;
	e.evaluate(script());
	if (e.hasUncaughtException()) {
		QString msg = QString(QLatin1String("There is an error on line %1:\n%2"))
		                .arg(QString::number(e.uncaughtExceptionLineNumber()))
		                .arg(e.uncaughtException().toString());
		QMessageBox::critical(this, "Program syntax error", msg);
		return;
	}

	QDialog::accept();
}

QString ScriptEditDlg::script() const
{
	return ui->text->toPlainText();
}
