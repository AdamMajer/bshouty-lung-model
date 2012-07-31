#include <QPixmap>
#include <QWidget>

class QLabel;
class OverlayMapWidget : public QWidget
{
	Q_OBJECT

public:
	OverlayMapWidget(const QImage &map, double mean_value, double stddev, QWidget *parent=0);
	virtual ~OverlayMapWidget();

public slots:
	void setGrid(bool is_visible);

protected:
	virtual void paintEvent(QPaintEvent *ev);

	virtual void enterEvent(QEvent *);
	virtual void leaveEvent(QEvent *);
	virtual void mouseMoveEvent(QMouseEvent *ev);

	virtual void showEvent(QShowEvent *ev);

private:
	bool grid_visible;
	QPixmap map;

	const QImage & original_map;
	QLabel *info_widget;

	const double mean, stddev;
};
