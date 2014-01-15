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

#include <QStyleOptionGraphicsItem>
#include <QPainter>
#include "capillaryview.h"
#include "model/model.h"
#include <math.h>


CapillaryView::CapillaryView(const ::Capillary *baseline, const ::Capillary *cap,
                             const ::Vessel *cv_baseline, const ::Vessel *cv, int idx)
        : VesselView(baseline, cap, VesselView::Capillary, -1, idx, cv_baseline, cv)
{
	setupPath();
}

CapillaryView::~CapillaryView()
{

}

QRectF CapillaryView::boundingRect() const
{
	return QRectF(QPointF(800,250), QSizeF(2400,1500));
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
	if (lod > 0.05) {
		QRectF textRect = boundingRect().adjusted(200, 100, -200, -100);
		double offset = textRect.width()/3;
		double line_height = painter->fontMetrics().lineSpacing();

		QString h = headers(lod*2);
		QFont font = painter->font();
		qDebug("line count: %d vs. %f", h.count(QLatin1Char('\n')), line_height);
		double scale_factor = textRect.height() / ((h.count(QLatin1Char('\n')) + 3) * line_height);
		qDebug("factor: %f - size %f", scale_factor, font.pointSizeF());

		if (font.pointSizeF() < 0)
			font.setPixelSize(font.pixelSize() * scale_factor);
		else
			font.setPointSizeF(font.pointSizeF() * scale_factor);
		line_height *= scale_factor;
		painter->setFont(font);

		qDebug("factor: %f - size %f", scale_factor, font.pointSizeF());

		painter->drawRect(textRect);
		painter->fillRect(textRect, QColor(255, 255, 255, 220));
		textRect.adjust(10, 0, -offset*2, 0);
		painter->drawText(textRect.adjusted(offset, line_height*1.5, offset, 0),
		                  baselineValuesText(lod*2));
		painter->drawText(textRect.adjusted(offset*2, line_height*1.5, offset*2, 0),
		                  calculatedValuesText(lod*2));

		font.setBold(true);
		painter->setFont(font);
		painter->drawText(textRect.adjusted(10, line_height*0.5, 0, 0),
		                  headers(lod*2));
		painter->drawText(textRect.adjusted(offset, 0, offset, 0),
		                  "Baseline");
		painter->drawText(textRect.adjusted(offset*2, 0, offset*2, 0),
		                  "Calculated");
	}
}

bool CapillaryView::contains(const QPointF &point) const
{
	return boundingRect().contains(point);
}

void CapillaryView::setupPath()
{
	path.moveTo(800, 1000);
	QRectF r(800, 250, 300, 1500);

	QRectF vessel_view(40, 0, 300, 300);
	for (int x=800; x<800+2400; x+=500)
		for (int y=250 + (((x-800)%1000>0)?300:0); y<250+1500; y+=600)
			path.addEllipse(vessel_view.adjusted(x, y, x, y));
}
