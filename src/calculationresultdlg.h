#include <QDialog>
#include "model/model.h"

namespace Ui {
	class CalculationResultDlg;
}


class QTableWidgetItem;
class CalculationResultDlg : public QDialog
{
	Q_OBJECT

public:
	CalculationResultDlg(QWidget *parent, const Model &m);

protected slots:
	void on_vesselTable_itemDoubleClicked(QTableWidgetItem *item);

private:
	Ui::CalculationResultDlg *ui;

signals:
	void highlightVessel(int type, int gen, int idx);
};
