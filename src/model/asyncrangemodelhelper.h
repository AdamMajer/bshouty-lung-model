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

#ifndef ASYNCRANGEHELPER_H
#define ASYNCRANGEHELPER_H

#include <QList>
#include <QPair>
#include <QObject>
#include "model/model.h"
#include "range.h"

/* The primary purpose of this class is to facilitate asynchronous, interruptable
 * computation of model values that may include range data.
 *
 * It can also be used to calculate single model without range data
 * where the computation is interruptible.
 */

class AsyncRangeModelHelper_p;
class AsyncRangeModelHelper : public QObject
{
	Q_OBJECT

public:
	AsyncRangeModelHelper(const Model &base_model, QObject *parent=0);
	~AsyncRangeModelHelper();

	/* Not thread safe, must not be called if calculation is running */
	void setRangeData(Model::DataType type, const Range &range);
	void setRangeData(QList<QPair<Model::DataType, Range> > ranges);
	ModelCalcList output();

	/* Thread safe */
	bool beginCalculation();
	bool isCalculationCompleted() const;
	bool waitForCompletion() const;

public slots:
	void terminateCalculation();

protected:
	virtual void timerEvent(QTimerEvent *);

protected slots:
	void calcThreadDone();

private:
	void cleanupHelper();

	QList<QPair<Model::DataType, Range> > data_ranges;
	ModelCalcList results;
	const Model &base_model;

	AsyncRangeModelHelper_p *p;
	int timer_id;

	QString label;

signals:
	void calculationComplete();
	void completionAmount(int value); // value is [0,10000]
	void updateLabel(QString label);
};

#endif // ASYNCRANGEHELPER_H
