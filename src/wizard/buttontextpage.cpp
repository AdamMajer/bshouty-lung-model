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

#include <QFile>
#include "buttontextpage.h"
#include "ui_buttontextpage.h"


ButtonTextPage::ButtonTextPage(const QString &f,
                               const QString &button1_text, int Button1_nextPage,
                               const QString &button2_text, int Button2_nextPage,
                               QWidget *parent)
    : QWizardPage(parent), filename(f)
{
	next_id[0] = Button1_nextPage;
	next_id[1] = Button2_nextPage;

	init();

	setRadioButtonText(1, button1_text);
	setRadioButtonText(2, button2_text);
}

ButtonTextPage::ButtonTextPage(QWidget *parent)
    : QWizardPage(parent)
{
	next_id[0] = -1;
	next_id[1] = -1;

	init();
}

ButtonTextPage::~ButtonTextPage()
{
	delete ui;
}

void ButtonTextPage::setRadioButtonText(int button_no, const QString &text)
{
	switch (button_no) {
	case 1:
		ui->button1->setText(text);
		break;
	case 2:
		ui->button2->setText(text);
		break;
	}
}

void ButtonTextPage::setText(const QString &text)
{
	ui->text->setText(text);
}

void ButtonTextPage::hideButtons(bool state)
{
	ui->groupBox->setHidden(state);
}

int ButtonTextPage::buttonChecked() const
{
	if (ui->button1->isChecked())
		return 1;
	if (ui->button2->isChecked())
		return 2;

	return 0;
}

void ButtonTextPage::initializePage()
{
	if (!filename.isEmpty()) {
		QFile file(filename);
		if (file.open(QIODevice::ReadOnly))
			ui->text->setText(QString::fromUtf8(file.readAll()));
	}
}

bool ButtonTextPage::isComplete() const
{
	return buttonChecked() > 0;
}

int ButtonTextPage::nextId() const
{
	if (buttonChecked() == 1)
		return next_id[0];

	return next_id[1];
}

void ButtonTextPage::setShowOnStartButtonVisible(bool isVisible)
{
	ui->show_on_start_checkbox->setVisible(isVisible);
}

void ButtonTextPage::setShowOnStartChecked(bool isChecked)
{
	ui->show_on_start_checkbox->setChecked(isChecked);
}

bool ButtonTextPage::isShowOnStartChecked() const
{
	return ui->show_on_start_checkbox->isChecked();
}

void ButtonTextPage::selectionChanged()
{
	emit completeChanged();
}

void ButtonTextPage::init()
{
	ui = new Ui::ButtonTextPage;
	ui->setupUi(this);

	connect(ui->button1, SIGNAL(clicked()), SLOT(selectionChanged()));
	connect(ui->button2, SIGNAL(clicked()), SLOT(selectionChanged()));

	ui->button1->setChecked(true);
	ui->show_on_start_checkbox->setHidden(true);
}
