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

#include "capillary.h"
#include "common.h"
#include "ui_capillary.h"
#include <QRegExpValidator>


//=========================================================
CapillaryDlg::CapillaryDlg( Capillary & cap, QWidget *parent ) : QDialog( parent ), c( cap )
{
	ui = new Ui::Capillary;
	ui->setupUi( this );

	ui->alpha->setText(doubleToString( cap.Alpha ));
	ui->ho->setText(doubleToString( cap.Ho ));
	ui->r->setText(doubleToString( cap.R ));

	QValidator *v = new QRegExpValidator(QRegExp(numeric_validator_rx), this);
	ui->alpha->setValidator(v);
	ui->ho->setValidator(v);
	ui->r->setValidator(v);
}
//=========================================================
void CapillaryDlg::accept()
{
	bool ok;
	double n;

	n = ui->alpha->text().toDouble( &ok );
	if( ok )
		c.Alpha = n;

	n = ui->ho->text().toDouble( &ok );
	if( ok )
		c.Ho = n;

	n = ui->r->text().toDouble( &ok );
	if( ok )
		c.R = n;

	QDialog::accept();
}
