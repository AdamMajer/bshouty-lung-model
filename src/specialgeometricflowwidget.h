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

#ifndef SpecialGeometricFlowWidget_H
#define SpecialGeometricFlowWidget_H

#include <QPixmap>
#include <QWidget>
#include "model/model.h"
#include "overlaysettingsdlg.h"

class QLabel;
class SpecialGeometricFlowWidget : public QWidget
{
	Q_OBJECT

public:
	SpecialGeometricFlowWidget(const Model &model, QWidget *parent=0);
	virtual ~SpecialGeometricFlowWidget();

public slots:
	void setGrid(bool is_visible);

protected:
	virtual void paintEvent(QPaintEvent *ev);

	virtual void leaveEvent(QEvent *);
	virtual void mouseMoveEvent(QMouseEvent *ev);

private:
	bool grid_visible;
	QPixmap map;
	QLabel *info_widget;
	QImage original_map;

	double mean_flow;
};

#endif // SpecialGeometricFlowWidget_H
