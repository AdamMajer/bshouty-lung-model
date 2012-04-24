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

#ifndef ButtonTextPage_H
#define ButtonTextPage_H

#include <QWizardPage>

namespace Ui {
class ButtonTextPage;
}

class ButtonTextPage : public QWizardPage
{
	Q_OBJECT

public:
	ButtonTextPage(const QString &filename, const QString &button1_text, int Button1_nextPage,
	               const QString &button2_text, int Button2_nextPage, QWidget *parent=0);
	ButtonTextPage(QWidget *parent=0);
	~ButtonTextPage();

	void setRadioButtonText(int button_no, const QString &text);
	void setText(const QString &text);
	void hideButtons(bool);

	int buttonChecked() const;

	virtual void initializePage();
	virtual bool isComplete() const;
	virtual int nextId() const;

	void setShowOnStartButtonVisible(bool isVisible);
	void setShowOnStartChecked(bool isChecked);
	bool isShowOnStartChecked() const;

public slots:
	void selectionChanged();

private:
	void init();

	Ui::ButtonTextPage *ui;
	QString filename;

	int next_id[2];
};


#endif
