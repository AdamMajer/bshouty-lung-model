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
#include <QTextStream>
#include <QTransform>
#include <QWheelEvent>
#include "lungview.h"
#include "modelscene.h"
#include <typeinfo>

LungView::LungView(QWidget *parent)
	: QGraphicsView(parent),
	  overlay_image(32, 1<<15, QImage::Format_ARGB32)
{
	init();
}

LungView::LungView(QGraphicsScene *scene, QWidget *parent)
	: QGraphicsView(scene, parent),
	  overlay_image(32, 1<<15, QImage::Format_ARGB32)
{
	init();
}

LungView::~LungView()
{

}

void LungView::resetZoom()
{
	double zoom = std::min(width() / scene()->width(),
	                       height() / scene()->height());
	setTransform(QTransform().scale(zoom, zoom));
}

void LungView::setOverlayType(OverlayType type)
{
	overlay_type = type;
	overlay_image.fill(QColor(0, 0, 0, 0));

	ModelScene *s = dynamic_cast<ModelScene*>(scene());
	if (s == NULL)
		return;

	switch (overlay_type) {
	case FlowOverlay:
		calculateFlowOverlay(s->model());
		break;
	case VolumeOverlay:
		calculateVolumeOverlay(s->model());
		break;
	case NoOverlay:
		s->setOverlay(false);
		viewport()->update();
		return;
	}

	overlay_pixmap = QPixmap::fromImage(overlay_image);
	s->setOverlay(true);
	viewport()->update();
}

void LungView::clearOverlay()
{
	setOverlayType(NoOverlay);
}

void LungView::setOverlaySettings(const OverlaySettings &settings)
{
	overlay_settings = settings;
	setOverlayType(overlay_type);
}

QColor LungView::gradientColor(double distance_from_mean)
{
	// gradient from transparent black => 50% red
	// input range [-1..1]

	if (distance_from_mean <= -1.0)
		return QColor::fromRgbF(0, 0, 1.0, 0.5);

	if (distance_from_mean >= 1.0)
		return QColor::fromRgbF(1.0, 0, 0, 0.5);

	if (distance_from_mean < 0)
		return QColor::fromRgbF(0, 0, -distance_from_mean, -0.5*distance_from_mean);

	return QColor::fromRgbF(distance_from_mean, 0, 0, 0.5*distance_from_mean);
}

double LungView::gradientToDistanceFromMean(QColor c)
{
	double b, g, r;

	c.getRgbF(&r, &g, &b, 0);
	if (r > 0)
		return r;
	if (b > 0)
		return -b;

	return 0.0;
}

void LungView::zoomToVessel(int t, int gen, int idx)
{
	ModelScene *s = dynamic_cast<ModelScene*>(scene());
	if (s == 0)
		return;

	VesselView::Type type = static_cast<VesselView::Type>(t);
	QRectF r = s->vesselRect(type, gen, idx);
	if (r.isValid()) {
		double s = std::min(static_cast<double>(width()) / r.width(),
		                    static_cast<double>(height()) / r.height());

		QTransform new_transform;
		new_transform.scale(s, s);
		new_transform.translate(-r.left(), -r.top());

		setTransform(new_transform);
	}
}

void LungView::drawForeground(QPainter *painter, const QRectF &rect)
{
	drawOverlay(painter, rect);

	// top 1ine of the window is for labels of generation in a given column
	// int dpx = viewport()->logicalDpiX();
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
	const int dpy = viewport()->logicalDpiY();

	QGraphicsView::scrollContentsBy(dx, dy);
	viewport()->update(0, 0, width(), dpy*5/4 + dy);
	if (overlay_type != NoOverlay)
		viewport()->update(0, height()-dpy*5/4+std::min(dy,0),
		                   width(), dpy*5/4-std::min(dy,0));
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
		zoomIn();
		event->accept();
		return;
	case Qt::Key_Minus:
	case Qt::Key_Underscore:
		zoomOut();
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

void LungView::paintEvent(QPaintEvent *event)
{
	// calculate which vessels to show
	const QRect v = viewport()->rect();
	const QRectF tv = mapToScene(v).boundingRect();

	try {
		dynamic_cast<ModelScene&>(*scene()).setVisibleRect(tv, v);
	}
	catch (std::bad_cast c) {

	}

	// draw
	QGraphicsView::paintEvent(event);
}

void LungView::drawGenLabel(QPainter *p, QRect r, int gen)
{
	r.setTop(0);
	r.setHeight(viewport()->logicalDpiY());
	if (r.width() > viewport()->logicalDpiX()*2/3)
		p->drawText(r, QString::number(gen), QTextOption(Qt::AlignCenter));
}

void LungView::drawOverlay(QPainter *p, const QRectF &r)
{
	if (overlay_type == NoOverlay)
		return;

	ModelScene *s = dynamic_cast<ModelScene*>(scene());
	if (s == NULL)
		return;

	const int n_gen = s->model().nGenerations();
	for (int i=1; i<=n_gen; ++i) {
		QRectF art_r( s->generationRect(i));
		QRectF vein_r( 200.0 - art_r.right(), art_r.top(),
		               art_r.width(), art_r.height());
		const double offset = (i>1) ? 10.0 : 0.0;
		const double scaling_factor = 32768.0/(art_r.height()-offset*2);

		// Stop at subpixel sizes
		if (p->transform().mapRect(art_r).width() < 1.0)
			break;

		// draw top
		art_r.setHeight(art_r.height()/2.0-offset);
		vein_r.setHeight(vein_r.height()/2.0-offset);

		QRectF draw_art_rect = art_r & r;
		QRectF draw_vein_rect = vein_r & r;

		if (!draw_art_rect.isEmpty()) {
			QRectF src_pixmap_pos(i-1, (draw_art_rect.top()+offset)*scaling_factor,
			                      1.0, draw_art_rect.height()*scaling_factor);
			p->drawPixmap(draw_art_rect, overlay_pixmap, src_pixmap_pos);
		}
		if (!draw_vein_rect.isEmpty()) {
			QRectF src_pixmap_pos(32-i, (draw_vein_rect.top()+offset)*scaling_factor,
			                      1.0, draw_vein_rect.height()*scaling_factor);
			p->drawPixmap(draw_vein_rect, overlay_pixmap, src_pixmap_pos);
		}

		// draw bottom
		art_r.translate(0.0, art_r.height()+2.0*offset);
		vein_r.translate(0.0, vein_r.height()+2.0*offset);

		draw_art_rect = art_r & r;
		draw_vein_rect = vein_r & r;

		if (!draw_art_rect.isEmpty()) {
			QRectF src_pixmap_pos(i-1, (draw_art_rect.top()-offset)*scaling_factor,
			                      1.0, draw_art_rect.height()*scaling_factor);
			p->drawPixmap(draw_art_rect, overlay_pixmap, src_pixmap_pos);
		}
		if (!draw_vein_rect.isEmpty()) {
			QRectF src_pixmap_pos(32-i, (draw_vein_rect.top()-offset)*scaling_factor,
			                      1.0, draw_vein_rect.height()*scaling_factor);
			p->drawPixmap(draw_vein_rect, overlay_pixmap, src_pixmap_pos);
		}
	}

	qDebug("(%f,%f    %f x %f", r.left(), r.top(), r.width(), r.height());

	drawOverlayLegend(p, r);
}

void LungView::drawOverlayLegend(QPainter *p, const QRectF &r)
{
	int dpy = viewport()->logicalDpiY();
	if (height() < dpy*2)
		return;

	const QRect screen_draw_rect = mapFromScene(r).boundingRect();
	const QRect legend_rect = QRect(QPoint(0,viewport()->height()-dpy),
	                                QSize(viewport()->width(), dpy));

	if ((screen_draw_rect & legend_rect).isEmpty())
		return;

	p->save();
	p->setTransform(QTransform());

	p->setPen(QColor(0, 0, 0, 0x7f));
	p->drawLine(legend_rect.topLeft(), legend_rect.topRight());
	// p->fillRect(legend_rect, QColor(0xff, 0xff, 0xff, 0xaf));

	QRect scale_rect = QRect(QPoint(legend_rect.left() + legend_rect.width()/5,
	                                legend_rect.top() + legend_rect.height()/2),
	                         QSize(legend_rect.width()*3/5,
	                               legend_rect.height()*3/8));
	p->setPen(Qt::black);
	p->drawRect(scale_rect);

	QFont f = p->font();
	f.setPixelSize(dpy / 10);
	p->setFont(f);

	const double scale_w = scale_rect.width()/20.0;
	QLinearGradient scale_gradient(scale_rect.topLeft(), scale_rect.topRight());
	scale_gradient.setColorAt(0, Qt::blue);
	scale_gradient.setColorAt(0.5, Qt::transparent);
	scale_gradient.setColorAt(1, Qt::red);

	p->fillRect(scale_rect, scale_gradient);
	p->fillRect(scale_rect, QColor(255, 255, 255, 0x7f));

	for (int i=0; i<21; ++i) {
		const double x = scale_rect.left() + i*scale_w;
		if (i>0 && i<20)
			p->drawLine(QPointF(x, scale_rect.top()),
			            QPointF(x, scale_rect.bottom()));

		const QString &text = overlay_text[i];
		if (!text.isEmpty() && i%5==0) {
			const QRect text_rect(QPoint(x-5*scale_w/2, legend_rect.top()),
			                      QSize(5*scale_w, legend_rect.height()/2));
			p->drawText(text_rect,
			            Qt::AlignBottom |
			            Qt::AlignHCenter |
			            Qt::TextSingleLine,
			            text);
		}
	}

	p->restore();

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
		double zoom_max = 10 << s->model().nGenerations();
		if (qMin(t.m11(), t.m22())*scale > zoom_max)
			scale = zoom_max / qMin(t.m11(), t.m22());
	}

	QTransform matrix;
	matrix.translate(scene_point.x(), scene_point.y());
	matrix.scale(scale, scale);
	matrix.translate(-scene_point.x(), -scene_point.y());
	setTransform(matrix, true);
}

void LungView::calculateFlowOverlay(const Model &model)
{
	// Flow in each generation is the same, so generate a
	// standard deviation from the expected flow for all vessels
	// The gradient should span 2 standard deviations
	const int n_gen = model.nGenerations();
	double variance = 0.0;

	double overlay_mean = model.getResult(Model::Flow_value);
	for (int gen=1; gen<=n_gen; ++gen) {
		const int n_elements = model.nElements(gen);

		for (int i=0; i<n_elements; ++i) {
			const Vessel &art = model.artery(gen, i);
			const Vessel &vein = model.vein(gen, i);

			if (!isnan(art.flow))
				variance += sqr(art.flow*n_elements - overlay_mean);
			if (!isnan(vein.flow))
				variance += sqr(vein.flow*n_elements - overlay_mean);
		}
	}

	// stddev used is no smaller than 1%
	double overlay_stddev = std::max(overlay_mean*0.01, sqrt(variance / (model.nElements()*2)));

	switch (overlay_settings.type) {
	case OverlaySettings::Absolute:
		if (overlay_settings.auto_min)
			overlay_settings.min = overlay_mean - overlay_stddev*2.0;
		if (overlay_settings.auto_max)
			overlay_settings.max = overlay_mean + overlay_stddev*2.0;
		break;
	case OverlaySettings::Relative:
		if (overlay_settings.auto_mean)
			overlay_settings.mean = overlay_mean;
		if (overlay_settings.auto_range)
			overlay_settings.range = 100.0 * 2.0 * overlay_stddev / overlay_mean;
		break;
	}


	// QPainter paint(&overlay_image);
	for (int gen=1; gen<=n_gen; ++gen) {
		const int n_elements = model.nElements(gen);

		// We can use `int` as height() is always a multiple of n_elements
		int paint_height = overlay_image.height() / n_elements;
		int y = 0;

		for (int i=0; i<n_elements; ++i) {
			const Vessel &art = model.artery(gen, i);
			const Vessel &vein = model.vein(gen, i);

			double vein_flow = isnan(vein.flow) ? 0.0 : vein.flow;
			double art_flow = isnan(art.flow) ? 0.0 : art.flow;

			QRgb art_col = qRgb(0,0,0), vein_col = qRgb(0,0,0);
			switch (overlay_settings.type) {
			case OverlaySettings::Absolute: {
				const double range = overlay_settings.max - overlay_settings.min;
				const double mean = overlay_settings.min + range / 2.0;
				art_col = gradientColor(2.0*(art_flow*n_elements - mean)/range).rgba();
				vein_col = gradientColor(2.0*(vein_flow*n_elements - mean)/range).rgba();
				break;
			}
			case OverlaySettings::Relative: {
				const double range = overlay_settings.mean * overlay_settings.range * 2.0 / 100.0;
				const double mean = overlay_settings.mean;
				art_col = gradientColor(2.0*(art_flow*n_elements - mean)/range).rgba();
				vein_col = gradientColor(2.0*(vein_flow*n_elements - mean)/range).rgba();
				break;
			}
			}

			for (int j=y; j<y+paint_height; ++j) {
				overlay_image.setPixel(gen-1, j, art_col);
				overlay_image.setPixel(32-gen, j, vein_col);
			}

			y += paint_height;
		}
	}

	for (int i=0; i<21; i++) {
		overlay_text[i].clear();
		QTextStream label(&overlay_text[i]);

		label.setNumberFlags(QTextStream::ForcePoint);
		label.setRealNumberNotation(QTextStream::FixedNotation);
		label.setRealNumberPrecision(1);

		switch (overlay_settings.type) {
		case OverlaySettings::Absolute: {
			double value = overlay_settings.min + (overlay_settings.max - overlay_settings.min)*i/20.0;
			if (overlay_settings.percent_absolute_scale)
				label << value*100.0/overlay_settings.max << "%";
			else
				label << value << "L/min";
			break;
		}
		case OverlaySettings::Relative:
			if (i==10)
				label << doubleToString(overlay_settings.mean) << "L/min";
			else
				label << (i-10)*overlay_settings.range/10.0 << "%";
			break;
		}
	}
}

void LungView::calculateVolumeOverlay(const Model &model)
{
	// Volume is as a fraction of maximal possible vessel dimention
	// in the baseline vessel.
	// mean is 50% of max volume
	// stddev is 25% of max volume
	//   -> scale is 0-100% of max volume
	// const double um_to_ul = 1e-9; // um^3 => ul
	const int n_gen = model.nGenerations();
	std::vector<double> max_art_size, max_vein_size;
	double mean=1.0, stddev=1.0;

	max_art_size.reserve(model.nElements());
	max_vein_size.reserve(model.nElements());

	// mean as perentage of max vessel volume
	for (int gen=1; gen<=n_gen; ++gen) {
		const int n_elements = model.nElements(gen);

		for (int i=0; i<n_elements; ++i) {
			const Vessel &art = model.artery(gen, i);
			const Vessel &vein = model.vein(gen, i);

			// FIXME: !!
			double art_s_max = 100.0; //art.a*sqr(art.D)*M_PI/4.0*art.length/art.vessel_ratio*um_to_ul;
			double vein_s_max = 100.0; // vein.a*sqr(vein.D)*M_PI/4.0*vein.length/vein.vessel_ratio*um_to_ul;

			max_art_size.push_back(art_s_max);
			max_vein_size.push_back(vein_s_max);

			mean += 100.0*art.volume/art_s_max +
			        100.0*vein.volume/vein_s_max;
		}
	}
	mean = mean / (2.0 * model.nElements());

	// stddev
	std::vector<double>::const_iterator art_size_iter = max_art_size.begin();
	std::vector<double>::const_iterator vein_size_iter = max_vein_size.begin();

	stddev = 0.0;
	for (int gen=1; gen<=n_gen; ++gen) {
		const int n_elements = model.nElements(gen);

		for (int i=0; i<n_elements; ++i) {
			stddev += sqr(100.0*model.artery(gen, i).volume/(*art_size_iter) - mean);
			stddev += sqr(100.0*model.vein(gen, i).volume/(*vein_size_iter) - mean);

			art_size_iter++;
			vein_size_iter++;
		}
	}
	stddev = sqrt(stddev / (2.0 * model.nElements()));

	// calculate actual used values
	double overlay_stddev = 0.0;
	double overlay_mean = 0.0;

	switch (overlay_settings.type) {
	case OverlaySettings::Absolute: {
		overlay_settings.min = (overlay_settings.auto_min ?
		                                std::max(0.0, mean-2.0*stddev) :
		                                overlay_settings.min);
		overlay_settings.max = (overlay_settings.auto_max ?
		                                std::min(100.0, mean+2.0*stddev) :
		                                overlay_settings.max);

		overlay_mean = (overlay_settings.min + overlay_settings.max) / 2.0;
		overlay_stddev = (overlay_settings.max - overlay_settings.min) / 4.0;
		break;
	}
	case OverlaySettings::Relative:
		overlay_settings.mean = (overlay_settings.auto_mean ?
		                                 mean :
		                                 overlay_settings.mean);
		overlay_settings.range = (overlay_settings.auto_range ?
		                                  stddev :
		                                  overlay_settings.range/2.0);

		overlay_mean = overlay_settings.mean;
		overlay_stddev = overlay_settings.range;
	}

	// generate overlay
	art_size_iter = max_art_size.begin();
	vein_size_iter = max_vein_size.begin();

	for (int gen=1; gen<=n_gen; ++gen) {
		const int n_elements = model.nElements(gen);
		int paint_height = overlay_image.height() / n_elements;
		int y = 0;

		for (int i=0; i<n_elements; ++i) {
			const Vessel &art = model.artery(gen, i);
			const Vessel &vein = model.vein(gen, i);

			double a = (100.0*art.volume/(*art_size_iter) - overlay_mean)
			           / (2.0*overlay_stddev);
			QRgb art_col = gradientColor(a).rgba();

			double v = (100.0*vein.volume/(*vein_size_iter) - overlay_mean)
			           / (2.0*overlay_stddev);
			QRgb vein_col = gradientColor(v).rgba();

			for (int j=y; j<y+paint_height; ++j) {
				overlay_image.setPixel(gen-1, j, art_col);
				overlay_image.setPixel(32-gen, j, vein_col);
			}

			y += paint_height;

			art_size_iter++;
			vein_size_iter++;
		}
	}

	// legend
	for (int i=0; i<21; i++) {
		overlay_text[i].clear();
		QTextStream label(&overlay_text[i]);

		label.setRealNumberNotation(QTextStream::FixedNotation);
		label.setRealNumberPrecision(1);

		label << overlay_mean + ((i-10)*2*overlay_stddev) / 10.0
		      << "%";
	}
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

	overlay_type = NoOverlay;
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

