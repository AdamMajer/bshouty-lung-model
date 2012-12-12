#ifndef SpecialGeometricFlowWidget_H
#define SpecialGeometricFlowWidget_H

#include <QPixmap>
#include <QWidget>
#include "model/model.h"
#include "overlaysettingsdlg.h"

class QLabel;
class SpecialGeometricFlowWidget : public QWidget
{
	Q_OBJECT

public:
	SpecialGeometricFlowWidget(const Model &model, QWidget *parent=0);
	virtual ~SpecialGeometricFlowWidget();

public slots:
	void setGrid(bool is_visible);

protected:
	virtual void paintEvent(QPaintEvent *ev);

	virtual void leaveEvent(QEvent *);
	virtual void mouseMoveEvent(QMouseEvent *ev);

private:
	bool grid_visible;
	QPixmap map;
	QLabel *info_widget;
	QImage original_map;

	double mean_flow;
};

#endif // SpecialGeometricFlowWidget_H
