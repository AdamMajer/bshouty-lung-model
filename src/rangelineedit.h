/*
 *   Bshouty Lung Model - Pulmonary Circulation Simulation
 *    Copyright (c) 1989-2014 Zoheir Bshouty, MD, PhD, FRCPC
 *    Copyright (c) 2011-2014 Adam Majer
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

#include <QLineEdit>

extern const char range_property[];

class RangeLineEdit : public QLineEdit
{
	Q_OBJECT

public:
	RangeLineEdit(QWidget * parent = 0);
	RangeLineEdit(const QString & contents, QWidget * parent = 0);

protected:
	virtual void contextMenuEvent(QContextMenuEvent *);
	virtual void focusInEvent(QFocusEvent *);
	virtual void focusOutEvent(QFocusEvent *);

protected slots:
	void setRange();
	void textChangedIsRange(const QString &txt);

signals:
	void focusChange(bool in_focus);
};
