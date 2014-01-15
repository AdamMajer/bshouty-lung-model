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

#include <QGraphicsItem>
#include "model/model.h"


class TransducerView : public QGraphicsItem
{
public:
	TransducerView(Model::Transducer pos);
	~TransducerView();

	virtual QRectF boundingRect() const;
	virtual void paint(QPainter *painter,
	                   const QStyleOptionGraphicsItem *option,
	                   QWidget *widget);

	virtual bool contains(const QPointF &point) const;

private:
	Model::Transducer trans_pos;
	mutable QTransform current_transform, inverse_transform;
};
