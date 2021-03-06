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

#include <QFont>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QPainter>
#include "modelscene.h"
#include "transducerview.h"

#ifdef _MSC_VER
# define _USE_MATH_DEFINES
# include <math.h>
# define exp2(x) exp(x*M_LN2)
#else
# include <math.h>
#endif

ModelScene::ModelScene(const Model &base, const Model &mod, QObject *parent)
        : QGraphicsScene(parent),
          m(mod), baseline(base)
{
	setSceneRect(-25, -25, 250, 150);

	arteries = new VesselItem[m.numArteries()];
	veins = new VesselItem[m.numVeins()];
	caps = new VesselItem[m.numCapillaries()];

	setItemIndexMethod(QGraphicsScene::NoIndex);

	double x_offset = 0;
	double y_offset = 96;

	for (int gen=1; gen<=m.nGenerations(); ++gen) {
		QTransform shear_transform = QTransform();
		double split_lung_offset = (gen>1) ? 10 : 0;
		int nElements = m.nElements(gen);
		int n = m.startIndex(gen);

		double cell_scaling_factor = 0.1/exp2(gen);
		bool final_generation = (gen == m.nGenerations());

		for (int i=0; i<nElements; ++i, ++n) {
			QTransform vein_mirror = QTransform(-1, 0, 0, 1, 0, 0);
			QTransform scale = QTransform().scale(cell_scaling_factor,
			                                      cell_scaling_factor);
			QTransform art_offset = QTransform().translate(x_offset,
			                                               y_offset*i);
			QTransform vein_offset = QTransform().translate(x_offset-200,
			                                                y_offset*i);

			VesselView *art = new VesselView(&baseline.artery(gen, i),
			                                 &m.artery(gen, i),
			                                 VesselView::Artery, gen, i);
			VesselView *vein = new VesselView(&baseline.vein(gen, i),
			                                  &m.vein(gen, i),
			                                  VesselView::Vein, gen, i);

			if (i<nElements/2) {
				art_offset.translate(0, -split_lung_offset);
				vein_offset.translate(0, -split_lung_offset);
			}
			else {
				art_offset.translate(0, +split_lung_offset);
				vein_offset.translate(0, +split_lung_offset);
			}

			art->setTransform(scale * art_offset);
			vein->setTransform(scale * vein_offset * vein_mirror);

			addItem(art);
			addItem(vein);

			arteries[n].v = art;
			veins[n].v = vein;


			if (final_generation) {
				// Add Capillaries
				CapillaryView *c = new CapillaryView(&baseline.capillary(i),
				                                     &m.capillary(i),
				                                     &baseline.artery(m.nGenerations()+1, i),
				                                     &m.artery(m.nGenerations()+1, i),
				                                     i);

				c->setTransform(scale * art_offset);
				c->setZValue(100.0);

				addItem(c);

				caps[i].con = 0;
				caps[i].v = c;

				arteries[n].con = 0;
				veins[n].con = 0;
			}
			else {
				// Add connecting items to next generation
				double y_offset = (gen==1) ? 200.0 : 0.0;

				VesselConnectionView *art_con = new VesselConnectionView(VesselView::Artery, gen, i, y_offset);
				VesselConnectionView *vein_con = new VesselConnectionView(VesselView::Vein, gen, i, y_offset);

				art_con->setTransform(shear_transform * scale * art_offset);
				vein_con->setTransform(shear_transform * scale * vein_offset * vein_mirror);

				addItem(art_con);
				addItem(vein_con);

				arteries[n].con = art_con;
				veins[n].con = vein_con;
			}
		}



		// Adjust offsets for next generation
		x_offset += 1000.0 * cell_scaling_factor;
		y_offset /= 2.0;
	}

	// Add visible transducer to the scene
	transducer[0] = new TransducerView(Model::Top);
	transducer[1] = new TransducerView(Model::Middle);
	transducer[2] = new TransducerView(Model::Bottom);

	for (int i=0; i<3; ++i) {
		const QTransform transducer_scale = QTransform(0.5, 0, 0, 0.5, 0, 0);
		QTransform t = transducer[i]->transform();

		transducer[i]->setTransform(transducer_scale * t);
		//transducer[i]->setZValue(-100);
		addItem(transducer[i]);
	}

	// Add labels
	QFont font("Arial", 10, 75);
	QFontMetrics fm(font);
	int pa_w = fm.width("Pulmonary");
	int h = fm.height();
	QGraphicsTextItem *item = new QGraphicsTextItem("Pulmonary\nArtery");
	item->setPos(-2*pa_w/5, 50-h);
	item->setScale(0.4);
	item->setFont(font);
	//item->setZValue(-100);
	addItem(item);

	item = new QGraphicsTextItem("Left\nAtrium");
	item->setFont(font);
	item->setScale(0.4);
	item->setPos(190, 50-h);
	//item->setZValue(-100);
	addItem(item);

	item = new QGraphicsTextItem("Left Lung");
	item->setFont(font);
	item->setScale(0.3);
	item->setPos(91, 37);
	//item->setZValue(-100);
	addItem(item);

	item = new QGraphicsTextItem("Right Lung");
	item->setFont(font);
	item->setScale(0.3);
	item->setPos(90, 52);
	//item->setZValue(-100);
	addItem(item);

	QGraphicsLineItem *line = new QGraphicsLineItem(QLineF(75, 49, 125, 49));
	line->setPen(QPen(QColor(128, 200, 128, 128), 0.25, Qt::DotLine));
	//line->setZValue(-100);
	addItem(line);

	updateModelValues();
}

ModelScene::~ModelScene()
{
	delete []arteries;
	delete []veins;
	delete []caps;
}

void ModelScene::setOverlay(bool active_overlay)
{
	const int n_elem = m.nElements();
	for (int i=0; i<n_elem; ++i) {
		arteries[i].v->setClear(active_overlay);
		veins[i].v->setClear(active_overlay);

		if (arteries[i].con != NULL) {
			arteries[i].con->setClear(active_overlay);
			veins[i].con->setClear(active_overlay);
		}
	}
}

QRectF ModelScene::generationRect(int n) const
{
	/*
	double x = 0.0;
	for (int i=1; i<n; ++i)
		x += 100 / exp2(i);

	return QRectF(x, 0, 100/exp2(n), 100);
	*/

	if (n<1 || n>16)
		return QRectF();

	const double rect_w[17] = { 0,
	                            50,
	                            75,
	                            87.5,
	                            93.75,
	                            96.875,
	                            98.4375,
	                            99.21875,
	                            99.609375,
	                            99.8046875,
	                            99.90234375,
	                            99.951171875,
	                            99.9755859375,
	                            99.98779296875,
	                            99.993896484375,
	                            99.9969482421875,
	                            99.99847412109375
	                          };

	int extra_h = 10;

	return QRectF(rect_w[n-1], -extra_h, rect_w[n]-rect_w[n-1], 96 + extra_h*2);
}

QRectF ModelScene::vesselRect(VesselView::Type type, int gen, int idx)
{
	if (gen < 1 || gen > 16 || idx < 0 || idx > m.nElements(gen))
		return QRectF();

	VesselView *vessel = 0;
	QTransform t;
	switch (type) {
	case VesselView::Artery:
		vessel = arteries[m.nElements(gen) - 1 + idx].v;
		break;
	case VesselView::Vein:
		vessel = veins[m.nElements(gen) - 1 + idx].v;
		break;
	case VesselView::Capillary:
		vessel = caps[idx].v;
		break;
	case VesselView::Connection:
	case VesselView::CornerVessel:
		break;
	}

	// retrieve vessel rect based on its transformation and boundingRect
	if (vessel == 0)
		return QRectF();

	return vessel->transform().mapRect(vessel->boundingRect());
}

void ModelScene::setVisibleRect(const QRectF &r, const QRect &screen_r)
{
	double min_generation = 4;
	double h_scaling = r.width() / screen_r.width();
	for (int i=4; i<=m.nGenerations(); ++i) {
		if (vesselRect(VesselView::Artery, i, 0).width() < 4.0*h_scaling)
			break;

		min_generation++;
	}

	struct {
		double x, y;
		VesselView::Type type;
		int gen;
	} vessel_rect[2]; // top-left, and bottom-right visible vessels

	vessel_rect[0].x = r.left() - r.width()*0.5;
	vessel_rect[0].y = r.top() - r.height()*0.5 + 10;
	vessel_rect[1].x = r.right() + r.width()*0.5;
	vessel_rect[1].y = r.bottom() + r.height()*0.5 + 10;

	for (int i=0; i<2; ++i) {
		double & x = vessel_rect[i].x;
		double & y = vessel_rect[i].y;

		if (y > 48.0)
			y = std::max(48.0, y-20.0);

		if (x < 0.0) {
			vessel_rect[i].type = VesselView::Artery;
			vessel_rect[i].gen = 1;
		}
		else {
			if (x < 100.0) {
				vessel_rect[i].type = VesselView::Artery;
			}
			else {
				vessel_rect[i].type = VesselView::Vein;
				x = -(x - 200.0);
			}

			vessel_rect[i].gen = std::max(1,
			                              std::min(16,
			                                       int(log(200.0/(100-x))/M_LN2)));
		}
	}

	int n_elements = m.nElements();

	int gen_no = 1;
	int idx = 0;
	int gen_elements = m.nElements(gen_no);
	for (int i=0; i<n_elements; ++i, ++idx) {
		if (idx >= gen_elements) {
			idx = 0;
			gen_no++;
			gen_elements = m.nElements(gen_no);
		}

		bool artery_visible =
		                min_generation > gen_no &&

		                vessel_rect[0].type == VesselView::Artery &&
		                vessel_rect[0].gen <= gen_no &&
		                ( vessel_rect[1].type == VesselView::Vein ||
		                  vessel_rect[1].gen >= gen_no ) &&

		                idx >= int(vessel_rect[0].y / 96.0 * gen_elements) &&
		                idx <= int(vessel_rect[1].y / 96.0 * gen_elements)+1;

		bool vein_visible =
		                min_generation > gen_no &&

		                ( vessel_rect[0].type == VesselView::Artery ||
		                  vessel_rect[0].gen >= gen_no ) &&
		                vessel_rect[1].type == VesselView::Vein &&
		                vessel_rect[1].gen <= gen_no &&

		                idx >= int(vessel_rect[0].y / 96.0 * gen_elements) &&
		                idx <= int(vessel_rect[1].y / 96.0 * gen_elements)+1;

		VesselItem &art = arteries[i];
		if (art.v->isVisible() != artery_visible) {
			art.v->setVisible(artery_visible);
			if (art.con)
				art.con->setVisible(artery_visible);
		}

		VesselItem &vein = veins[i];
		if (vein.v->isVisible() != vein_visible) {
			vein.v->setVisible(vein_visible);
			if (vein.con)
				vein.con->setVisible(vein_visible);
		}

		if (gen_no == 16)
			caps[idx].v->setVisible(vein_visible || artery_visible);
	}
}

void ModelScene::updateModelValues()
{
	/* Show correct transducer */
	for (int i=0; i<3; ++i)
		transducer[i]->hide();

	switch(m.transducerPos()) {
	case Model::Top:
		transducer[0]->show();
		break;
	case Model::Middle:
		transducer[1]->show();
		break;
	case Model::Bottom:
		transducer[2]->show();
		break;
	}
}

void ModelScene::drawBackground(QPainter *painter, const QRectF &)
{
	if (lines.isEmpty())
		setupDisplay();

	if (!lines.isEmpty()) {
		// draw a vertical line to differenciate each generation
		QColor c(220, 220, 220);
		QPen pen(c, 5);
		int n = 0;

		pen.setCapStyle(Qt::FlatCap);
		painter->setPen(pen);
		foreach (const QLineF &line, lines) {
			if (n++%2 == 0) {
				pen.setWidthF(pen.widthF() / 2.0);
				painter->setPen(pen);
			}
			painter->drawLine(line);
		}
	}
}

void ModelScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
	VesselView *item = dynamic_cast<VesselView*>(itemAt(event->scenePos(),
	                                                    QTransform()));
	if (item == 0)
		return;

	switch (item->type()) {
	case VesselView::Artery:
		emit arteryDoubleClicked(item->generation(), item->vesselIndex());
		break;
	case VesselView::Capillary:
		emit capillaryDoubleClicked(item->vesselIndex());
		break;
	case VesselView::Vein:
		emit veinDoubleClicked(item->generation(), item->vesselIndex());
		break;
	}
}

void ModelScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
	bool popupTransducerMenu = false;
	const QPointF scenePos = event->scenePos();

	switch (m.transducerPos()) {
	case Model::Top:
		popupTransducerMenu = transducer[0]->contains(scenePos);
		break;
	case Model::Middle:
		popupTransducerMenu = transducer[1]->contains(scenePos);
		break;
	case Model::Bottom:
		popupTransducerMenu = transducer[2]->contains(scenePos);
		break;
	}

	if (popupTransducerMenu) {
		QMenu *menu = new QMenu(event->widget());
		QAction *top_tr_action = menu->addAction("Move To Top of Lung");
		QAction *middle_tr_action = menu->addAction("Move To Right Atrial Level");
		QAction *bottom_tr_action = menu->addAction("Move To Bottom of Lung");
		QAction *current = 0;

		top_tr_action->setData(0);
		middle_tr_action->setData(1);
		bottom_tr_action->setData(2);

		switch (m.transducerPos()) {
		case Model::Top:
			top_tr_action->setCheckable(true);
			top_tr_action->setChecked(true);
			top_tr_action->setText("Top of Lung");
			current = top_tr_action;
			break;
		case Model::Middle:
			middle_tr_action->setCheckable(true);
			middle_tr_action->setChecked(true);
			middle_tr_action->setText("Right Atrial Level");
			current = middle_tr_action;
			break;
		case Model::Bottom:
			bottom_tr_action->setCheckable(true);
			bottom_tr_action->setChecked(true);
			bottom_tr_action->setText("Bottom of Lung");
			current = bottom_tr_action;
			break;
		}

		connect(menu, SIGNAL(triggered(QAction*)),
		        SLOT(transducerPositionChange(QAction*)));
		menu->popup(event->screenPos(), current);

		event->accept();
	}
}

void ModelScene::setupDisplay()
{
	QPointF top_left(0, 0);
	QPointF top_right(200, 0);
	QPointF bottom_offset(0, 100);
	int n_gen = m.nGenerations();

	// Vertical lines between generations, except for the final
	// generation and capillaries
	for (int gen=1; gen<n_gen; ++gen) {
		double x_offset = 100.0 / exp2(gen);
		top_left.rx() += x_offset;
		top_right.rx() -= x_offset;

		lines << QLineF(top_left, top_left+bottom_offset);
		lines << QLineF(top_right, top_right+bottom_offset);
	}
}

void ModelScene::transducerPositionChange(QAction *ac)
{
	bool is_ok;
	int pos = ac->data().toInt(&is_ok);

	if (!is_ok || pos<0 || pos>2)
		return;

	switch (pos) {
	case 0:
		emit updateTransducerPosition(Model::Top);
		break;
	case 1:
		emit updateTransducerPosition(Model::Middle);
		break;
	case 2:
		emit updateTransducerPosition(Model::Bottom);
		break;
	}

	update();
}
