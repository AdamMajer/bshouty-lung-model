#include "overlaymapwidget.h"
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <cmath>
#include "common.h"
#include "lungview.h"

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
	/* 32 x 32 grid */
	if (grid_visible == is_visible)
		return;

	grid_visible = is_visible;
	update();
}

void OverlayMapWidget::paintEvent(QPaintEvent *ev)
{
	if (map.size() != size())
		map = QPixmap::fromImage(original_map.scaled(size()));

	QPainter p(this);

	const QRect paint_rect = ev->rect();
	p.eraseRect(paint_rect);
	p.drawPixmap(paint_rect, map, paint_rect);

	if (grid_visible) {
		/* 32 x 32 grid, with border */
		QVector<QLineF> grid;
		const int h = height()-1;
		const int w = width()-1;
		const double y_spacing = h / 32.0;
		const double x_spacing = w / 32.0;

		const double x_start = paint_rect.left() - fmod(paint_rect.left(), x_spacing);
		const double y_start = paint_rect.top() - fmod(paint_rect.top(), y_spacing);

		for (double x=x_start; static_cast<int>(x)<=paint_rect.right(); x+=x_spacing)
			grid << QLineF(x, paint_rect.top(), x, paint_rect.bottom());
		for (double y=y_start; static_cast<int>(y)<=paint_rect.bottom(); y+=y_spacing)
			grid << QLineF(paint_rect.left(), y, paint_rect.right(), y);

		p.drawLines(grid);
	}

	QPen pen = p.pen();
	pen.setWidth(3);
	p.setPen(pen);

	QVector<QLineF> lines;
	// NOTE: don't split the first generation as it "belongs" to both lungs
	lines << QLineF(width()/32.0, height()/2.0, width()*31.0/32.0, height()/2.0);
	lines << QLineF(width()/2.0, 0, width()/2.0, height());
	p.drawLines(lines);

	ev->accept();
}

void OverlayMapWidget::enterEvent(QEvent *ev)
{
	QWidget::enterEvent(ev);

	info_widget->show();
}

void OverlayMapWidget::leaveEvent(QEvent *ev)
{
	QWidget::leaveEvent(ev);

	info_widget->hide();
}

void OverlayMapWidget::mouseMoveEvent(QMouseEvent *ev)
{
	const QPoint & pos = ev->pos();
	const int h = height();
	const int w = width();

	if (pos.x() >= 0 && pos.y() >= 0 &&
	    pos.x() < w && pos.y() < h) {
		int gen_no = pos.x() * original_map.width() / w;
		const QColor & color = original_map.pixel(gen_no,
		                                          pos.y() * original_map.height() / h);
		const double mean_dist = LungView::gradientToDistanceFromMean(color);
		const double min_max_dist = (mean_dist+1.0)/2.0;
		const double value = settings.min + (settings.max-settings.min)*min_max_dist;

		QString text = QLatin1String("Gen: %4\nVessel: %5 of %6\n\nValue: %1\nMean: %2 %3%");
		// TODO: fix when model works again for other than 16 generations
		if (gen_no > 15)
			gen_no = 31 - gen_no;

		const int n_elem = 1 << gen_no;
		info_widget->setText(text.arg(doubleToString(value / n_elem),
		                              doubleToString((settings.min+(settings.max-settings.min)/2) / n_elem),
		                              doubleToString(min_max_dist*100.0),
		                              QString::number(gen_no+1),
		                              QString::number(static_cast<int>(n_elem * pos.y() / h) + 1),
		                              QString::number(n_elem)));
		info_widget->resize(info_widget->sizeHint());

		const QSize s = info_widget->size();
		const int min_dist = s.height();

		QPoint widget_pos;
		if (pos.x() + s.width() + min_dist < w) {
			widget_pos.setX(pos.x() + min_dist);
		}
		else {
			widget_pos.setX(std::max(0, pos.x() - min_dist - s.width()));
		}

		widget_pos.setY(std::max(0, std::min(pos.y(), h-min_dist)));
		info_widget->move(widget_pos);
	}

	ev->accept();
}

void OverlayMapWidget::showEvent(QShowEvent *ev)
{
	QWidget::showEvent(ev);

	const QPoint mouse_pos = info_widget->mapFromGlobal(QCursor::pos());
	if (info_widget->rect().contains(mouse_pos))
		info_widget->show();
	else
		info_widget->hide();
}
