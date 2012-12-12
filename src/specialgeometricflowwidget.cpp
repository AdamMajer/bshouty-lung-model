#include "specialgeometricflowwidget.h"
#include <QImage>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <cmath>
#include "common.h"
#include "lungview.h"
#include "model/model.h"

SpecialGeometricFlowWidget::SpecialGeometricFlowWidget(const Model &model, QWidget *parent)
        : QWidget(parent),
          original_map(12, 32, QImage::Format_ARGB32)
{
	grid_visible = false;

	info_widget = new QLabel(this);
	info_widget->setFrameStyle(QFrame::Box | QFrame::Plain);
	info_widget->setStyleSheet("background: yellow;");
	info_widget->hide();

	setMouseTracking(true);
	setCursor(Qt::CrossCursor);

	mean_flow = model.getResult(Model::Flow_value);

	for (int gen=1; gen<=6; ++gen) {
		int n_elements = model.nElements(gen);
		double av_flow = mean_flow / n_elements;
		int c = 32/n_elements;

		for (int vessel=0; vessel<n_elements; ++vessel) {
			const Vessel &v = model.artery(gen, vessel);
			double flow = isnan(v.flow) ? 0.0 : v.flow;
			double flow_fraction = flow / av_flow;
			/*
			if (flow_fraction > 1 && n_elements>1)
				flow_fraction = (flow_fraction-1.0)/(n_elements-1.0) + 1.0;
			*/

			double log_flow = log(flow_fraction);
			double max_flow = log((double)n_elements);
			quint32 pixel = LungView::gradientColor(std::max(-max_flow, std::min(max_flow, log_flow)) / max_flow).rgba();

			for (int y=vessel*c; y<(vessel+1)*c; ++y) {
				original_map.setPixel(gen-1, y, pixel);
				original_map.setPixel(12-gen, y, pixel);
			}
		}
	}

	map = QPixmap::fromImage(original_map);

	setMinimumSize(logicalDpiX()*5, logicalDpiY()*6);
}

SpecialGeometricFlowWidget::~SpecialGeometricFlowWidget()
{
	delete info_widget;
}

void SpecialGeometricFlowWidget::setGrid(bool is_visible)
{
	/* 12 x 32 grid */
	if (grid_visible == is_visible)
		return;

	grid_visible = is_visible;
	update();
}

void SpecialGeometricFlowWidget::paintEvent(QPaintEvent *ev)
{
	QPainter p(this);

	int dpy = p.device()->logicalDpiY();
	QRect pix_rect = QRect(QPoint(0,0), size());
	QRect legend_rect = pix_rect;

	pix_rect.setBottom(pix_rect.bottom() - dpy);
	legend_rect.setTop(legend_rect.bottom() - dpy);

	const QRect paint_rect = ev->rect() & pix_rect;
	p.eraseRect(ev->rect());
	p.drawPixmap(pix_rect, map, QRect(QPoint(0,0), map.size()));

	if (grid_visible) {
		/* 12 x 32 grid, with border */
		QVector<QLineF> grid;
		const int h = pix_rect.height()-1;
		const int w = pix_rect.width()-1;
		const double y_spacing = h / 32.0;
		const double x_spacing = w / 12.0;

		const double x_start = paint_rect.left() - fmod(paint_rect.left(), x_spacing);
		const double y_start = paint_rect.top() - fmod(paint_rect.top(), y_spacing);

		for (double x=x_start; static_cast<int>(x)<=paint_rect.right(); x+=x_spacing)
			grid << QLineF(x, paint_rect.top(), x, paint_rect.bottom());
		for (double y=y_start; static_cast<int>(y)<=paint_rect.bottom(); y+=y_spacing)
			grid << QLineF(paint_rect.left(), y, paint_rect.right(), y);

		p.drawLines(grid);
	}

	p.setPen(Qt::red);
	p.drawLine(pix_rect.bottomLeft(), pix_rect.bottomRight());

	QPen pen = p.pen();
	pen.setColor(Qt::black);
	pen.setWidth(3);
	p.setPen(pen);

	QVector<QLineF> lines;
	// NOTE: don't split the first generation as it "belongs" to both lungs
	lines << QLineF(pix_rect.left()+pix_rect.width()/32.0, pix_rect.top()+pix_rect.height()/2.0,
	                pix_rect.left()+pix_rect.width()*31.0/32.0, pix_rect.top()+pix_rect.height()/2.0);
	lines << QLineF(pix_rect.left()+pix_rect.width()/2.0, pix_rect.top(),
	                pix_rect.left()+pix_rect.width()/2.0, pix_rect.bottom());
	p.drawLines(lines);

	/* Draw legend */
	int dpx = p.device()->logicalDpiX();
	QRect r = legend_rect.adjusted(dpx/2, dpy/2, -dpx/2, -1);
	QLinearGradient gradient(r.topLeft(), r.topRight());

	gradient.setColorAt(0, QColor(0, 0, 0xff, 0x7f));
	gradient.setColorAt(0.5, Qt::transparent);
	gradient.setColorAt(1, QColor(0xff, 0, 0, 0x7f));

	pen.setWidth(1);
	p.setPen(pen);
	p.fillRect(r, gradient);
	p.drawRect(r);

	double w = r.width() / 21.0;
	for (double x=r.left()+w; x<r.right(); x+=w) {
		p.drawLine(QPointF(x, r.top()),
		           QPointF(x, r.bottom()));
	}

	p.drawText(r.left() - p.fontMetrics().width("No flow")/2,
	           r.top()-dpy/16, "No flow");

	p.drawText(r.right() - p.fontMetrics().width("Max flow")/2,
	           r.top()-dpy/16, "Max flow");

	p.drawText(r.left() + r.width()/2 - p.fontMetrics().width("Mean flow")/2,
	           r.top()-dpy/16, "Mean flow");

	ev->accept();
}

void SpecialGeometricFlowWidget::leaveEvent(QEvent *ev)
{
	QWidget::leaveEvent(ev);

	info_widget->hide();
}

void SpecialGeometricFlowWidget::mouseMoveEvent(QMouseEvent *ev)
{
	const QPoint & pos = ev->pos();
	const QRect r = rect().adjusted(0, 0, 0, -logicalDpiY());

	if (!r.isValid())
		return;

	const int h = r.height();
	const int w = r.width();

	if (r.contains(pos)) {
		int gen_no = pos.x() * original_map.width() / w + 1;
		const QColor & color = original_map.pixel(gen_no-1,
		                                          pos.y() * original_map.height() / h);
		const double mean_dist = LungView::gradientToDistanceFromMean(color);

		QString text = QLatin1String("Gen: %2\nVessel: %3 of %4\n\nValue: %1\nPercent of Max: %5%");
		// TODO: fix when model works again for other than 16 generations
		if (gen_no > 6)
			gen_no = 12 - gen_no;
		const int n_elem = 1 << (gen_no-1);
		const double value = exp(mean_dist*log(static_cast<double>(n_elem)))*mean_flow/n_elem;

		info_widget->setText(text.arg(doubleToString(value),
		                              QString::number(gen_no),
		                              QString::number(static_cast<int>(n_elem * pos.y() / h) + 1),
		                              QString::number(n_elem),
		                              QString::number((mean_dist+1.0)/2.0 * 100.0)));
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

		widget_pos.setY(std::max(0, std::min(pos.y(), rect().height()-min_dist)));
		info_widget->show();
		info_widget->move(widget_pos);
	}
	else {
		info_widget->hide();
	}

	ev->accept();
}
