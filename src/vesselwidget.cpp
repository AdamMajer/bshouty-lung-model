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

	double scaling_x = widget_rect.width() / (double)data_width / 1.25;
	double scaling_y = widget_rect.height() / (3*y_size);

	int idx = std::max(0, static_cast<int>(ev->rect().left() / scaling_x));
	int end_idx = std::min(data_width, static_cast<int>(ev->rect().right() / scaling_x)+1);

	QPainterPath top_path, bottom_path;

	top_path.moveTo(idx*scaling_x + widget_rect.width()/8.0,
	                widget_rect.height()/2 - vessel_r[idx]*scaling_y);
	bottom_path.moveTo(idx*scaling_x + widget_rect.width()/8.0,
	                   widget_rect.height()/2 + vessel_r[idx]*scaling_y);

	while (++idx < end_idx) {
		top_path.lineTo(idx*scaling_x + widget_rect.width()/8.0,
		                widget_rect.height()/2 - vessel_r[idx]*scaling_y);
		bottom_path.lineTo(idx*scaling_x + widget_rect.width()/8.0,
		                   widget_rect.height()/2 + vessel_r[idx]*scaling_y);
	}

	p.setPen(QPen(Qt::red, 2, Qt::SolidLine, Qt::RoundCap));
	p.setRenderHint(QPainter::Antialiasing);
	p.drawPath(top_path);
	p.drawPath(bottom_path);
}
