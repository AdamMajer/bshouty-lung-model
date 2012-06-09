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

#include <QStyleOptionGraphicsItem>
#include <QPainter>
#include "capillaryview.h"
#include "model/model.h"
#include <math.h>


CapillaryView::CapillaryView(const ::Capillary *baseline, const ::Capillary *cap, int idx)
        : VesselView(baseline, cap, VesselView::Capillary, -1, idx)
{
	setupPath();
}

CapillaryView::~CapillaryView()
{

}

QRectF CapillaryView::boundingRect() const
{
	return QRectF(QPointF(80,25), QSizeF(240,150));
}

void CapillaryView::paint(QPainter *painter,
                          const QStyleOptionGraphicsItem *option,
                          QWidget *widget)
{
	Q_UNUSED(widget);
	double lod = option->levelOfDetailFromTransform(painter->transform());
	if (lod < minLOD())
		return;

	QLinearGradient linear(QPointF(80,25), QPointF(80+240, 25));
	linear.setColorAt(0, QColor(120, 120, 255));
	linear.setColorAt(1, QColor(255, 50, 50));

	painter->fillRect(boundingRect(), linear);
	painter->drawRect(boundingRect());
	painter->setPen(QPen(penColor(option), 3.2));
	painter->drawPath(path);
	painter->fillPath(path, Qt::white);

	// Draw rectangle to display text in the middle
	if (lod > 0.5) {
		QRectF textRect = boundingRect().adjusted(20, 10, -20, -10);
		double offset = textRect.width()/3;
		double line_height = painter->fontMetrics().lineSpacing();

		painter->drawRect(textRect);
		painter->fillRect(textRect, QColor(255, 255, 255, 220));
		textRect.adjust(0, 0, -offset*2, 0);
		painter->drawText(textRect.adjusted(offset, line_height, offset, 0), baselineValuesText(lod*2));
		painter->drawText(textRect.adjusted(offset*2, line_height, offset*2, 0), calculatedValuesText(lod*2));

		QFont font = painter->font();
		font.setBold(true);
		painter->setFont(font);
		painter->drawText(textRect, headers(lod*2));
		painter->drawText(textRect.adjusted(offset, 0, offset, 0), "Baseline");
		painter->drawText(textRect.adjusted(offset*2, 0, offset*2, 0), "Calculated");
	}
}

bool CapillaryView::contains(const QPointF &point) const
{
	return boundingRect().contains(point);
}

void CapillaryView::setupPath()
{
	path.moveTo(80, 100);
	QRectF r(80, 25, 30, 150);

	QRectF vessel_view(4, 0, 30, 30);
	for (int x=80; x<80+240; x+=50)
		for (int y=25 + (((x-80)%100>0)?30:0); y<25+150; y+=60)
			path.addEllipse(vessel_view.adjusted(x, y, x, y));
}