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

#include "rangelineedit.h"
#include <QContextMenuEvent>
#include <QMenu>
#include "rangestepdlg.h"

const char range_property[] = "range";

RangeLineEdit::RangeLineEdit(QWidget *parent)
        : QLineEdit(parent)
{
	connect(this, SIGNAL(textChanged(QString)), SLOT(textChangedIsRange(QString)));
}

RangeLineEdit::RangeLineEdit(const QString &contents, QWidget *parent)
        : QLineEdit(contents, parent)
{
	connect(this, SIGNAL(textChanged(QString)), SLOT(textChangedIsRange(QString)));
}

void RangeLineEdit::contextMenuEvent(QContextMenuEvent *e)
{
	QMenu *menu = createStandardContextMenu();
	QList<QAction*> ac = menu->actions();
	// ac = actions() + ac;
	QAction *range_ac = menu->addAction("&Set Range", this, SLOT(setRange()));
	menu->addActions(ac);
	menu->setDefaultAction(range_ac);
	range_ac->setDisabled(property(range_property).isNull());
	menu->popup(mapToGlobal(e->pos()));
}

void RangeLineEdit::focusInEvent(QFocusEvent *e)
{
	QLineEdit::focusInEvent(e);
	emit focusChange(true);
}

void RangeLineEdit::focusOutEvent(QFocusEvent *e)
{
	emit focusChange(false);
	QLineEdit::focusOutEvent(e);
}

void RangeLineEdit::setRange()
{
	Range range(property(range_property).toString());
	RangeStepDlg dlg(range, this);
	dlg.setRange(Range(text()));

	if (dlg.exec() == QDialog::Accepted) {
		range = dlg.range();
		setText(range.toString());
	}
}

void RangeLineEdit::textChangedIsRange(const QString &txt)
{
	Range range(txt);
	setReadOnly(range.sequenceCount() > 1);
}
