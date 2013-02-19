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

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include "vesselconnectionview.h"

VesselConnectionView::VesselConnectionView(VesselView::Type type, int gen, int idx, double ysplit)
        : VesselView(0, 0, type, gen, idx), y_split(ysplit)
{
	setupPath();
}

VesselConnectionView::~VesselConnectionView()
{

}

QRectF VesselConnectionView::boundingRect() const
{
	return QRectF(QPointF(800,160*2-50-y_split), QPointF(1100,800*2+50+y_split));
}

void VesselConnectionView::paint(QPainter *painter,
                                 const QStyleOptionGraphicsItem *option,
                                 QWidget *widget)
{
	double lod = option->levelOfDetailFromTransform(painter->transform());
	if (lod < minLOD())
		return;

	QColor fill_color = baseColor(option);
	QColor dark_fill_color = fill_color.dark();

	QGradientStops stops;
	stops << QGradientStop(0.038, dark_fill_color)
	      << QGradientStop(0.128, fill_color)
	      << QGradientStop(0.174, fill_color)
	      << QGradientStop(0.174+0.125-0.03, dark_fill_color)
	      << QGradientStop(0.45, fill_color)
	      << QGradientStop(0.55, fill_color)

	      << QGradientStop(1-(0.174+0.125-0.03), dark_fill_color)
	      << QGradientStop(1-0.174, fill_color)
	      << QGradientStop(1-0.128, fill_color)
	      << QGradientStop(1-0.038, dark_fill_color);
	const QRectF &br = boundingRect();
	QLinearGradient gradient(QLineF(br.topLeft(), br.topRight()).pointAt(0.5),
	                         QLineF(br.bottomRight(), br.bottomLeft()).pointAt(0.5));
	gradient.setStops(stops);

	QLinearGradient gradient2(QLineF(br.topLeft(), br.bottomLeft()).pointAt(0.5),
	                          QLineF(br.topRight(), br.bottomRight()).pointAt(0.5));
	QColor transparent_color(fill_color), semi_transparent_color(fill_color);
	transparent_color.setAlpha(0);
	semi_transparent_color.setAlpha(200);

	gradient2.setStops(QGradientStops()
	                   << QGradientStop(0, transparent_color)
	                   << QGradientStop(0.45, semi_transparent_color)
	                   << QGradientStop(0.55, semi_transparent_color)
	                   << QGradientStop(1, transparent_color));

	painter->setPen(QPen(penColor(option), 32/2, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
	painter->drawPath(path);

	if (isClearBg()) {
		painter->fillPath(fill, Qt::white);
	}
	else {
		painter->fillPath(fill, gradient);
		painter->fillPath(fill, gradient2);
	}
}

void VesselConnectionView::setupPath()
{
	path.moveTo(QPointF(800,320*2-32/4.0)); // draw bottom arm outline
	path.cubicTo(QPointF(950, 320*2), QPointF(1000, 160*2-y_split), QPointF(1100,160*2-y_split));
	path.moveTo(QPointF(800,320*2));
	path.cubicTo(QPointF(950, 320*2), QPointF(1000, 160*2-y_split), QPointF(1100,160*2-y_split));

	path.moveTo(QPointF(800,640*2+32/4.0)); // draw top arm outline
	path.cubicTo(QPointF(950, 640*2), QPointF(1000, 800*2+y_split), QPointF(1100,800*2+y_split));

	path.moveTo(QPointF(1100,320*2-y_split)); // middle
	path.cubicTo(QPointF(1050, 320*2-y_split), QPointF(950, 480*2), QPointF(850,480*2));
	path.cubicTo(QPointF(950, 480*2), QPointF(1000, 640*2+y_split), QPointF(1100,640*2+y_split));


	fill.moveTo(QPointF(800,320*2));
	fill.cubicTo(QPointF(950, 320*2), QPointF(1000, 160*2-y_split), QPointF(1100,160*2-y_split));
	fill.lineTo(QPointF(1100,320*2-y_split));
	fill.cubicTo(QPointF(1050, 320*2-y_split), QPointF(950, 480*2), QPointF(850,480*2));
	fill.cubicTo(QPointF(950, 480*2), QPointF(1000, 640*2+y_split), QPointF(1100,640*2+y_split));
	fill.lineTo(QPointF(1100, 800*2+y_split));
	fill.cubicTo(QPointF(1000, 800*2+y_split), QPointF(950, 640*2), QPointF(800,640*2));
	fill.closeSubpath();

//	path_top.lineTo(points[5]);
	//path_top.lineTo(points[2]);
	//path_top.lineTo(points[4]);
}
