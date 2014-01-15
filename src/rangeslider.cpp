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

#include "rangeslider.h"
#include <QPainter>
#include <QPaintEvent>
#include <QStyle>
#include <QStyleOptionSlider>


const int slider_style = QStyle::CC_CustomBase + 1;

RangeSlider::RangeSlider(QWidget *parent)
        : QSlider(parent)
{

}

RangeSlider::RangeSlider(Qt::Orientation o, QWidget *parent)
        : QSlider(o, parent)
{

}

void RangeSlider::paintEvent(QPaintEvent *ev)
{
	QPainter p(this);
	QStyleOptionSlider opt;
	initStyleOption(&opt);

	opt.subControls = QStyle::SC_SliderGroove | QStyle::SC_SliderHandle;
	if (tickPosition() != NoTicks)
		opt.subControls |= QStyle::SC_SliderTickmarks;
//	if (ev->) {
//		opt.activeSubControls = d->pressedControl;
//		opt.state |= QStyle::State_Sunken;
//	} else {
//		opt.activeSubControls = d->hoverControl;
//	}

	style()->drawComplexControl((QStyle::ComplexControl)slider_style, &opt, &p, this);
}
