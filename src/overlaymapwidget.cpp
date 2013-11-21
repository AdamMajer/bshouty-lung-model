#include "overlaymapwidget.h"
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QTextStream>
#include <cmath>
#include "common.h"
#include "lungview.h"
#include <limits>

OverlayMapWidget::OverlayMapWidget(const QImage &map,
                                   const OverlaySettings &s,
                                   QWidget *parent)
        : QWidget(parent),
//          map(QPixmap::fromImage(map)),
          original_map(map),
          settings(s)
{
	grid_visible = false;

	info_widget = new QLabel(this);
	info_widget->setFrameStyle(QFrame::Box | QFrame::Plain);
	info_widget->setStyleSheet("background: yellow;");

	setMouseTracking(true);
	setCursor(Qt::CrossCursor);
}

OverlayMapWidget::~OverlayMapWidget()
{
	delete info_widget;
}

void OverlayMapWidget::setGrid(bool is_visible)
{
	/* nGenerations()*2 x 32 grid */
	if (grid_visible == is_visible)
		return;

	grid_visible = is_visible;
	update();
}

void OverlayMapWidget::paintEvent(QPaintEvent *ev)
{
	QPainter p(this);
	int dpx = logicalDpiX();
	int dpy = logicalDpiY();
	const QRect paint_rect = ev->rect();
	const QRect map_rect = rect().adjusted(0, 0, 0, -dpy);
	const QRect legend_rect = rect().adjusted(0, map_rect.height(), 0, 0);

	p.eraseRect(paint_rect);

	const QRect map_paint_rect = paint_rect & map_rect;
	QPen pen = p.pen();
	if (!map_paint_rect.isEmpty()) {
		p.drawPixmap(map_paint_rect, map, map_paint_rect);

		pen.setCapStyle(Qt::FlatCap);

		if (grid_visible) {
			/* nGenerations()*2 x 32 grid, with border */
			QVector<QLineF> grid;
			const int h = map_rect.height();
			const int w = map_rect.width();
			const double y_spacing = h / 32.0;
			const double x_spacing = w / (nGenerations()*2.0);

			const double x_start = map_paint_rect.left() - fmod(map_paint_rect.left(), x_spacing);
			const double y_start = map_paint_rect.top() - fmod(map_paint_rect.top(), y_spacing);

			for (double x=x_start; static_cast<int>(x)<=map_paint_rect.right(); x+=x_spacing)
				grid << QLineF(x, map_paint_rect.top(), x, map_paint_rect.bottom()+1);
			for (double y=y_start; static_cast<int>(y)<=map_paint_rect.bottom(); y+=y_spacing)
				grid << QLineF(map_paint_rect.left(), y, map_paint_rect.right()+1, y);

			pen.setWidth(1);
			p.setPen(pen);
			p.drawLines(grid);
		}

		QVector<QLineF> lines;
		// NOTE: don't split the first generation as it "belongs" to both lungs
		lines << QLineF(map_rect.width()/(nGenerations()*2), map_rect.height()/2.0,
		                map_rect.width()*static_cast<double>(nGenerations()*2-1)/nGenerations(), map_rect.height()/2.0);
		lines << QLineF(map_rect.width()/2.0, 0,
		                map_rect.width()/2.0, map_rect.height());
		pen.setWidth(3);
		p.setPen(pen);
		p.drawLines(lines);
	}

	// Draw legend
	if (!(legend_rect & paint_rect).isEmpty()) {
		const QRect legend_bar_rect = legend_rect.adjusted(dpx, dpy/2, -dpx, 0);
		pen.setWidth(1);
		p.setPen(pen);

		QLinearGradient gr(legend_bar_rect.left(), 0, legend_bar_rect.right(), 0);
		gr.setColorAt(0, QColor(0, 0, 0xff, 0x7f));
		gr.setColorAt(0.5, Qt::transparent);
		gr.setColorAt(1, QColor(0xff, 0, 0, 0x7f));
		p.setBrush(gr);
		p.drawRect(legend_bar_rect);

		for (double x = dpx; static_cast<int>(x+1)<legend_bar_rect.right(); x += legend_bar_rect.width()/11.0) {
			p.drawLine(x, legend_bar_rect.top(),
			           x, legend_bar_rect.bottom());
		}

		for (int text_pos=0; text_pos<5; text_pos++) {
			QFontMetrics fm = p.fontMetrics();
			int x = legend_bar_rect.left() + legend_bar_rect.width()*text_pos/4;
			int y = legend_bar_rect.top() - fm.descent();

			QString txt;
			{
				QTextStream out(&txt, QIODevice::WriteOnly);
				out.setRealNumberNotation(QTextStream::FixedNotation);
				out.setRealNumberPrecision(1);

				double value;
				switch (settings.type) {
				case OverlaySettings::Relative:
					value = 2.0*settings.range*(x-dpx-legend_bar_rect.width()/2)/legend_bar_rect.width();
					if (text_pos == 2) { // middle
						out << settings.mean << "L/min";
					}
					else {
						out.setNumberFlags(QTextStream::ForceSign);
						out << value << "%";
					}
					break;
				case OverlaySettings::Absolute:
					value = settings.min + (settings.max-settings.min)*text_pos/4;
					if (settings.percent_absolute_scale)
						out << value/settings.max*100.0 << "%";
					else
						out << value << "L/min";
					break;
				}
			}

			p.drawText(QPoint(x-fm.width(txt)/2, y), txt);
		}
	}

	ev->accept();
}

void OverlayMapWidget::leaveEvent(QEvent *ev)
{
	QWidget::leaveEvent(ev);

	info_widget->hide();
}

void OverlayMapWidget::mouseMoveEvent(QMouseEvent *ev)
{
	const QPoint & pos = ev->pos();
	QRect map_rect = mapRect();

	if (!containsMap(pos)) {
		info_widget->hide();
		return;
	}

	info_widget->show();
	const double value = mapValue(pos);

	const QString text = QLatin1String("Gen: %4\nVessel: %5 of %6\n\nValue: %1\nMean: %2 %3%");
	const int gen_no = genNo(pos);
	const int n_elem = 1 << (gen_no-1);
	double mean=1.0;
	switch (settings.type) {
	case OverlaySettings::Absolute:
		mean = (settings.min + settings.max) / 2.0;
		break;
	case OverlaySettings::Relative:
		mean = settings.mean;
		break;
	}

	info_widget->setText(text.arg(doubleToString(value / n_elem),
	                              doubleToString(mean / n_elem),
	                              (value>mean ? "+" : "") + doubleToString((value - mean)/mean*100.0),
	                              QString::number(gen_no),
	                              QString::number(static_cast<int>(n_elem * pos.y() / map_rect.height()) + 1),
	                              QString::number(n_elem)));
	info_widget->resize(info_widget->sizeHint());

	const QSize s = info_widget->size();
	const int min_dist = s.height();

	QPoint widget_pos;
	if (pos.x() + s.width() + min_dist < width()) {
		widget_pos.setX(pos.x() + min_dist);
	}
	else {
		widget_pos.setX(std::max(0, pos.x() - min_dist - s.width()));
	}

	widget_pos.setY(std::max(0, std::min(pos.y(), height()-min_dist)));
	info_widget->move(widget_pos);

	// update legend
	update(rect().adjusted(0, map_rect.bottom(), 0, 0));

	ev->accept();
}

void OverlayMapWidget::resizeEvent(QResizeEvent *ev)
{
	QSize s = ev->size();
	s.setHeight(std::max(0, s.height() - logicalDpiY()));
	scaleMapToSize(s);
}

void OverlayMapWidget::scaleMapToSize(const QSize &s)
{
	if (map.size() == s)
		return;

	map = QPixmap::fromImage(
	              original_map.scaled(s,
	                                  Qt::IgnoreAspectRatio,
	                                  Qt::FastTransformation));
}

void OverlayMapWidget::showEvent(QShowEvent *ev)
{
	QWidget::showEvent(ev);

	const QPoint mouse_pos = info_widget->mapFromGlobal(QCursor::pos());
	if (mapRect().contains(mouse_pos))
		info_widget->show();
	else
		info_widget->hide();
}

QRect OverlayMapWidget::mapRect() const
{
	return rect().adjusted(0, 0, 0, -logicalDpiY());
}

bool OverlayMapWidget::containsMap(const QPoint &pos) const
{
	return mapRect().contains(pos);
}

double OverlayMapWidget::mapValue(const QPoint &pos) const
{
	QRect map_rect = mapRect();
	QColor c = original_map.pixel(pos.x()*original_map.width()/map_rect.width(),
	                              pos.y()*original_map.height()/map_rect.height());
	double n = LungView::gradientToDistanceFromMean(c);

	double range=1.0, mean=1.0;
	switch (settings.type) {
	case OverlaySettings::Absolute:
		range = settings.max - settings.min;
		mean = (settings.max + settings.min) / 2.0;
		break;
	case OverlaySettings::Relative:
		range = settings.mean * settings.range * 2.0 / 100.0;
		mean = settings.mean;
		break;
	}

	return n*range/2.0 + mean;
}

int OverlayMapWidget::genNo(const QPoint &pos) const
{
	QRect r = mapRect();
	QSize s = r.size();
	int x = pos.x() - r.left();

	if (x > s.width()/2)
		x = s.width() - x;

	double n = static_cast<double>(s.width()) / nGenerations() / 2.0;

	// Using 0.99999.. instead of 1.0 to force downward the case where
	// pixel on the line exactly in the middle can be considered to be
	// part of the next generation
	return static_cast<int>(x/n + 0.999999);
}
