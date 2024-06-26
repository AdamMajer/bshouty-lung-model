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

#include "common.h"
#include "model/model.h"
#include "vesselview.h"
#include <QDebug>
#include <QLinearGradient>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include <exception>


VesselView::VesselView(const void *baseline_data, const void *vessel_or_cap,
                       VesselView::Type t, int g, int i,
                       const Vessel *baseline_corner, const Vessel *corner)
        : v_type(t), gen(g), idx(i),
          cv(corner), baseline_cv(baseline_corner)
{
	setAcceptHoverEvents(true);
	is_clear_bg = false;

	cap = 0;
	vessel = 0;

	switch (v_type) {
	case Capillary:
		cap = static_cast<const ::Capillary*>(vessel_or_cap);
		baseline_cap = static_cast<const ::Capillary*>(baseline_data);

		// FIXME: use seperate constructor for capillaries and veins
		if (baseline_cv == NULL || cv == NULL)
			throw "NULL corner vessels for capillaries";
		break;
	case Vein:
		vessel = static_cast<const ::Vessel*>(vessel_or_cap);
		baseline_vessel = static_cast<const ::Vessel*>(baseline_data);
		break;
	case Artery:
		vessel = static_cast<const ::Vessel*>(vessel_or_cap);
		baseline_vessel = static_cast<const ::Vessel*>(baseline_data);
		break;
	case Connection:
	case CornerVessel:
		break;
	}

	vessel_title = vesselToStringTitle(v_type, gen, idx);
	setupPath();
}

QString VesselView::vesselToStringTitle(Type type, int gen, int idx)
{
	QString vessel_side;
	int gen_count = 1;
	if (gen>1) {
		vessel_side = (idx < 1<<(gen-2)) ? QLatin1String("Lt.") : QLatin1String("Rt.");
		gen_count = 1<<(gen-2);
	}
	QString t = QLatin1String("%3  %4 (%1 of %2)");
	int idx_from_bottom = gen_count - idx%gen_count;

	switch (type) {
	case Capillary:
		return t.arg(idx_from_bottom).arg(gen_count).arg("Capillary").arg(vessel_side);
	case Vein:
		return t.arg(idx_from_bottom).arg(gen_count).arg("Vein").arg(vessel_side);
	case Artery:
		return t.arg(idx_from_bottom).arg(gen_count).arg("Artery").arg(vessel_side);
	case Connection:
	case CornerVessel:
		break;
	}

	return QString();
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
	QPen p(penColor(option), 32, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin);
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

	QRectF draw_area = painter->transform().mapRect(fill.adjusted(30, 30, -30, -30));
	painter->resetTransform();
	lod /= 2;
	painter->scale(lod/2, lod/2);
	draw_area = painter->transform().inverted().mapRect(draw_area);

	if (lod > 0.05) {
		double offset = draw_area.width()/3;
		draw_area.adjust(0, 0, -offset*2, 0);
		QFont font = painter->font();
		QTextOption to(Qt::AlignLeft);
		QString header_str = headers(lod);

		to.setWrapMode(QTextOption::NoWrap);

		double line_height = painter->fontMetrics().lineSpacing();
		if (type() == Capillary)
			qDebug() << "# headers" << header_str.count(QLatin1Char('\n'));
		double scale_factor = draw_area.height() / ((header_str.count(QLatin1Char('\n')) + 4) * line_height);

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

		painter->drawText(draw_area.adjusted(offset, line_height*3, offset, 0),
		                  baselineValuesText(lod),
		                  to);
		painter->drawText(draw_area.adjusted(offset*2, line_height*3, offset*2, 0),
		                  calculatedValuesText(lod),
		                  to);

		font.setBold(true);
		painter->setFont(font);
		painter->drawText(draw_area.adjusted(0,line_height*2,0,0), header_str, to);
		painter->drawText(draw_area.adjusted(offset,line_height*2,offset,0), "Baseline", to);
		painter->drawText(draw_area.adjusted(offset*2,line_height*2,offset*2,0), "Calculated", to);
	}
}

bool VesselView::contains(const QPointF &point) const
{
	return boundingRect().contains(point);
}

QString VesselView::headers(double lod) const
{
	Q_UNUSED(lod);

	return headers((Type)type(), false);
}

QString VesselView::headers(Type type, bool) const
{
	switch (type) {
	case Artery:
	case Vein:
		return QString::fromUtf8("\nR\nγ\nφ\nC\nPeri. a\nPeri. b\nPeri. c\nP0\n\n"
		                         "GP\nPTP\nTone\nFlow (μL/s)\nPin\nPout\n\n"
		                         "Length (μm)\nD0 (μm)\nD (μm)\nDmin (μm)\nDmax (μm)\nVisc Factor\nVolume (μL)\nTotal R.");
	case Capillary:
		return QLatin1String("\nR\nAlpha\nHo\nFlow\nKrc\nHin\nHout\n- - -") + headers(Artery, false);
	case CornerVessel:
	case Connection:
		break;
	}

	return QString();
}

QString VesselView::baselineValuesText(double lod) const
{
	Q_UNUSED(lod);

	return baselineValuesText((Type)type(), false);
}

QString VesselView::baselineValuesText(Type type, bool) const
{
	QStringList value_list;
	const Vessel *v = baseline_vessel;

	if (type == CornerVessel) {
		v = baseline_cv;
	}

	switch (type) {
	case Artery:
	case Vein:
	case CornerVessel:
		value_list << doubleToString(v->R);
		value_list << doubleToString(v->gamma);
		value_list << doubleToString(v->phi);
		value_list << doubleToString(v->c);
		value_list << QString();
		value_list << QString();
		value_list << QString();
		value_list << QString();
		value_list << QString();
		value_list << doubleToString(v->GP);
		value_list << doubleToString(v->Ptp);
		value_list << doubleToString(v->tone);
		value_list << QString();
		value_list << QString();
		value_list << QString();
		value_list << QString();
		value_list << doubleToString(v->length);
		value_list << doubleToString(v->D);
		value_list << QString();
		value_list << QString();
		value_list << QString();
		value_list << QString();
		value_list << doubleToString(v->volume);
		value_list << doubleToString(v->total_R);
		break;
	case Capillary:
		value_list << doubleToString(baseline_cap->R);
		value_list << doubleToString(baseline_cap->Alpha);
		value_list << doubleToString(baseline_cap->Ho);
		value_list << QString();
		value_list << doubleToString(baseline_cap->Krc);
		value_list << QString();
		value_list << QString();
		value_list << "- - - ";
		value_list << baselineValuesText(CornerVessel, false);
		break;

	case Connection:
		break;
	}

	return value_list.join(QLatin1String("\n"));
}

QString VesselView::calculatedValuesText(double lod) const
{
	Q_UNUSED(lod);

	return calculatedValuesText((Type)type(), false);
}

QString VesselView::calculatedValuesText(Type type, bool) const
{
	QStringList value_list;
	const Vessel *v = vessel;

	if (type == CornerVessel) {
		v = cv;
	}
	switch (type) {
	case Artery:
	case Vein:
	case CornerVessel:
		value_list << doubleToString(v->R);
		value_list << doubleToString(v->gamma);
		value_list << doubleToString(v->phi);
		value_list << doubleToString(v->c);
		value_list << doubleToString(v->perivascular_press_a);
		value_list << doubleToString(v->perivascular_press_b);
		value_list << doubleToString(v->perivascular_press_c);
		value_list << doubleToString(v->pressure_0);
		value_list << QString();
		value_list << doubleToString(v->GP);
		value_list << doubleToString(v->Ptp);
		value_list << doubleToString(v->tone);
		value_list << doubleToString(v->flow * 1e6/60); // L/min => uL/s
		value_list << doubleToString(v->pressure_in);
		value_list << doubleToString(v->pressure_out);
		value_list << QString();
		value_list << doubleToString(v->length);
		value_list << doubleToString(v->D);
		value_list << doubleToString(v->D_calc);
		value_list << doubleToString(v->Dmin);
		value_list << doubleToString(v->Dmax);
		value_list << doubleToString(v->viscosity_factor);
		value_list << doubleToString(v->volume);
		value_list << doubleToString(v->total_R);
		break;
	case Capillary:
		value_list << doubleToString(cap->R);
		value_list << doubleToString(cap->Alpha);
		value_list << doubleToString(cap->Ho);
		value_list << doubleToString(cap->flow * 1e6/60); // L/min => uL/s
		value_list << doubleToString(cap->Krc);
		value_list << doubleToString(cap->Hin);
		value_list << doubleToString(cap->Hout);
		value_list << " - - -";
		value_list << calculatedValuesText(CornerVessel, false);
		break;

	case Connection:
		break;
	}

	return value_list.join(QLatin1String("\n"));
}

void VesselView::setupPath()
{
	top = QLineF(QPointF(200,320*2), QPointF(800,320*2));
	bottom = QLineF(QPointF(200,640*2), QPointF(800,640*2));

	fill = QRectF(QPointF(200, 320*2), QPointF(800, 640*2));
	if (generation() == 1)
		fill.adjust(-16, 0, 10, 0);
	else
		fill.adjust(-10, 0, 10, 0);
}
