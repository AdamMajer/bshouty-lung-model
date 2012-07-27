#include <QPixmap>
#include <QWidget>

class OverlayMapWidget : public QWidget
{
	Q_OBJECT

public:
	OverlayMapWidget(const QImage &map, QWidget *parent=0);
	virtual ~OverlayMapWidget();

public slots:
	void setGrid(bool is_visible);

protected:
	virtual void paintEvent(QPaintEvent *ev);

private:
	bool grid_visible;
	QPixmap map;
};
