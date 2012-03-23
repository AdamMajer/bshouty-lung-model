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

class LungView : public QGraphicsView
{
public:
	LungView(QWidget *parent = 0);
	LungView(QGraphicsScene* scene, QWidget * parent = 0);
	~LungView();

protected:
	virtual void drawForeground(QPainter *painter, const QRectF &rect);
	virtual void scrollContentsBy(int dx, int dy);

	virtual void keyPressEvent(QKeyEvent *event);
	virtual void keyReleaseEvent(QKeyEvent *event);
	virtual bool viewportEvent(QEvent *event);
	virtual void wheelEvent(QWheelEvent *event);

	void drawGenLabel(QPainter *p, QRect r, int gen);
	void zoom(double scale, QPointF scene_point);

private:
	void init();
	void setMarginToSceneTransform();

	bool fControlButtonDown;
};
