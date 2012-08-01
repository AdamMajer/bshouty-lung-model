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

#include "common.h"
#include "model/model.h"
#include "vesselview.h"
#include <QDebug>
#include <QLinearGradient>
#include <QPainter>
#include <QStyleOptionGraphicsItem>


VesselView::VesselView(const void *baseline_data, const void *vessel_or_cap,
                       VesselView::Type t, int g, int i)
        : v_type(t), gen(g), idx(i)
{
	setAcceptHoverEvents(true);
	is_clear_bg = false;

	cap = 0;
	vessel = 0;

	QString vessel_side;
	int gen_count = 1;
	if (gen>1) {
		vessel_side = (idx < 1<<(gen-2)) ? QLatin1String("Lt.") : QLatin1String("Rt.");
		gen_count = 1<<(gen-2);
	}

	vessel_title = QLatin1String("%3  %4 (%1 of %2)");
	int idx_from_bottom = gen_count - idx%gen_count;

	switch (v_type) {
	case Capillary:
		cap = static_cast<const ::Capillary*>(vessel_or_cap);
		baseline_cap = static_cast<const ::Capillary*>(baseline_data);
		vessel_title = vessel_title.arg(idx_from_bottom).arg(gen_count).arg("Capillary").arg(vessel_side);
		break;
	case Vein:
		vessel_title = vessel_title.arg(idx_from_bottom).arg(gen_count).arg("Vein").arg(vessel_side);
		vessel = static_cast<const ::Vessel*>(vessel_or_cap);
		baseline_vessel = static_cast<const ::Vessel*>(baseline_data);
		break;
	case Artery:
		vessel_title = vessel_title.arg(idx_from_bottom).arg(gen_count).arg("Artery").arg(vessel_side);
		vessel = static_cast<const ::Vessel*>(vessel_or_cap);
		baseline_vessel = static_cast<const ::Vessel*>(baseline_data);
		break;
	case Connection:
		break;
	}

	setupPath();
}

VesselView::~VesselView()
{

}

QColor VesselView::baseColor(const QStyleOptionGraphicsItem *option) const
{
	if (is_clear_bg)
		return Qt::white;

	if (option->state & QStyle::State_MouseOver)
		return Qt::gray;

	switch (type()) {
	case VesselView::Artery:
		return QColor(120, 120, 255);
	case VesselView::Vein:
		return QColor(255, 50, 50);
	}

	return Qt::black;
}

QColor VesselView::penColor(const QStyleOptionGraphicsItem *) const
{

	return Qt::darkBlue;
}

void VesselView::setClear(bool is_clear_bg_new)
{
	if (is_clear_bg != is_clear_bg_new) {
		is_clear_bg = is_clear_bg_new;
		update();
	}
}

QRectF VesselView::boundingRect() const
{
	return fill.adjusted(0, -3, 0, 3);
}

void VesselView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                       QWidget *)
{
	double lod = option->levelOfDetailFromTransform(painter->transform());
	if (lod < minLOD())
		return;

	/* Drawing area is 100x96 */
	QPen p(penColor(option), 3.2, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin);
	if (generation() ==1)
		// round cap is only on the left side of 1st
		p.setCapStyle(Qt::RoundCap);

	painter->setPen(p);
	painter->drawLine(top);
	painter->drawLine(bottom);

	if (is_clear_bg) {
		painter->fillRect(fill, Qt::white);
	}
	else {
		QGradientStops stops(4);
		stops << QGradientStop(0, baseColor(option).dark())
		      << QGradientStop(0.4, baseColor(option))
		      << QGradientStop(0.6, baseColor(option))
		      << QGradientStop(1, baseColor(option).dark());
		QLinearGradient gradient(QLineF(fill.topLeft(), fill.topRight()).pointAt(0.5),
		                         QLineF(fill.bottomRight(), fill.bottomLeft()).pointAt(0.5));
		gradient.setStops(stops);
		painter->fillRect(fill, gradient);
	}

	QRectF draw_area = painter->transform().mapRect(fill.adjusted(3, 3, -3, -3));
	painter->resetTransform();
	lod /= 2;
	painter->scale(lod/2, lod/2);
	draw_area = painter->transform().inverted().mapRect(draw_area);

	if (lod > 0.5) {
		double offset = draw_area.width()/3;
		draw_area.adjust(0, 0, -offset*2, 0);
		QFont font = painter->font();
		QTextOption to(Qt::AlignLeft);

		to.setWrapMode(QTextOption::NoWrap);

		// scale the font to allow a minimum of 22 (3 for title) lines of text to be displayed
		double line_height = painter->fontMetrics().lineSpacing();
		double scale_factor = draw_area.height() / (25.0 * line_height);

		if (font.pointSizeF() < 0)
			font.setPixelSize(font.pixelSize() * scale_factor);
		else
			font.setPointSizeF(font.pointSizeF() * scale_factor);

		font.setUnderline(true);
		painter->setFont(font);
		painter->drawText(draw_area.adjusted(0, 0, offset*2, 0),
		                  Qt::AlignHCenter | Qt::AlignTop,
		                  vessel_title);

		font.setUnderline(false);
		painter->setFont(font);
		line_height = painter->fontMetrics().lineSpacing();

		painter->drawText(draw_area.adjusted(offset, line_height*4, offset, 0),
		                  baselineValuesText(lod),
		                  to);
		painter->drawText(draw_area.adjusted(offset*2, line_height*4, offset*2, 0),
		                  calculatedValuesText(lod),
		                  to);

		font.setBold(true);
		painter->setFont(font);
		painter->drawText(draw_area.adjusted(0,line_height*3,0,0), headers(lod), to);
		painter->drawText(draw_area.adjusted(offset,line_height*3,offset,0), "Baseline", to);
		painter->drawText(draw_area.adjusted(offset*2,line_height*3,offset*2,0), "Calculated", to);
	}
}

bool VesselView::contains(const QPointF &point) const
{
	return boundingRect().contains(point);
}

QString VesselView::headers(double lod) const
{
	Q_UNUSED(lod);

	switch (type()) {
	case Artery:
		return QString::fromUtf8("\nR\nA\nB\nPeri. a\nPeri. b\nPeri. c\n\nGP\nPTP\nTone\nFlow (μL/s)\nPin\n\nLength (μm)\nD (μm)\nDmin (μm)\nDmax (μm)\nVisc Factor\nVolume (μL)");
	case Vein:
		return QString::fromUtf8("\nR\nB\nC\nPeri. a\nPeri. b\nPeri. c\n\nGP\nPTP\nTone\nFlow (μL/s)\nPin\n\nLength (μm)\nD (μm)\nDmin (μm)\nDmax (μm)\nVisc Factor\nVolume (μL)");
	case Capillary:
		return QLatin1String("\nR\nAlpha\nHo");
	}

	return QString();
}

QString VesselView::baselineValuesText(double lod) const
{
	Q_UNUSED(lod);

	QStringList value_list;

	switch (type()) {
	case Artery:
		value_list << doubleToString(baseline_vessel->R);
		value_list << doubleToString(baseline_vessel->a);
	case Vein:
		if (type() == Vein)
			value_list << doubleToString(baseline_vessel->R);
		value_list << doubleToString(baseline_vessel->b);
		if (type() == Vein)
			value_list << doubleToString(baseline_vessel->c);
		value_list << QString::null;
		value_list << QString::null;
		value_list << QString::null;
		value_list << QString::null;
		value_list << doubleToString(baseline_vessel->GP);
		value_list << doubleToString(baseline_vessel->Ptp);
		value_list << doubleToString(baseline_vessel->tone);
		value_list << QString::null;
		value_list << QString::null;
		value_list << QString::null;
		value_list << doubleToString(baseline_vessel->length);
		value_list << doubleToString(baseline_vessel->D);
		value_list << QString::null;
		value_list << QString::null;
		value_list << QString::null;
		value_list << doubleToString(baseline_vessel->volume);
		break;
	case Capillary:
		value_list << doubleToString(baseline_cap->R);
		value_list << doubleToString(baseline_cap->Alpha);
		value_list << doubleToString(baseline_cap->Ho);
		break;
	}

	return value_list.join(QLatin1String("\n"));
}

QString VesselView::calculatedValuesText(double lod) const
{
	Q_UNUSED(lod);

	QStringList value_list;

	switch (type()) {
	case Artery:
		value_list << doubleToString(vessel->R);
		value_list << doubleToString(vessel->a);
	case Vein:
		if (type() == Vein)
			value_list << doubleToString(vessel->R);
		value_list << doubleToString(vessel->b);
		if (type() == Vein)
			value_list << doubleToString(vessel->c);
		value_list << doubleToString(vessel->perivascular_press_a);
		value_list << doubleToString(vessel->perivascular_press_b);
		value_list << doubleToString(vessel->perivascular_press_c);
		value_list << QString::null;
		value_list << doubleToString(vessel->GP);
		value_list << doubleToString(vessel->Ptp);
		value_list << doubleToString(vessel->tone);
		value_list << doubleToString(vessel->flow * 1e6/60); // L/min => uL/s
		value_list << doubleToString(vessel->pressure);
		value_list << QString::null;
		value_list << doubleToString(vessel->length);
		value_list << doubleToString(vessel->D_calc);
		value_list << doubleToString(vessel->Dmin);
		value_list << doubleToString(vessel->Dmax);
		value_list << doubleToString(vessel->viscosity_factor);
		value_list << doubleToString(vessel->volume);
		break;
	case Capillary:
		value_list << doubleToString(cap->R);
		value_list << doubleToString(cap->Alpha);
		value_list << doubleToString(cap->Ho);
		break;
	}

	return value_list.join(QLatin1String("\n"));
}

void VesselView::setupPath()
{
	top = QLineF(QPointF(20,32*2), QPointF(80,32*2));
	bottom = QLineF(QPointF(20,64*2), QPointF(80,64*2));

	fill = QRectF(QPointF(20, 32*2), QPointF(80, 64*2));
	if (generation() == 1)
		fill.adjust(-1.6, 0, 1, 0);
	else
		fill.adjust(-1, 0, 1, 0);
}
