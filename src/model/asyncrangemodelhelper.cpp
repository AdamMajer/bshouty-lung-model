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

#include <QtConcurrentRun>
#include <QFuture>
#include <QThread>
#include <QTimerEvent>
#include "asyncrangemodelhelper.h"
#include <stdexcept>

class AsyncRangeModelHelper_p : public QThread
{
public:
	AsyncRangeModelHelper_p(QList<QPair<Model::DataType, Range> > dr,
	                        const Model &bm,
	                        QObject *parent)
	        :QThread(parent), data_ranges(dr), base_model(bm)
	{
		total_models = 1;
		completed_models = 0;
		op_model = 0;
	}

	/* Thread safe */
	void setAbort() {
		abort_flag = 1;

		if (!op.isFinished() && op_model != 0) {
			QMutexLocker lock(&op_model_locker);
			op_model->setAbort();
		}
	}

	/* Thread safe */
	int completedAmount() {
		op_model_locker.lock();
		int model_progress = op.isFinished() ? 0 : op_model->progress();
		op_model_locker.unlock();

		int n_models = total_models;
		int comp = completed_models;

		// qDebug() << n_models;
		return 10000*comp/n_models + model_progress/n_models;
	}

	ModelCalcList getResults() const {
		return results;
	}

protected:
	virtual void run() {
		abort_flag = 0;

		// Allocate memory
		Model *run_clone = base_model.clone();

		try {
			total_models = 1;
			for (QList<QPair<Model::DataType, Range> >::const_iterator i=data_ranges.begin(); i!=data_ranges.end(); ++i)
				total_models = total_models * i->second.sequenceCount();
			completed_models = 0;
			recursiveDataSet(data_ranges, *run_clone);
		}
		catch(std::bad_alloc *error) {
			// cleanup memory
			while (!results.isEmpty()) {
				delete results.front().second;
				results.pop_front();
			}
		}
		delete run_clone;

		// Calculate
		for (ModelCalcList::iterator i=results.begin(); i!=results.end(); ++i) {
			op_model_locker.lock();
			op_model = i->second;
			op = QtConcurrent::run(op_model, &Model::calc, 50);
			op_model_locker.unlock();

			if (abort_flag)
				op_model->setAbort();
			op.waitForFinished();

			i->first = op.result();
			++completed_models; // inc completed models

			if (abort_flag)
				break;
		}
	}

	void recursiveDataSet(QList<QPair<Model::DataType, Range> > data,
	                      Model &model) {
		if (abort_flag)
			return;

		if (data.size() > 0) {
			QPair<Model::DataType, Range> range = data.takeFirst();
			QList<double> value_list = range.second.sequence();

			foreach (double value, value_list) {
				if (abort_flag)
					return;

				model.setData(range.first, value);
				recursiveDataSet(data, model);
			}
		}
		else {
			op_model_locker.lock();
			op_model = model.clone();
			op_model_locker.unlock();
			results.append(QPair<int,Model*>(0, op_model));
		}
	}


	QList<QPair<Model::DataType, Range> > data_ranges;
	const Model &base_model;
	ModelCalcList results;

	QFuture<int> op;
	QMutex op_model_locker;
	Model *op_model;

	int completed_models, total_models;
	volatile int abort_flag;
};




AsyncRangeModelHelper::AsyncRangeModelHelper(const Model &bm, QObject *parent)
        : QObject(parent), base_model(bm)
{
	p = 0;
	timer_id = -1;
}

AsyncRangeModelHelper::~AsyncRangeModelHelper()
{
	while (!results.isEmpty())
		delete results.takeFirst().second;

	cleanupHelper();
}

void AsyncRangeModelHelper::setRangeData(Model::DataType type, const Range &range)
{
	// slow check to verify we haven't had this data type included already
	// if type already set, it is replaced in the list
	QPair<Model::DataType, Range> data(type, range);

	for (QList<QPair<Model::DataType, Range> >::iterator i=data_ranges.begin();
	     i!=data_ranges.end(); ++i) {
		if ((*i).first == type) {
			*i = data;
			return;
		}
	}

	data_ranges << data;
}

void AsyncRangeModelHelper::setRangeData(QList<QPair<Model::DataType, Range> > ranges)
{
	for(QList<QPair<Model::DataType,Range> >::const_iterator i=ranges.begin();
	    i!=ranges.end();
	    ++i)
		setRangeData((*i).first, (*i).second);
}

ModelCalcList AsyncRangeModelHelper::output()
{
	if (p && p->isFinished()) {
		results = p->getResults();

		if (timer_id >= 0)
			killTimer(timer_id);
		timer_id = -1;

		delete p;
		p = 0;
	}
	return results;
}

bool AsyncRangeModelHelper::beginCalculation()
{
	// clear results, if any
	while (!results.isEmpty())
		delete results.takeFirst().second;

	cleanupHelper();
	p = new AsyncRangeModelHelper_p(data_ranges, base_model, parent());
	connect(p, SIGNAL(finished()), SLOT(calcThreadDone()));
	p->start();
	startTimer(2000);
	return true;
}

bool AsyncRangeModelHelper::isCalculationCompleted() const
{
	return p ? p->isFinished() : false;
}

bool AsyncRangeModelHelper::waitForCompletion() const
{
	return p ? p->wait() : true;
}

void AsyncRangeModelHelper::terminateCalculation()
{
	if (!p)
		return;

	p->setAbort();
	return;
}

void AsyncRangeModelHelper::timerEvent(QTimerEvent *e)
{
	if (p == 0)
		return;

	// qDebug() << "completed amount" << p->completedAmount();
	emit completionAmount(p->completedAmount());
	e->accept();
}

void AsyncRangeModelHelper::calcThreadDone()
{
	killTimer(timer_id);
	p->wait();
	emit calculationComplete();
}

void AsyncRangeModelHelper::cleanupHelper()
{
	if (p) {
		if (p->isRunning()) {
			p->setAbort();
			p->wait();
		}

		if (timer_id >= 0)
			killTimer(timer_id);
		timer_id = -1;

		ModelCalcList r = p->getResults();
		while (r.isEmpty())
			delete r.takeFirst().second;
		delete p;
		p = 0;
	}
}
