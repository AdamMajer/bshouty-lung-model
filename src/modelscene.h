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

#include <QAction>
#include <QGraphicsScene>
#include "capillaryview.h"
#include "model/model.h"
#include "vesselconnectionview.h"

struct VesselItem
{
	VesselConnectionView *con;
	VesselView *v;

	Vessel art_vein_values;
	Capillary cap_values;
};

class ModelScene : public QGraphicsScene
{
	Q_OBJECT

public:
	ModelScene(const Model &baseline, const Model &m, QObject *parent);
	~ModelScene();

	void setOverlay(bool active_overlay);

	const Model & model() const { return m; }
	const Model &baselineModel() const { return baseline; }
	QRectF generationRect(int n) const;
	QRectF vesselRect(VesselView::Type type, int gen, int idx);

	// FIXME: Qt QGraphicsView draw index is broken so we have to
	// emulate an index with this function, hiding/unhiding elements
	// as they come into view.
	// IF you need multiple views of the scene, this needs to be disabled!
	void setVisibleRect(const QRectF &r, const QRect &screen_r);

public slots:
	void updateModelValues();

protected:
	virtual void drawBackground(QPainter *painter, const QRectF &rect);
	virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);

	virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent *event);

	void setupDisplay();

protected slots:
	void transducerPositionChange(QAction *ac);

private:
	const Model & m;
	const Model & baseline;

	VesselItem *arteries, *veins;

	// capilaries do not have `con` member set
	// these are connected to veins/arteries directly
	VesselItem *caps;

	// Transducer
	QGraphicsItem *transducer[3]; // top, middle, bottom

	// vertical lines in the background separating generations
	QList<QLineF> lines;


signals:
	void arteryDoubleClicked(int gen, int no);
	void veinDoubleClicked(int gen, int no);
	void capillaryDoubleClicked(int no);

	void updateTransducerPosition(Model::Transducer);
};
