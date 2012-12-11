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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <map>
#include <QGraphicsScene>
#include <QHash>
#include <QMainWindow>
#include "diseasemodel.h"
#include "model/asyncrangemodelhelper.h"
#include "model/disease.h"
#include "model/model.h"
#include "vesseldlg.h"

namespace Ui {
	class MainWindow;
}
class QActionGroup;
class QButtonGroup;
class QCheckBox;
class ModelScene;
class MultiModelOutput;
class QPropertyAnimation;

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	enum ModelCalculationType { DiseaseProgressCalculation, PAPmCalculation };
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

	void load(const QString &filename);
	void save(const QString &filename);

public slots:
	void updateResults();

	// void on_transducerPosition_activated( int position );
	void on_actionCalculate_triggered();
	void on_actionResetModel_triggered();

	void on_actionLoad_triggered();
	void on_actionSave_triggered();
	void on_actionSaveAs_triggered();

	void on_actionZoomIn_triggered();
	void on_actionZoomOut_triggered();
	void on_actionZoomReset_triggered();

	void on_actionConfigureDisease_triggered();
	void on_actionOpenCL_triggered();

	void on_actionConvergenceSummary_triggered();
	void on_actionCalibrateModel_triggered();

	void on_actionOverview_triggered();
	void on_actionAbout_triggered();
	void on_actionAboutQt_triggered();

	void on_actionOverlayMap_triggered();
	void on_actionOverlaySettings_triggered();

	void on_actionFixedFlow_triggered();

	void on_gender_currentIndexChanged(int idx);
	void setCalculationType(QAction*);
	void adjustVisibleModelInputs(ModelCalculationType calc_type);
	void setOverlayType(QAction*);
	void setIntegralType(QAction*);

	void modifyArtery(int gen, int idx);
	void modifyVein(int gen, int idx);
	void modifyCap(int idx);

	void transducerPositionChanged(Model::Transducer tr);

protected:
	virtual void changeEvent(QEvent *e);
	virtual void resizeEvent(QResizeEvent *);
	virtual void showEvent(QShowEvent *);

	void scaleLungViewToWindow();
	void updateInputsOutputs();
	QList<QPair<Model::DataType, Range> > fetchModelInputs() const;

	void setupNewModelScene();
	void updateDiseaseMenu();
	// set window title based on save_filename
	void updateTitleFilename();
	bool isVesselPtpReadOnly() const;

	// animation general setup function
	void animateLabelVisibility(QWidget *label, bool fVisible);

protected slots:
	void calculationCompleted();
	void modelSelected(Model *model, int n_iters);
	void multiModelWindowClosed();

	void diseaseActionTriggered(bool);
	void diseaseGroupBoxTriggered(int id);

	void calculationTypeButtonGroupClick(int id);
	void allocateNewModel(bool propagate_diseases_to_new_model=true);

	void patHtWtChanged();
	void vesselPtpDependenciesChanged();
	void globalVolumesChanged();
	void vesselDimsChanged();

	// label animations
	void animatePatHtLabel(bool fVisible);
	void animatePatWtLabel(bool fVisible);
	void animateLungHtLabel(bool fVisible);
	void animatePplModLabel(bool fVisible);
	void animatePalModLabel(bool fVisible);
	void animatePVParams(bool fVisible);
	void animationFinished();

private:
	Ui::MainWindow *ui;

	Model *baseline, *model;
	ModelScene *scene;

	AsyncRangeModelHelper *calc_thread;
	QString save_filename;
	MultiModelOutput *mres;

	std::map<QAction*,Disease> disease_actions;
	QButtonGroup *disease_box_group;
	DiseaseModel *disease_model;

	std::list<QPropertyAnimation*> animations;
};

#endif // MAINWINDOW_H
