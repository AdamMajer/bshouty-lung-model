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

#include <QGraphicsView>

class Model;
class ModelScene;
class LungView : public QGraphicsView
{
public:
	enum OverlayType { NoOverlay,
		           FlowOverlay,
		           FixedFlowOverlay,
		           VolumeOverlay
	};

	LungView(QWidget *parent = 0);
	LungView(QGraphicsScene* scene, QWidget * parent = 0);
	~LungView();

	void zoomIn() { zoom(1.15, mapToScene(rect().center())); }
	void zoomOut() { zoom(0.85, mapToScene(rect().center())); }
	void resetZoom();

	void setOverlayType(OverlayType type);
	OverlayType overlayType() const { return overlay_type; }
	void clearOverlay();

	const QImage& overlayMap() const { return overlay_image; }
	double overlayStddev() const { return overlay_stddev; }
	double overlayMean() const { return overlay_mean; }

	static QColor gradientColor(double stddev_from_mean);
	static double gradientToDistanceFromMean(QColor color);

protected:
	virtual void drawForeground(QPainter *painter, const QRectF &rect);
	virtual void scrollContentsBy(int dx, int dy);

	virtual void keyPressEvent(QKeyEvent *event);
	virtual void keyReleaseEvent(QKeyEvent *event);
	virtual bool viewportEvent(QEvent *event);
	virtual void wheelEvent(QWheelEvent *event);

	void drawGenLabel(QPainter *p, QRect r, int gen);
	void drawOverlay(QPainter *p, const QRectF &r);
	void drawOverlayLegend(QPainter *p, const QRectF &r);
	void zoom(double scale, QPointF scene_point);

	void calculateFlowOverlay(const Model &model);
	void calculateFixedFlowOverlay(const Model &model);
	void calculateVolumeOverlay(const Model &model);

private:
	void init();
	void setMarginToSceneTransform();

	bool fControlButtonDown;
	OverlayType overlay_type;
	QString overlay_text[7];

	QImage overlay_image; // (32x32768), or (gen*2,max_vessel_count_in_gen)
	double overlay_stddev, overlay_mean;
	QPixmap overlay_pixmap;
};
