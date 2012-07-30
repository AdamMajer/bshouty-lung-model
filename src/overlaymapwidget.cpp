#include "overlaymapwidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <cmath>

OverlayMapWidget::OverlayMapWidget(const QImage &map, QWidget *parent)
        : QWidget(parent),
          map(QPixmap::fromImage(map))
{
	grid_visible = false;
}

OverlayMapWidget::~OverlayMapWidget()
{

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
	QPainter p(this);

	const QRect paint_rect = ev->rect();
	const double y_scale = map.height() / (double)height();
	const double x_scale = map.width() / (double)width();

	const QRect pixmap_rect(paint_rect.left()*x_scale, paint_rect.top()*y_scale,
	                        paint_rect.width()*x_scale, paint_rect.height()*y_scale);

	p.eraseRect(paint_rect);
	p.drawPixmap(paint_rect, map, pixmap_rect);

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
