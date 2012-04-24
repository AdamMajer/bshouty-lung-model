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

#include <QApplication>
#include <QMap>
#include "common.h"
#include "dbsettings.h"
#include "wizard.h"
#include "wizard/simpletextpage.h"
#include "wizard/buttontextpage.h"

Wizard::Wizard(QWidget *parent)
    : QWizard(parent)
{
	QMap<WizardPages, QWizardPage*> page_map;
	const QString root_path = QCoreApplication::applicationDirPath();

	first_page = new SimpleTextPage(root_path + "/data/wizard_intro.html",
	                                                this,
		                                        Wizard_GenerationPage);
	first_page->setShowOnStartButtonVisible(true);
	first_page->setShowOnStartChecked(DbSettings::value(show_wizard_on_start, true).toBool());

	page_map.insert(Wizard_IntroPage, first_page);
	page_map.insert(Wizard_GenerationPage,
	                new ButtonTextPage(root_path + "/data/generations.html",
	                                   "5-Generation Model", Wizard_ModelPage,
	                                   "15-Generation Model", Wizard_ModelPage,
	                                   this));
	page_map.insert(Wizard_ModelPage,
	                new ButtonTextPage(root_path + "/data/full_ph.html",
	                                   "Standard Model", Wizard_FullModelPage,
	                                   "PH Model", Wizard_PHPage,
	                                   this));
	page_map.insert(Wizard_FullModelPage,
	                new SimpleTextPage(root_path + "/data/fullmodel.html", this));
	page_map.insert(Wizard_PHPage,
	                new ButtonTextPage(root_path + "/data/ph.html",
	                                   "PAH", Wizard_PHPahPage,
	                                   "PVOD", Wizard_PHPvodPage,
	                                   this));
	page_map.insert(Wizard_PHPahPage,
	                new ButtonTextPage(root_path + "/data/papm_compromise.html",
	                                   "PAP", Wizard_PahPAPPage,
	                                   "Compromise", Wizard_PahAreaPage,
	                                   this));
	page_map.insert(Wizard_PahPAPPage,
	                new SimpleTextPage(root_path + "/data/pap_calculation.html"));
	page_map.insert(Wizard_PahAreaPage,
	                new SimpleTextPage(root_path + "/data/area_calculation.html"));

	page_map.insert(Wizard_PHPvodPage,
	                new ButtonTextPage(root_path + "/data/papm_compromise.html",
	                                   "PAP", Wizard_PvodPAPPage,
	                                   "Compromise", Wizard_PvodAreaPage,
	                                   this));
	page_map.insert(Wizard_PvodPAPPage,
	                new SimpleTextPage(root_path + "/data/pap_calculation.html"));
	page_map.insert(Wizard_PvodAreaPage,
	                new SimpleTextPage(root_path + "/data/area_calculation.html"));


	QMap<WizardPages, QWizardPage*>::const_iterator i;
	for (i=page_map.begin(); i!=page_map.end(); ++i)
		setPage(i.key(), i.value());


	setWindowTitle("New Model Wizard");

	QRect new_size = DbSettings::value("/settings/wizard_size", QRect()).toRect();
	if (new_size.isValid())
		setGeometry(new_size);
}

Wizard::~Wizard()
{
	DbSettings::setValue("/settings/wizard_size", geometry());
	DbSettings::setValue(show_wizard_on_start, first_page->isShowOnStartChecked());
}

int Wizard::nGenerations() const
{
	ButtonTextPage *p = qobject_cast<ButtonTextPage*>(page(Wizard_GenerationPage));
	if (p && p->buttonChecked() == 2)
		return 15;

	// default
	return 5;
}
