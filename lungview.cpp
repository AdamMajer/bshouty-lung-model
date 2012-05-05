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

#include <QMouseEvent>
#include <QWheelEvent>
#include "lungview.h"
#include "modelscene.h"

LungView::LungView(QWidget *parent)
        : QGraphicsView(parent)
{
	init();
}

LungView::LungView(QGraphicsScene *scene, QWidget *parent)
	: QGraphicsView(scene, parent)
{
	init();
}

LungView::~LungView()
{

}

void LungView::drawForeground(QPainter *painter, const QRectF &rect)
{
	// top 1ine of the window is for labels of generation in a given column
	int dpx = viewport()->logicalDpiX();
	int dpy = viewport()->logicalDpiY();
	QColor bg_color(240, 240, 240, 120);
	QRectF r = mapToScene(QRect(0, 0, width(), dpy)).boundingRect();

	painter->setPen(QPen(Qt::darkGray, r.height() / 90));
	painter->drawLine(r.bottomLeft(), r.bottomRight());
	painter->fillRect(r, bg_color);

	// labels are 50 points in size (80 points == 1 in.)
	ModelScene *s = dynamic_cast<ModelScene*>(scene());
	if (s != 0) {
		int n_gen = s->model().nGenerations();
		QFont f("Serif");
		f.setPointSizeF(50);
		painter->resetTransform();
		painter->setFont(f);
		for (int i=1; i<=n_gen; ++i) {
			// artery side
			QRectF ar = s->generationRect(i);

			if (i == n_gen) {
				// merge both sides of model into 1 label
				// centered on the capillaries
				ar.setRight(200.0 - ar.left());
			}

			if (r.right() > ar.left() && r.left() < ar.right())
				drawGenLabel(painter, mapFromScene(ar).boundingRect(), i);

			// vein side
			QRectF vr = ar;
			vr.setRight(200.0 - ar.left());
			vr.setLeft(vr.right() - ar.width());

			if (r.right() > vr.left() && r.left() < vr.right())
				drawGenLabel(painter, mapFromScene(vr).boundingRect(), i);
		}
	}
}

void LungView::scrollContentsBy(int dx, int dy)
{
	QGraphicsView::scrollContentsBy(dx, dy);
	viewport()->update(0, 0, width(), viewport()->logicalDpiY()*5/4 + dy);
}

void LungView::keyPressEvent(QKeyEvent *event)
{
	switch (event->key()) {
	case Qt::Key_Control:
		fControlButtonDown = true;
		viewport()->setCursor(Qt::SizeAllCursor);
		break;
	case Qt::Key_Plus:
	case Qt::Key_Equal:
		zoom(1.15, mapToScene(rect().center()));
		event->accept();
		return;
	case Qt::Key_Minus:
	case Qt::Key_Underscore:
		zoom(0.85, mapToScene(rect().center()));
		event->accept();
		return;
	}

	QGraphicsView::keyPressEvent(event);
}

void LungView::keyReleaseEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Control) {
		fControlButtonDown = false;
		viewport()->setCursor(Qt::OpenHandCursor);
	}

	QGraphicsView::keyReleaseEvent(event);
}

bool LungView::viewportEvent(QEvent *event)
{
	bool ret = QGraphicsView::viewportEvent(event);

	switch (event->type()) {
	case QEvent::Resize:
	//case QEvent::Show:
		setMarginToSceneTransform();
	default:
		;
	}

	return ret;
}

void LungView::wheelEvent(QWheelEvent *event)
{
	// zoom in/out function with mouse wheel when Ctrl is pressed
	if (fControlButtonDown) {
		// 1 wheel rotation is 3x zoom
		double max_delta = event->delta() < -2880.0/3.01 ? -2880.0/3.01 : event->delta();
		double z = 1.0 + 3.0 * max_delta / 2880.0;

		zoom(z, mapToScene(event->pos()));
		event->accept();
	}
	else
		QGraphicsView::wheelEvent(event);
}

void LungView::drawGenLabel(QPainter *p, QRect r, int gen)
{
	r.setTop(0);
	r.setHeight(viewport()->logicalDpiY());
	if (r.width() > viewport()->logicalDpiX()*2/3)
		p->drawText(r, QString::number(gen), QTextOption(Qt::AlignCenter));
}

void LungView::zoom(double scale, QPointF scene_point)
{

	// limit min zoom
	QTransform t = transform();
	if (qMax(t.m11(), t.m22())*scale < 1.00)
		scale = 1.0 / qMax(t.m11(), t.m22());

	// limit max zoom, depending on numGenerations()
	ModelScene *s = dynamic_cast<ModelScene*>(scene());
	if (s != 0) {
		double zoom_max = 7 << s->model().nGenerations();
		if (qMin(t.m11(), t.m22())*scale > zoom_max)
			scale = zoom_max / qMin(t.m11(), t.m22());
	}

	QTransform matrix;
	matrix.translate(scene_point.x(), scene_point.y());
	matrix.scale(scale, scale);
	matrix.translate(-scene_point.x(), -scene_point.y());
	setTransform(matrix, true);
}

void LungView::init()
{
	// setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
	setOptimizationFlags(QGraphicsView::DontClipPainter |
	                     QGraphicsView::DontAdjustForAntialiasing);
	setRenderHint(QPainter::Antialiasing);
	setDragMode(QGraphicsView::ScrollHandDrag);
	setTransformationAnchor(QGraphicsView::NoAnchor);

	fControlButtonDown = false;
}

void LungView::setMarginToSceneTransform()
{
	/* We require 1 in. of margin on the top of the viewport to view our
	 * header. So we adjust scene rect accoridng to the scaling factor
	 */
	int dpy = logicalDpiY();
	QRect r(QPoint(0,0), QSize(10,dpy));
	double translated_height = mapToScene(r).boundingRect().height();
	qDebug("translated height: %f %d", translated_height, dpy);

	setSceneRect(-30, -25-translated_height, 255, 150+translated_height);
}

