#ifndef SpecialFixedFlowWidget_H
#define SpecialFixedFlowWidget_H

#include <QPixmap>
#include <QWidget>
#include "model/model.h"
#include "overlaysettingsdlg.h"

class QLabel;
class SpecialFixedFlowWidget : public QWidget
{
	Q_OBJECT

public:
	SpecialFixedFlowWidget(const Model &model, QWidget *parent=0);
	virtual ~SpecialFixedFlowWidget();

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

#endif // SpecialFixedFlowWidget_H
