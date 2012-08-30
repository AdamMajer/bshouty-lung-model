#include "vesselwidget.h"
#include <QPaintEvent>
#include <QPainter>

void VesselWidget::paintEvent(QPaintEvent *ev)
{
	int data_width = vessel_r.size();
	const QSize widget_rect = size();

	if (data_width < 2)
		return;

	QPainter p(this);
	p.setClipRegion(ev->region());
	p.eraseRect(ev->rect());

	const int w = widget_rect.width();
	const int h = widget_rect.height();
	double scaling_x = w / (double)data_width / 1.25;
	double scaling_y = h / (3*y_size);

	int idx = std::max(0, static_cast<int>((w-ev->rect().right()-w/8) / scaling_x));
	int end_idx = std::min(data_width, static_cast<int>((w-ev->rect().left()-w/8) / scaling_x)+1);

	QPainterPath top_path, bottom_path;
	top_path.moveTo(w - (idx*scaling_x + w/8.0),
	                h/2 - vessel_r[idx]*scaling_y);
	bottom_path.moveTo(w - (idx*scaling_x + w/8.0),
	                   widget_rect.height()/2 + vessel_r[idx]*scaling_y);

	while (++idx < end_idx) {
		top_path.lineTo(w - (idx*scaling_x + w/8.0),
		                h/2 - vessel_r[idx]*scaling_y);
		bottom_path.lineTo(w - (idx*scaling_x + w/8.0),
		                   h/2 + vessel_r[idx]*scaling_y);
	}

	p.setPen(QPen(Qt::red, 2, Qt::SolidLine, Qt::RoundCap));
	p.setRenderHint(QPainter::Antialiasing);
	p.drawPath(top_path);
	p.drawPath(bottom_path);

	// flow direction arrow
	p.drawLine(3*w/30, h/2, 10*w/30, h/2);
	p.drawLine(8*w/30, 9*h/20, 10*w/30, h/2);
	p.drawLine(8*w/30, 11*h/20, 10*w/30, h/2);

	QFont font = p.font();
	font.setPointSizeF(font.pointSizeF() * (h/10) / p.fontMetrics().height());
	p.setFont(font);
	p.drawText(12*w/30, h/2 + p.fontMetrics().ascent()/2, QLatin1String("Flow"));
}
