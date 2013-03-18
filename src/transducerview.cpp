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

#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QWidget>
#include "transducerview.h"



TransducerView::TransducerView(Model::Transducer pos)
        : QGraphicsItem(), trans_pos(pos)
{
	setZValue(1);

	switch (trans_pos) {
	case Model::Top:
		setTransform(QTransform().translate(10, 0));
		break;
	case Model::Bottom:
		setTransform(QTransform().translate(10, 100-7.5));
		break;
	case Model::Middle:
		setTransform(QTransform().translate(55, 45));
		break;
	}

	setToolTip("Right click to move transducer to a different position");
}

TransducerView::~TransducerView()
{

}

QRectF TransducerView::boundingRect() const
{
	return QRectF(QPointF(0, 0), QSizeF(25, 15));
}

void TransducerView::paint(QPainter *painter,
                           const QStyleOptionGraphicsItem *option,
                           QWidget *widget)
{
	Q_UNUSED(option);
	Q_UNUSED(widget);

	const QRectF r = boundingRect().adjusted(1.5, 1.5, -1.5, -1.5);
	painter->setPen(QPen(Qt::black, 1.5, Qt::SolidLine, Qt::FlatCap));
	painter->drawEllipse(r);

	painter->drawText(r, "T", QTextOption(Qt::AlignCenter));
}

bool TransducerView::contains(const QPointF &point) const
{
	if (transform() == current_transform)
		return boundingRect().contains(inverse_transform.map(point));

	current_transform = transform();
	inverse_transform = current_transform.inverted();
	return contains(point);
}
