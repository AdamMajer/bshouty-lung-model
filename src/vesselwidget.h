#include <QWidget>
#include <vector>


class VesselWidget : public QWidget
{
public:
	VesselWidget(QWidget *parent=0) : QWidget(parent) {
		setStyleSheet("background:");
	}
	virtual ~VesselWidget() {}

	void setDrawData(const std::vector<double> &vessel_r_data) {
		y_size = 0.0;
		vessel_r = vessel_r_data;

		foreach (double v, vessel_r_data)
			y_size = std::max(y_size, v);
		update();
	}

protected:
	virtual void paintEvent(QPaintEvent *ev);

private:
	std::vector<double> vessel_r;
	double y_size;
};
