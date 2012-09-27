#ifndef OVERLAYSETTINGS_H
#define OVERLAYSETTINGS_H

#include <QDialog>

struct OverlaySettings {
	enum OverlayType { Absolute, Relative };
	OverlayType type;

	union { bool auto_min; bool auto_mean; };
	union { bool auto_max; bool auto_range; };

	union { double min; double mean; };
	union { double max; double range; };

	OverlaySettings() {
		type = Absolute;
		auto_min = true;
		auto_max = true;
	}
};

namespace Ui {
class OverlaySettingsDlg;
}

class OverlaySettingsDlg : public QDialog {
public:
	OverlaySettingsDlg(QWidget *parent=0);
	OverlaySettingsDlg(const OverlaySettings &settings, QWidget *parent=0);

	virtual void accept();

	const OverlaySettings& overlaySettings() { return s; }
	void setOverlaySettings(const OverlaySettings &settings);

private:
	void init();
	void setDialogState(); // set in controls
	void getDialogState(); // store in 's'

	OverlaySettings s;
	Ui::OverlaySettingsDlg *ui;
};

#endif // OVERLAYSETTINGS_H
