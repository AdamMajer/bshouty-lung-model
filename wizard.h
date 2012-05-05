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

#include <QWizard>

class ButtonTextPage;
class Wizard : public QWizard
{
public:
	enum WizardPages {
		Wizard_IntroPage,
		Wizard_ModelTypePage,  // single/double lung models
		Wizard_GenerationPage, // select number of generation in model
		Wizard_ModelPage,
		Wizard_FullModelPage,
		Wizard_PHPage,
//		Wizard_SingleRunPage,
//		Wizard_MultipleRunsPage,
		Wizard_PHPahPage,
		Wizard_PHPvodPage,
		Wizard_PahPAPPage,
		Wizard_PahAreaPage,
		Wizard_PvodPAPPage,
		Wizard_PvodAreaPage
	};

	Wizard(QWidget *parent=0);
	virtual ~Wizard();

	int nGenerations() const;
	bool singleLungModel() const; // true => single lung, false => double lung

	WizardPages exitPage() const { return (WizardPages)currentId(); }

private:
	ButtonTextPage *first_page;
};
