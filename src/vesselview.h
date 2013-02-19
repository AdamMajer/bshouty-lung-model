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

#ifndef VESSELVIEW_H
#define VESSELVIEW_H

#include <QGraphicsItem>

struct Capillary;
struct Vessel;

class VesselView : public QGraphicsItem
{
public:
	enum Type { Artery = UserType+1,
		    Vein = UserType+2,
		    Capillary = UserType+3,
		    Connection = UserType+4,

		    CornerVessel
	          };

	VesselView(const void *baseline_data, const void *vessel_data,
	           Type type, int gen, int index,
	           const Vessel *b_cv=0, const Vessel *cv=0);
	~VesselView();

	double minLOD() const { return 0.0025; }
	int generation() const { return gen; }
	int vesselIndex() const { return idx; }
	virtual int type() const { return v_type; }
	QColor baseColor(const QStyleOptionGraphicsItem *option) const;
	QColor penColor(const QStyleOptionGraphicsItem *option) const;

	void setClear(bool is_clear_bg);
	bool isClearBg() const { return is_clear_bg; }

	virtual QRectF boundingRect() const;
	virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
	                   QWidget *widget);
	virtual bool contains(const QPointF &point) const;

	static QString vesselToStringTitle(Type type, int gen, int idx);

protected:
	QString headers(double lod) const;
	QString headers(Type type, bool) const;
	QString baselineValuesText(double lod) const;
	QString baselineValuesText(Type type, bool) const;
	QString calculatedValuesText(double lod) const;
	QString calculatedValuesText(Type type, bool) const;
	QString valuesText(const void *s) const;
	void setupPath();

private:
	Type v_type;
	int gen, idx;

	QLineF top, bottom;
	QRectF fill;

	bool is_clear_bg;

	const ::Capillary *cap, *baseline_cap;
	const Vessel *vessel, *baseline_vessel;
	const Vessel *cv, *baseline_cv;
	QString vessel_title;
};


#endif // ifdef VESSELVIEW_H
