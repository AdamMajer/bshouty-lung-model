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

#include "about.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "calculationresultdlg.h"
#include "calibratedlg.h"
#include "capillary.h"
#include "common.h"
#include "dbsettings.h"
#include "diseaselistdlg.h"
#include "diseaseparamdelegate.h"
#include "modelscene.h"
#include "multimodeloutput.h"
#include "model/compromisemodel.h"
#include "opencldlg.h"
#include "opencl.h"
#include "overlaymapwidget.h"
#include "specialgeometricflowwidget.h"

#include "model/integrationhelper/cpuhelper.h"

#include <QActionGroup>
#include <QCheckBox>
#include <QFileDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QPropertyAnimation>
#include <QSqlDatabase>
#include <QTime>
#include <QTimer>

#include <QDebug>

#include <algorithm>

#ifdef Q_OS_WIN
# ifdef WINVER
#  undef WINVER
# endif

#define WINVER 0x0700
#include <windows.h>
#include <winbase.h>
#include <Shellapi.h>
#endif

#include <errno.h>

#ifdef Q_OS_UNIX
# include <unistd.h>
# include <sys/sysinfo.h>
#endif


static const QLatin1String db_name("modeldb");


MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
{
	calc_thread = 0;
	mres = 0;

	baseline = new Model(Model::Middle, Model::SegmentedVesselFlow);
	model = baseline->clone();
	ui = new Ui::MainWindow;
	ui->setupUi(this);

	disease_model = new DiseaseModel(this);
	ui->diseaseView->setModel(disease_model);
	ui->diseaseView->setItemDelegate(new DiseaseParamDelegate(ui->diseaseView));

	QActionGroup *calc_type = new QActionGroup(this);
	calc_type->addAction(ui->actionDiseaseProgression);
	calc_type->addAction(ui->actionPAP);
	calc_type->setExclusive(true);
	connect(calc_type, SIGNAL(triggered(QAction*)),
	        SLOT(setCalculationType(QAction*)));

	QActionGroup *overlay_type = new QActionGroup(this);
	overlay_type->addAction(ui->actionNoOverlay);
	overlay_type->addAction(ui->actionFlowOverlay);
	overlay_type->addAction(ui->actionVolumeOverlay);
	overlay_type->setExclusive(true);
	connect(overlay_type, SIGNAL(triggered(QAction*)),
	        SLOT(setOverlayType(QAction*)));

	QActionGroup *integral_type = new QActionGroup(this);
	integral_type->addAction(ui->actionSegmentedVessels);
	integral_type->addAction(ui->actionRigidVessels);
	integral_type->addAction(ui->actionNavierStokes);
	integral_type->setExclusive(true);
	connect(integral_type, SIGNAL(triggered(QAction*)),
	        SLOT(setIntegralType(QAction*)));

	QButtonGroup *bg = new QButtonGroup(this);
	bg = new QButtonGroup(this);
	bg->addButton(ui->calculationTypeDiseaseParams, DiseaseProgressCalculation);
	bg->addButton(ui->calculationTypePAPm, PAPmCalculation);
	bg->setExclusive(true);
	connect(bg, SIGNAL(buttonClicked(int)),
	        SLOT(calculationTypeButtonGroupClick(int)));

	// link the group boxes with actions, avoiding recursive loops
	typedef QPair<QAction*,QRadioButton*> linked_control;
	QList<linked_control> linked_controls;
	linked_controls << linked_control(ui->actionDiseaseProgression, ui->calculationTypeDiseaseParams)
	                << linked_control(ui->actionPAP, ui->calculationTypePAPm);
	foreach (const linked_control &c, linked_controls)
		connect(c.first, SIGNAL(toggled(bool)), c.second, SLOT(setChecked(bool)));
	scene = 0;
	setupNewModelScene();

	connect(ui->recalc, SIGNAL(clicked()), SLOT(updateResults()));
	updateInputsOutputs();

	/* dock widget visibility <=> window/input item checked link */
	// actiongroup is required to prevent checkmark from "toggling"
	QActionGroup *group_of_one = new QActionGroup(this);
	group_of_one->setExclusive(true);

	ui->actionInputOutputPane->setCheckable(true);
	ui->actionInputOutputPane->setActionGroup(group_of_one);
	connect(ui->dockWidget, SIGNAL(visibilityChanged(bool)),
	        ui->actionInputOutputPane, SLOT(setChecked(bool)));
	connect(ui->actionInputOutputPane, SIGNAL(triggered()),
	        ui->dockWidget, SLOT(show()));

	adjustVisibleModelInputs(PAPmCalculation);

	// Set valid range data
	ui->patHt->setProperty(range_property, "50 to 250;1");
	ui->patWt->setProperty(range_property, "1 to 200;0.1");
	ui->lungHt->setProperty(range_property, "1 to 50;0.1");
	ui->lungHt_left->setProperty(range_property, "1 to 50;0.1");
	ui->lungHt_right->setProperty(range_property, "1 to 50;0.1");
	ui->Vm->setProperty(range_property, "0 to 95;1");
	ui->Vm_left->setProperty(range_property, "0 to 95;1");
	ui->Vm_right->setProperty(range_property, "0 to 95;1");
	ui->Vrv->setProperty(range_property, "5 to 95;1");
	ui->Vrv_left->setProperty(range_property, "5 to 95;1");
	ui->Vrv_right->setProperty(range_property, "5 to 95;1");
	ui->Vfrc->setProperty(range_property, "5 to 95;1");
	ui->Vfrc_left->setProperty(range_property, "5 to 95;1");
	ui->Vfrc_right->setProperty(range_property, "5 to 95;1");
	ui->Vtlc->setProperty(range_property, "5 to 300;1");
	ui->Vtlc_left->setProperty(range_property, "5 to 300;1");
	ui->Vtlc_right->setProperty(range_property, "5 to 300;1");

	ui->Pal->setProperty(range_property, "0 to 100;0.1");
	ui->Ppl->setProperty(range_property, "-50 to 50;0.1");
	ui->LAP->setProperty(range_property, "-10 to 50;0.1");
	ui->CO->setProperty(range_property, "0.1 to 25;0.1");
	ui->PAPm->setProperty(range_property, "0 to 150;0.1");

	ui->Hct->setProperty(range_property, "5 to 95;0.1");
	ui->PA_EVL->setProperty(range_property, "0.5 to 20;0.1");
	ui->PA_Diameter->setProperty(range_property, "0.5 to 20;0.05");
	ui->PV_EVL->setProperty(range_property, "0.5 to 20;0.1");
	ui->PV_Diameter->setProperty(range_property, "0.5 to 20;0.05");

	ui->diseaseView->hide();
	ui->diseaseViewLabel->hide();

	connect(disease_model, SIGNAL(emptiedModel(bool)),
	        ui->diseaseView, SLOT(setHidden(bool)));
	connect(disease_model, SIGNAL(emptiedModel(bool)),
	        ui->diseaseViewLabel, SLOT(setHidden(bool)));
	connect(ui->diseaseView, SIGNAL(doubleClicked(QModelIndex)),
	        disease_model, SLOT(setCompromiseModelSlewParameter(QModelIndex)));

	connect(disease_model, SIGNAL(diseaseAdded(QModelIndex)),
	        ui->diseaseView, SLOT(expand(QModelIndex)));
	connect(disease_model, SIGNAL(modelReset()), ui->diseaseView, SLOT(expandAll()));

	ui->PatHtModLabel->setVisible(false);
	ui->PatWtModLabel->setVisible(false);
	ui->lungHtModLabel->setVisible(false);
	ui->PalModLabel->setVisible(false);
	ui->PplModLabel->setVisible(false);
	ui->pv_params_label->setVisible(false);

	connect(ui->patHt, SIGNAL(focusChange(bool)), SLOT(animatePatHtLabel(bool)));
	connect(ui->patWt, SIGNAL(focusChange(bool)), SLOT(animatePatWtLabel(bool)));
	connect(ui->lungHt, SIGNAL(focusChange(bool)), SLOT(animateLungHtLabel(bool)));
	connect(ui->Ppl, SIGNAL(focusChange(bool)), SLOT(animatePplModLabel(bool)));
	connect(ui->Pal, SIGNAL(focusChange(bool)), SLOT(animatePalModLabel(bool)));
	connect(ui->PA_Diameter, SIGNAL(focusChange(bool)), SLOT(animatePVParams(bool)));
	connect(ui->PA_EVL, SIGNAL(focusChange(bool)), SLOT(animatePVParams(bool)));
	connect(ui->PV_Diameter, SIGNAL(focusChange(bool)), SLOT(animatePVParams(bool)));
	connect(ui->PV_EVL, SIGNAL(focusChange(bool)), SLOT(animatePVParams(bool)));

	connect(ui->patHt, SIGNAL(textChanged(QString)), SLOT(patHtWtChanged()));
	connect(ui->patWt, SIGNAL(textChanged(QString)), SLOT(patHtWtChanged()));
	connect(ui->lungHt, SIGNAL(textChanged(QString)), SLOT(vesselPtpDependenciesChanged()));
	connect(ui->lungHt_left, SIGNAL(textChanged(QString)), SLOT(vesselPtpDependenciesChanged()));
	connect(ui->lungHt_right, SIGNAL(textChanged(QString)), SLOT(vesselPtpDependenciesChanged()));
	connect(ui->Pal, SIGNAL(textChanged(QString)), SLOT(vesselPtpDependenciesChanged()));
	connect(ui->Ppl, SIGNAL(textChanged(QString)), SLOT(vesselPtpDependenciesChanged()));

	connect(ui->Vm, SIGNAL(textEdited(QString)), SLOT(globalVolumesChanged()));
	connect(ui->Vm_left, SIGNAL(textEdited(QString)), SLOT(globalVolumesChanged()));
	connect(ui->Vm_right, SIGNAL(textEdited(QString)), SLOT(globalVolumesChanged()));
	connect(ui->Vrv, SIGNAL(textEdited(QString)), SLOT(globalVolumesChanged()));
	connect(ui->Vrv_left, SIGNAL(textEdited(QString)), SLOT(globalVolumesChanged()));
	connect(ui->Vrv_right, SIGNAL(textEdited(QString)), SLOT(globalVolumesChanged()));
	connect(ui->Vfrc, SIGNAL(textEdited(QString)), SLOT(globalVolumesChanged()));
	connect(ui->Vfrc_left, SIGNAL(textEdited(QString)), SLOT(globalVolumesChanged()));
	connect(ui->Vfrc_right, SIGNAL(textEdited(QString)), SLOT(globalVolumesChanged()));
	connect(ui->Vtlc, SIGNAL(textEdited(QString)), SLOT(globalVolumesChanged()));
	connect(ui->Vtlc_left, SIGNAL(textEdited(QString)), SLOT(globalVolumesChanged()));
	connect(ui->Vtlc_right, SIGNAL(textEdited(QString)), SLOT(globalVolumesChanged()));

	connect(ui->PA_Diameter, SIGNAL(textChanged(QString)), SLOT(vesselDimsChanged()));
	connect(ui->PA_EVL, SIGNAL(textChanged(QString)), SLOT(vesselDimsChanged()));
	connect(ui->PV_Diameter, SIGNAL(textChanged(QString)), SLOT(vesselDimsChanged()));
	connect(ui->PV_EVL, SIGNAL(textChanged(QString)), SLOT(vesselDimsChanged()));

	QButtonGroup *dissimilar_lungs_group = new QButtonGroup(this);
	dissimilar_lungs_group->addButton(ui->similarLungsRadioButton, 0);
	dissimilar_lungs_group->addButton(ui->dissimilarLungsRadioButton, 1);
	connect(dissimilar_lungs_group, SIGNAL(buttonClicked(int)), SLOT(dissimilarLungSelection(int)));

	disease_box_group = new QButtonGroup(this);
	disease_box_group->setExclusive(false);
	connect(disease_box_group, SIGNAL(buttonClicked(int)), SLOT(diseaseGroupBoxTriggered(int)));
	updateDiseaseMenu();

	// restore window geometry
	QRect new_rect = DbSettings::value("/settings/main_window_size", QRect()).toRect();
	if (new_rect.isValid())
		setGeometry(new_rect);

#if (QT_POINTER_SIZE != 8)
	ui->actionOpenCL->setEnabled(false);
#else
	ui->actionOpenCL->setEnabled(cl->isAvailable());
#endif

	/* Add shortcut texts to menu items. These cannot be readily added
	 * in Designer because they are handled elsewhere, not via shortcuts.
	 */
	ui->actionZoomIn->setText(ui->actionZoomIn->text() + "\t+");
	ui->actionZoomOut->setText(ui->actionZoomOut->text() + "\t-");
}

void MainWindow::load(const QString &filename)
{
	{
		QProgressDialog dlg(this);
		dlg.setRange(0, 2000);
		dlg.setModal(true);
		dlg.setLabelText("Loading...");
		dlg.setCancelButton(0);
		dlg.setAutoReset(false);
		dlg.show();

		QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", db_name);
		db.setDatabaseName(filename);

		try {
			// TODO: move to exception based error detection only
			if (db.open() && baseline->load(db, 0, &dlg)) {
				if (!model->load(db, 1, &dlg))
					*model = *baseline;
				save_filename = filename;
				setupNewModelScene();
				updateInputsOutputs();
				disease_model->load(db);
			}
			else
				throw std::runtime_error("Cannot load file");
		}
		catch (std::runtime_error error) {
			qDebug() << error.what();
			QMessageBox::critical(this, "Error loading file",
			                      QString("An error occured trying to load \n%1.").arg(filename));
		}

		db.close();
	}
	QSqlDatabase::removeDatabase(db_name);
}

void MainWindow::save(const QString &filename)
{
	const QLatin1String ext(".lungmodel");
	bool save_error = false;
	QString fn = filename;

	if (!fn.endsWith(ext))
		fn.append(ext);

	// make backup file
	QString bk_filename = fn + ".bak";
	QFile::rename(fn, bk_filename);
	QFile::remove(fn);

	// save to file
	{
		QProgressDialog dlg(this);
		dlg.setRange(0, 2000);
		dlg.setModal(true);
		dlg.setLabelText("...");
		dlg.setCancelButton(0);
		dlg.setAutoReset(false);
		dlg.show();

		DiseaseList diseases = disease_model->diseases();
		baseline->clearDiseases();
		for (DiseaseList::const_iterator i=diseases.begin(); i!=diseases.end(); ++i)
			baseline->addDisease(*i);

		// in block so we are certain to destroy the `db` object prior to DB removal
		QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", db_name);
		db.setDatabaseName(fn);
		try {
			// TODO: move to exception based error detection only
			if (db.open() &&
			                baseline->save(db, 0, &dlg) &&
			                model->save(db, 1, &dlg)) {
				disease_model->save(db);
				save_filename = fn;
			}
			else
				save_error = true;
		}
		catch (std::runtime_error error) {
			qDebug() << error.what();
			save_error = true;
		}

		if (save_error)
			QMessageBox::critical(this, "Error saving file",
			                      QString("An error occurred trying to save to \n%1").arg(fn));

		db.close();
	}
	QSqlDatabase::removeDatabase(db_name);

	if (save_error) {
		// restore backup
		QFile::remove(fn);
		QFile::rename(bk_filename, fn);
	}
	else {
		// remove backup
		QFile::remove(bk_filename);
	}

	DbSettings::setValue("/settings/directory", QFileInfo(filename).absolutePath());
}

MainWindow::~MainWindow()
{
	// save geometry
	DbSettings::setValue("/settings/main_window_size", geometry());

	if (calc_thread) {
		if (!calc_thread->isCalculationCompleted()) {
			calc_thread->terminateCalculation();
			calc_thread->waitForCompletion();
		}

		calc_thread->waitForCompletion();
		delete calc_thread;
	}
	if (mres)
		delete mres;
	delete ui;
	delete scene;

	delete model;
	delete baseline;
}

void MainWindow::updateResults()
{

	// setup models for calculation
	if (calc_thread) {
		if (calc_thread->isCalculationCompleted())
			delete calc_thread;
		else {
			calc_thread->terminateCalculation();
			calc_thread->waitForCompletion();
			delete calc_thread;
		}
	}

	QList<QPair<Model::DataType, Range> > data_ranges = fetchModelInputs();
	unsigned long long n_models = 1;
	unsigned long long total_memory = 0xFFFFFFFFFFFFFFFFULL;
	unsigned long long total_ram = total_memory;
	const unsigned long long memory_per_model =
	                (baseline->numArteries() + baseline->numVeins())*sizeof(Vessel) +
	                baseline->numCapillaries()*sizeof(Capillary); // 19 elemnts+capilaries

	for(QList<QPair<Model::DataType, Range> >::const_iterator i=data_ranges.begin();
	    i!=data_ranges.end();
	    ++i) {
		if (i->second.sequenceCount() < 1) {
			qDebug("Invalid data input");
			return;
		}

		n_models *= i->second.sequenceCount();
	}

	DiseaseList diseases = disease_model->diseases();
	const std::vector<std::vector<Range> > disease_param_ranges = disease_model->diseaseParamRanges();
	for (unsigned disease_no=0; disease_no<disease_param_ranges.size(); ++disease_no) {
		const std::vector<Range> &param_ranges = disease_param_ranges.at(disease_no);

		for (unsigned param_no=0; param_no<param_ranges.size(); ++param_no) {
			Range range = param_ranges.at(param_no);

			if (range.sequenceCount() > 1) {
				Model::DataType type = Model::diseaseHybridType(disease_no, param_no);
				data_ranges.push_back(QPair<Model::DataType, Range>(type, range));
			}

			n_models *= range.sequenceCount();
		}
	}

#if defined(Q_OS_WIN32)
	MEMORYSTATUSEX mem;

	mem.dwLength = sizeof(mem);
	if (GlobalMemoryStatusEx(&mem)) {
		total_ram = mem.ullTotalPhys;
		total_memory = total_ram + mem.ullAvailPageFile;
	}
#elif defined(Q_OS_LINUX)
	struct sysinfo si;
	if (sysinfo(&si) == 0) {
		total_ram = si.totalram;
		total_memory = si.totalswap + total_ram;
	}
#else
#warning "FIXME: Memory size not available on this platform."
#endif

	if (n_models > 1000000ULL) {
		QMessageBox::critical(this, "Too many models",
		                      "You have asked for more than 1 MILLION models to be calculated.\n"
		                      "This is probably not what you wanted.\n\n"
		                      "Calculation aborted.");
		return;
	}

	if (memory_per_model*n_models > total_memory) {
		QString oom_msg(QLatin1String("Current settings will require %1 MB of RAM while only %2 MB available"));
		QMessageBox::critical(this, "Out of Memory", oom_msg.arg((memory_per_model*n_models)>>20).arg(total_memory>>20));
		return;
	}

	if (memory_per_model*n_models > total_ram*3/4) {
		QString oom_msg(QLatin1String("Current settings will require %1 MB of RAM while you have %2 MB installed.\n"
		                              "Continuing may cause excessive slowdown of your system.\n\n"
		                              "Do you wish to continue?"));
		int ret = QMessageBox::question(this, "Out of Memory",
		                                oom_msg.arg((memory_per_model*n_models)>>20).arg(total_ram>>20),
		                                QMessageBox::Yes | QMessageBox::No,
		                                QMessageBox::No);

		if (ret != QMessageBox::Yes)
			return;
	}

	baseline->clearDiseases();
	for (DiseaseList::const_iterator i=diseases.begin(); i!=diseases.end(); ++i)
		baseline->addDisease(*i);

	if (dynamic_cast<CompromiseModel*>(baseline) != 0) {
		if (diseases.empty()) {
			QMessageBox::critical(this, "No diseases selected",
			                      "You wish to calculate disease severity based on a given PAPm, \n"
			                      "yet you have not specified any diseases to be used as contraints.");
			return;
		}

		if (!disease_model->hasSlewParameter()) {
			QMessageBox::critical(this, "No slew parameter selected",
			                      "Please select a parameter that will be adjusted for a given \n"
			                      "target PAPm by double-clicking on it in the disease list.");
			return;
		}

		CompromiseModel *cm = static_cast<CompromiseModel*>(baseline);
		int param_no;
		const Disease slew_disease = disease_model->slewParameter(param_no);

		cm->setCalculatedParameter(slew_disease, param_no);
		cm->setData(Model::PAP_value, ui->PAPm->text().toDouble());
	}

	ui->recalc->setEnabled(false);

	*model = *baseline;
	calc_thread = new AsyncRangeModelHelper(*model, this);
	calc_thread->setRangeData(data_ranges);

	QProgressDialog *progress = new QProgressDialog(this);
	progress->setRange(0, 10000);
	progress->setAttribute(Qt::WA_DeleteOnClose);
	progress->setCancelButtonText("Cancel");
	progress->setLabelText("Calculating ...");
	progress->show();

	connect(progress, SIGNAL(canceled()), calc_thread, SLOT(terminateCalculation()));
	connect(calc_thread, SIGNAL(completionAmount(int)), progress, SLOT(setValue(int)));
	connect(calc_thread, SIGNAL(calculationComplete()), progress, SLOT(close()));
	connect(calc_thread, SIGNAL(calculationComplete()), SLOT(calculationCompleted()));
	connect(calc_thread, SIGNAL(updateLabel(QString)), progress, SLOT(setLabelText(const QString&)));
	calc_thread->beginCalculation();
}

void MainWindow::on_actionCalculate_triggered()
{
	updateResults();
}

void MainWindow::on_actionResetModel_triggered()
{
	allocateNewModel(false);
}

void MainWindow::on_actionLoad_triggered()
{
	QString fn;

	fn = save_filename;
	if (fn.isEmpty())
		fn = DbSettings::value("/settings/directory", QDir::homePath()).toString();

	fn = QFileDialog::getOpenFileName(this,
	                                  "Load Model",
	                                  fn,
	                                  tr("Lung Model (*.lungmodel)"));
	if (fn.isEmpty())
		return;

	load(fn);
	updateTitleFilename();
}

void MainWindow::on_actionSave_triggered()
{
	if (!save_filename.isEmpty())
		save(save_filename);
	else
		on_actionSaveAs_triggered();

	updateTitleFilename();
}

void MainWindow::on_actionSaveAs_triggered()
{
	QString fn;

	QString cur = save_filename;
	if (cur.isEmpty())
		cur = DbSettings::value("/settings/directory", QDir::homePath()).toString();

	fn = QFileDialog::getSaveFileName(this,
	                                  "Save Model As...",
	                                  cur,
	                                  tr("Lung Model (*.lungmodel)"));
	if (!fn.isEmpty())
		save(fn);

	updateTitleFilename();
}

void MainWindow::on_actionZoomIn_triggered()
{
	ui->view->zoomIn();
}

void MainWindow::on_actionZoomOut_triggered()
{
	ui->view->zoomOut();
}

void MainWindow::on_actionZoomReset_triggered()
{
	ui->view->resetZoom();
}

void MainWindow::on_actionConfigureDisease_triggered()
{
	DiseaseListDlg dlg(this);
	dlg.exec();

	updateDiseaseMenu();
}

void MainWindow::on_actionOpenCL_triggered()
{
	OpenCLDlg dlg(this);
	dlg.exec();
}

void MainWindow::on_actionConvergenceSummary_triggered()
{
	CalculationResultDlg *dlg = new CalculationResultDlg(this, *model);
	connect(dlg, SIGNAL(highlightVessel(int,int,int)),
	        ui->view, SLOT(zoomToVessel(int,int,int)));
	connect(dlg, SIGNAL(finished(int)), dlg, SLOT(deleteLater()));
	dlg->show();
}

void MainWindow::on_actionCalibrateModel_triggered()
{
	CalibrateDlg dlg(this);
	dlg.exec();

	allocateNewModel();
}

void MainWindow::on_actionOverview_triggered()
{
	QString path("file://" + QCoreApplication::applicationDirPath() + "/data/Overview.html");

#if defined(Q_OS_WIN)
	// qDebug() << QProcess::execute(path) << " " << path;

	ShellExecute(winId(), NULL, path.toStdWString().c_str(), NULL, NULL, SW_SHOW);
#elif defined(Q_OS_LINUX)
	if (fork() == 0) {
		const char *c_path = path.toLocal8Bit().constData();
		execl("/usr/bin/x-www-browser",
		      "/usr/bin/x-www-browser",
		      c_path,
		      (void*)0);

		qDebug() << errno;
		perror(0);
		exit(0);
	}
#else
#error Fix for other platforms
#endif
}

void MainWindow::on_actionAbout_triggered()
{
	About dlg(this);
	dlg.exec();
}

void MainWindow::on_actionAboutQt_triggered()
{
	QApplication::aboutQt();
}

void MainWindow::on_actionOverlayMap_triggered()
{
	QDialog dlg(this);
	QGridLayout *layout = new QGridLayout(&dlg);
	OverlayMapWidget *widget = new OverlayMapWidget(ui->view->overlayMap(),
	                                                ui->view->overlaySettings(),
	                                                &dlg);
	QSizePolicy expanding_policy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	expanding_policy.setHorizontalStretch(10);
	expanding_policy.setVerticalStretch(10);
	widget->setSizePolicy(expanding_policy);
	widget->setMinimumSize(450,450);
	widget->setStyleSheet("background: white");

	QCheckBox *visible_check = new QCheckBox("Show Grid", &dlg);
	visible_check->setChecked(false);

	layout->addWidget(new QLabel("<center>Arteries", &dlg), 0, 1);
	layout->addWidget(new QLabel("<center>Veins", &dlg), 0, 2);
	layout->addWidget(new QLabel("Lt", &dlg), 1, 0);
	layout->addWidget(new QLabel("Rt", &dlg), 2, 0);

	layout->addWidget(widget, 1, 1, 2, 2);
	layout->addWidget(visible_check, 3, 0, 1, 3);

	connect(visible_check, SIGNAL(toggled(bool)),
	        widget, SLOT(setGrid(bool)));

	dlg.setWindowTitle("Overlay Map");
	dlg.exec();
}

void MainWindow::on_actionOverlaySettings_triggered()
{
	OverlaySettingsDlg dlg(ui->view->overlaySettings());
	if (dlg.exec())
		ui->view->setOverlaySettings(dlg.overlaySettings());
}

void MainWindow::on_actionGeometricFlow_triggered()
{
	QDialog dlg(this);
	QGridLayout *layout = new QGridLayout(&dlg);
	SpecialGeometricFlowWidget *widget = new SpecialGeometricFlowWidget(*model,
	                                                            this);
	QSizePolicy expanding_policy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	expanding_policy.setHorizontalStretch(10);
	expanding_policy.setVerticalStretch(10);
	widget->setSizePolicy(expanding_policy);
	widget->setStyleSheet("background: white");

	QCheckBox *visible_check = new QCheckBox("Show Grid", &dlg);
	visible_check->setChecked(false);

	layout->addWidget(new QLabel("<center>Arteries", &dlg), 0, 1);
	layout->addWidget(new QLabel("<center>Veins", &dlg), 0, 2);
	layout->addWidget(new QLabel("Lt", &dlg), 1, 0);
	layout->addWidget(new QLabel("Rt", &dlg), 2, 0);

	layout->addWidget(widget, 1, 1, 2, 2);
	layout->addWidget(visible_check, 3, 0, 1, 3);

	connect(visible_check, SIGNAL(toggled(bool)),
	        widget, SLOT(setGrid(bool)));

	dlg.setWindowTitle("Overlay Map");
	dlg.exec();
}

void MainWindow::on_gender_currentIndexChanged(int idx)
{
	Model::Gender g = idx==0 ? Model::Male : Model::Female;

	if (g == baseline->gender())
		return;

	baseline->setGender(g);
	ui->patWt->setText(doubleToString(baseline->getResult(Model::Pat_Wt_value)));
}

void MainWindow::setCalculationType(QAction*)
{
	allocateNewModel();
}

void MainWindow::adjustVisibleModelInputs(ModelCalculationType calculation_type)
{
	switch(calculation_type) {
	case DiseaseProgressCalculation:
		ui->PAPm->show();
		ui->PAPm_label->show();
		break;
	case PAPmCalculation:
		ui->PAPm->hide();
		ui->PAPm_label->hide();
		break;
	}
}

void MainWindow::setOverlayType(QAction *ac)
{
	if (ac == ui->actionNoOverlay) {
		ui->view->clearOverlay();
		return;
	}

	if (ac == ui->actionFlowOverlay)
		ui->view->setOverlayType(LungView::FlowOverlay);
	else if(ac == ui->actionVolumeOverlay)
		ui->view->setOverlayType(LungView::VolumeOverlay);
}

void MainWindow::setIntegralType(QAction *ac)
{
	if (ac == ui->actionSegmentedVessels)
		baseline->setIntegralType(Model::SegmentedVesselFlow);
	else if (ac == ui->actionRigidVessels)
		baseline->setIntegralType(Model::RigidVesselFlow);
	else if (ac == ui->actionNavierStokes)
		baseline->setIntegralType(Model::NavierStokes);
}

void MainWindow::modifyArtery(int gen, int idx)
{
	std::vector<double> dims;
	CpuIntegrationHelper(model, model->integralType()).integrateWithDimentions(Vessel::Artery, gen, idx, dims);
	Vessel v(baseline->artery(gen, idx));
	VesselDlg dlg(Vessel::Artery, v, model->artery(gen,idx), dims, isVesselPtpReadOnly(), this);
	if (dlg.exec() == QDialog::Accepted)
		baseline->setArtery(gen, idx, v);
}

void MainWindow::modifyVein(int gen, int idx)
{
	std::vector<double> dims;
	CpuIntegrationHelper(model, model->integralType()).integrateWithDimentions(Vessel::Vein, gen, idx, dims);
	Vessel v(baseline->vein(gen, idx));
	VesselDlg dlg(Vessel::Vein, v, model->vein(gen,idx), dims, isVesselPtpReadOnly(), this);
	if (dlg.exec() == QDialog::Accepted)
		baseline->setVein(gen, idx, v);
}

void MainWindow::modifyCap(int idx)
{
	Capillary c(baseline->capillary(idx));
	CapillaryDlg dlg(c, this);
	if (dlg.exec() == QDialog::Accepted)
		baseline->setCapillary(idx, c);
}

void MainWindow::transducerPositionChanged(Model::Transducer tr)
{
	if (baseline->transducerPos() != tr) {
		baseline->setTransducerPos(tr);
		model->setTransducerPos(tr);
		scene->updateModelValues();
	}
}


void MainWindow::changeEvent(QEvent *e)
{
	QMainWindow::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	default:
		break;
	}
}

void MainWindow::resizeEvent(QResizeEvent *e)
{
	QMainWindow::resizeEvent(e);
	scaleLungViewToWindow();
}

void MainWindow::showEvent(QShowEvent *e)
{
	QMainWindow::showEvent(e);
	scaleLungViewToWindow();
}

void MainWindow::scaleLungViewToWindow()
{
	const double scene_height = scene->height();
	const double scene_width = scene->width();
	qreal scale = qMin(ui->view->viewport()->height()/(scene_height*1.05),
	                   ui->view->viewport()->width()/(scene_width*1.05));

	// adjust transform's zoom, if window area is larger than the scene rect.
	QTransform t = ui->view->transform();

	if (t.m11() < scale && t.m22() < scale)
		ui->view->setTransform(t.scale(scale/t.m11(), scale/t.m22()));
}

void MainWindow::updateInputsOutputs()
{
#define GET_MODEL(a, b) (b->setText(doubleToString(model->getResult(a))))

	/* Read input that were set in the model */
	GET_MODEL(Model::Lung_Ht_value, ui->lungHt);
	GET_MODEL(Model::Lung_Ht_L_value, ui->lungHt_left);
	GET_MODEL(Model::Lung_Ht_R_value, ui->lungHt_right);
	GET_MODEL(Model::CO_value, ui->CO);
	GET_MODEL(Model::LAP_value, ui->LAP);
	GET_MODEL(Model::Pal_value, ui->Pal);
	GET_MODEL(Model::Ppl_value, ui->Ppl);
	GET_MODEL(Model::Tlrns_value, ui->Tolerance);
	GET_MODEL(Model::Vm_value, ui->Vm);
	GET_MODEL(Model::Vm_L_value, ui->Vm_left);
	GET_MODEL(Model::Vm_R_value, ui->Vm_right);
	GET_MODEL(Model::Vrv_value, ui->Vrv);
	GET_MODEL(Model::Vrv_L_value, ui->Vrv_left);
	GET_MODEL(Model::Vrv_R_value, ui->Vrv_right);
	GET_MODEL(Model::Vfrc_value, ui->Vfrc);
	GET_MODEL(Model::Vfrc_L_value, ui->Vfrc_left);
	GET_MODEL(Model::Vfrc_R_value, ui->Vfrc_right);
	GET_MODEL(Model::Vtlc_value, ui->Vtlc);
	GET_MODEL(Model::Vtlc_L_value, ui->Vtlc_left);
	GET_MODEL(Model::Vtlc_R_value, ui->Vtlc_right);
	GET_MODEL(Model::Pat_Ht_value, ui->patHt);
	GET_MODEL(Model::Pat_Wt_value, ui->patWt);
	GET_MODEL(Model::Hct_value, ui->Hct);
	GET_MODEL(Model::PA_EVL_value, ui->PA_EVL);
	GET_MODEL(Model::PA_Diam_value, ui->PA_Diameter);
	GET_MODEL(Model::PV_EVL_value, ui->PV_EVL);
	GET_MODEL(Model::PV_Diam_value, ui->PV_Diameter);

	/* Update results in the results pane */
	double pvr = model->getResult(Model::TotalR_value);
	double rus = model->getResult(Model::Rus_value);
	double rm = model->getResult(Model::Rm_value);
	double rds = model->getResult(Model::Rds_value);

	ui->PAPm2->setText(doubleToString(model->getResult(Model::PAP_value)));
	ui->PVR->setText(doubleToString(pvr));
	ui->Rus->setText(doubleToString(rus));
	ui->Rm->setText(doubleToString(rm));
	ui->Rds->setText(doubleToString(rds));
	ui->Rus_PVR->setText(doubleToString(100*rus/pvr) + "%");
	ui->Rm_PVR->setText(doubleToString(100*rm/pvr, 2) + "%");
	ui->Rds_PVR->setText(doubleToString(100*rds/pvr, 2) + "%");

	// update disease parameters
	const DiseaseList disease_list = model->diseases();
	if (disease_model->hasSlewParameter()) {
		int param_no;
		Disease slew_disease = disease_model->slewParameter(param_no);

		disease_model->setDiseases(model->diseases());
		disease_model->setSlewParameter(slew_disease, param_no);
	}
	else
		disease_model->setDiseases(disease_list);

	ui->diseaseView->setHidden(disease_list.empty());
	ui->diseaseViewLabel->setHidden(disease_list.empty());

	// update calibration ratios
	ui->krc_factor->setText(doubleToString(model->getKrc()));

	// force overlay relaculation, if any
	ui->view->setOverlayType(ui->view->overlayType());

#undef GET_MODEL
}

static void ADD_MODEL_RANGE(QList<QPair<Model::DataType, Range> > &ret,
                            Model::DataType type, QLineEdit *control)
{
	if (!control->isEnabled())
		return;

	Range r(control->text());
	if (r.isValid())
		ret << QPair<Model::DataType, Range>(type, r);
}

QList<QPair<Model::DataType, Range> > MainWindow::fetchModelInputs() const
{
	QList<QPair<Model::DataType, Range> > ret;

	/* Update model inputs */
	if (ui->similarLungsRadioButton->isChecked()) {
		// similar lungs
		ADD_MODEL_RANGE(ret, Model::Lung_Ht_value, ui->lungHt);
		ADD_MODEL_RANGE(ret, Model::Vm_value, ui->Vm);
		ADD_MODEL_RANGE(ret, Model::Vrv_value, ui->Vrv);
		ADD_MODEL_RANGE(ret, Model::Vfrc_value, ui->Vfrc);
		ADD_MODEL_RANGE(ret, Model::Vtlc_value, ui->Vtlc);
	}
	else {
		// dissimilar lungs
		ADD_MODEL_RANGE(ret, Model::Lung_Ht_L_value, ui->lungHt_left);
		ADD_MODEL_RANGE(ret, Model::Lung_Ht_R_value, ui->lungHt_right);
		ADD_MODEL_RANGE(ret, Model::Vm_L_value, ui->Vm_left);
		ADD_MODEL_RANGE(ret, Model::Vm_R_value, ui->Vm_right);
		ADD_MODEL_RANGE(ret, Model::Vrv_L_value, ui->Vrv_left);
		ADD_MODEL_RANGE(ret, Model::Vrv_R_value, ui->Vrv_right);
		ADD_MODEL_RANGE(ret, Model::Vfrc_L_value, ui->Vfrc_left);
		ADD_MODEL_RANGE(ret, Model::Vfrc_R_value, ui->Vfrc_right);
		ADD_MODEL_RANGE(ret, Model::Vtlc_L_value, ui->Vtlc_left);
		ADD_MODEL_RANGE(ret, Model::Vtlc_R_value, ui->Vtlc_right);
	}
	ADD_MODEL_RANGE(ret, Model::CO_value, ui->CO);
	ADD_MODEL_RANGE(ret, Model::LAP_value, ui->LAP);
	ADD_MODEL_RANGE(ret, Model::Pal_value, ui->Pal);
	ADD_MODEL_RANGE(ret, Model::Ppl_value, ui->Ppl);
	ADD_MODEL_RANGE(ret, Model::Tlrns_value, ui->Tolerance);
	ADD_MODEL_RANGE(ret, Model::Pat_Ht_value, ui->patHt);
	ADD_MODEL_RANGE(ret, Model::Pat_Wt_value, ui->patWt);
	ADD_MODEL_RANGE(ret, Model::Hct_value, ui->Hct);
	ADD_MODEL_RANGE(ret, Model::PA_EVL_value, ui->PA_EVL);
	ADD_MODEL_RANGE(ret, Model::PA_Diam_value, ui->PA_Diameter);
	ADD_MODEL_RANGE(ret, Model::PV_EVL_value, ui->PV_EVL);
	ADD_MODEL_RANGE(ret, Model::PV_Diam_value, ui->PV_Diameter);

	if (ui->PAPm->isVisible()) {
		// PAPm range for calculation of compromise
		ADD_MODEL_RANGE(ret, Model::PAP_value, ui->PAPm);
	}

	return ret;
}

void MainWindow::setupNewModelScene()
{
	// FIXME: adapt model scene to redraw model without reallocation if the
	// new model contains same geometry.
	ModelScene *old_scene = scene;
	scene = new ModelScene(*baseline, *model, this);
	ui->view->setScene(scene);

	if (old_scene)
		delete old_scene;

	connect(scene, SIGNAL(arteryDoubleClicked(int,int)), SLOT(modifyArtery(int,int)));
	connect(scene, SIGNAL(veinDoubleClicked(int,int)), SLOT(modifyVein(int,int)));
	connect(scene, SIGNAL(capillaryDoubleClicked(int)), SLOT(modifyCap(int)));

	connect(scene, SIGNAL(updateTransducerPosition(Model::Transducer)),
	        SLOT(transducerPositionChanged(Model::Transducer)));
}

void MainWindow::updateDiseaseMenu()
{
	ui->menuDisease->clear();
	for (std::map<QAction*,Disease>::const_iterator i=disease_actions.begin();
	     i!=disease_actions.end(); ++i) {
		delete i->first;
	}
	disease_actions.clear();

	const QList<QAbstractButton*> disease_checkbox_list = disease_box_group->buttons();
	foreach (QAbstractButton *button, disease_checkbox_list)
		delete button;

	// build disease menu and disease button box
	const DiseaseList &diseases = Disease::allDiseases();
	const DiseaseList &model_diseases = disease_model->diseases();
	foreach(const Disease &disease, diseases) {
		bool disease_in_model = false;

		foreach (const Disease &model_disease, model_diseases)
			if (model_disease.id() == disease.id()) {
				disease_in_model = true;
				break;
			}


		QAction *a = new QAction(disease.name(), this);
		a->setCheckable(true);
		a->setData(disease.id());
		a->setChecked(disease_in_model);
		connect(a, SIGNAL(triggered(bool)), SLOT(diseaseActionTriggered(bool)));

		QCheckBox *checkbox = new QCheckBox(disease.name(), ui->diseaseBox);
		checkbox->setChecked(disease_in_model);
		ui->diseaseBoxLayout->addWidget(checkbox);
		disease_box_group->addButton(checkbox, disease.id());

		ui->menuDisease->addAction(a);
		disease_actions.insert(std::pair<QAction*,Disease>(a, disease));
	}

	ui->menuDisease->addSeparator();
	ui->menuDisease->addAction(ui->actionConfigureDisease);

}

void MainWindow::updateTitleFilename()
{
	const QLatin1Char sep = QLatin1Char('-');
	QString title_proper = windowTitle();

	title_proper = title_proper.left(title_proper.indexOf(sep));
	if (!save_filename.isEmpty()) {
		QFileInfo fi(save_filename);
		title_proper = title_proper.simplified()
		                .append(QString(" ") + sep + " " + fi.fileName());

		if (baseline->isModified() || model->isModified())
			title_proper.append(QLatin1Char('*'));
	}

	if (windowTitle() != title_proper)
		setWindowTitle(title_proper);
}

bool MainWindow::isVesselPtpReadOnly() const
{
	return Range(ui->lungHt->text()).sequenceCount() > 1 ||
	       Range(ui->Ppl->text()).sequenceCount() > 1 ||
	       Range(ui->Pal->text()).sequenceCount() > 1;
}

void MainWindow::animateLabelVisibility(QWidget *label, bool fVisible)
{
	// remove animations already in progress
	for (std::list<QPropertyAnimation*>::iterator i=animations.begin(); i!=animations.end(); ) {
		QPropertyAnimation &anim = **i;
		if (anim.targetObject() == label) {
			anim.deleteLater();
			i = animations.erase(i);

			// invalidate current geometry that may not be optimal
			label->setVisible(false);
		}
		else {
			++i;
		}
	}

	// called first to update geometry
	label->setVisible(true);

	// TODO: position of label may change if another label becomes visible
	// above currently animated label, so gemetry is not ideal for this
	QSize s = label->geometry().size();
	QPropertyAnimation *anim = new QPropertyAnimation(label, "geometry", this);
	const QRect min_rect(label->geometry().topLeft(), QSize(s.width(), 0));
	const QRect max_rect(label->geometry().topLeft(), s);

	anim->setStartValue(fVisible ? min_rect : max_rect);
	anim->setEndValue(fVisible ? max_rect : min_rect);
	anim->setDuration(250);
	anim->setEasingCurve(QEasingCurve::OutCubic);
	anim->start();
	animations.push_back(anim);

	connect(anim, SIGNAL(finished()), SLOT(animationFinished()));
}

void MainWindow::calculationCompleted()
{
	ModelCalcList res = calc_thread->output();
	if (res.size() < 1)
		return;

	int calculation_errors = 0;
	for (ModelCalcList::const_iterator i=res.begin(); i!=res.end(); ++i) {
		calculation_errors = i->second->calculationErrors();

		if (calculation_errors != 0)
			break;
	}
	if (calculation_errors) {
		QMessageBox::information(this, "Calculation Errors",
		                         QString("Internal calculation errors were encountered.\n"
		                                 "Please try disabling OpenCL in the Options menu \n"
		                                 "and recalculating.\n\n"
		                                 "Error #: %1").arg(calculation_errors));
	}

	QApplication::beep();
	activateWindow();

	if (res.size() == 1) {
		const Model *m = res.first().second;
		modelSelected(res.first().second, res.first().first);

		calc_thread->deleteLater();
		calc_thread = 0;

		/* Diagnostic messages for out of bounds calculations */
		if (ui->actionDiseaseProgression->isChecked()) {
			const double targetPAP = ui->PAPm->text().toDouble();
			const double calcPAP = ui->PAPm2->text().toDouble();
			const double tlrns = m->getResult(Model::Tlrns_value);

			if (calcPAP - targetPAP > tlrns * targetPAP) {
				QMessageBox::information(this, "PAPm cannot be calculated",
				          "The target PAPm is below the estimated PAPm \n"
				          "for the minimum level of disease possible.");
			}
			else if (calcPAP - targetPAP < -tlrns * targetPAP) {
				QMessageBox::information(this, "PAPm cannot be calculated",
				          "The target PAPm is above the estimated PAPm \n"
				          "for the maximum level of disease possible.");
			}
		}

		ui->recalc->setEnabled(true);
		return;
	}

	// multi-output result
	if (mres)
		delete mres;

	mres = new MultiModelOutput(this);
	mres->setModels(res);
	mres->show();

	connect(mres, SIGNAL(finished(int)), SLOT(multiModelWindowClosed()));
	connect(mres, SIGNAL(doubleClicked(Model*,int)), SLOT(modelSelected(Model*,int)));
}

void MainWindow::modelSelected(Model *new_model, int n_iters)
{
	const QString iter_msg("Iterations: %1");
	ui->statusbar->showMessage(iter_msg.arg(n_iters));

	*model = *new_model;
	updateInputsOutputs();
	update();

	updateTitleFilename();

	if (model->numIterations() >= 50)
		on_actionConvergenceSummary_triggered();
}

void MainWindow::multiModelWindowClosed()
{
	if (mres) {
		mres->deleteLater();
		mres = 0;

		if (calc_thread)
			calc_thread->deleteLater();
		calc_thread = 0;
	}

	ui->recalc->setEnabled(true);
}

void MainWindow::diseaseActionTriggered(bool is_enabled)
{
	QAction *ac = static_cast<QAction*>(sender());
	std::map<QAction*,Disease>::const_iterator i = disease_actions.find(ac);
	if (i == disease_actions.end())
		return;

	const Disease &d = i->second;

	if (is_enabled)
		disease_model->addDisease(d);
	else
		disease_model->removeDisease(d);


	QAbstractButton *disease_button = disease_box_group->button(d.id());
	if (disease_button)
		disease_button->setChecked(is_enabled);
}

void MainWindow::diseaseGroupBoxTriggered(int id)
{
	for (std::map<QAction*,Disease>::const_iterator i=disease_actions.begin();
	     i!=disease_actions.end();
	     ++i) {

		if (i->second.id() == id) {
			i->first->trigger();
			break;
		}
	}
}

void MainWindow::calculationTypeButtonGroupClick(int id)
{
	switch ((ModelCalculationType)id) {
	case DiseaseProgressCalculation:
		ui->actionDiseaseProgression->trigger();
		break;
	case PAPmCalculation:
		ui->actionPAP->trigger();
		break;
	}
}

void MainWindow::allocateNewModel(bool propagate_diseases_to_new_model)
{
	Model *old_model = model;

	ModelCalculationType calculation_type = PAPmCalculation;
	Model::Transducer trans_pos = model->transducerPos();
	Model::IntegralType type = Model::SegmentedVesselFlow;

	if (ui->actionDiseaseProgression->isChecked())
		calculation_type = DiseaseProgressCalculation;

	if (ui->actionSegmentedVessels->isChecked())
		type = Model::SegmentedVesselFlow;
	else if(ui->actionRigidVessels->isChecked())
		type = Model::RigidVesselFlow;
	else if(ui->actionNavierStokes->isChecked())
		type = Model::NavierStokes;

	switch (calculation_type) {
	case DiseaseProgressCalculation:
		model = new CompromiseModel(trans_pos, type);
		break;
	case PAPmCalculation:
		model = new Model(trans_pos, type);
		break;
	}

	if (propagate_diseases_to_new_model) {
		const DiseaseList diseases = disease_model->diseases();
		for (DiseaseList::const_iterator i=diseases.begin(); i!=diseases.end(); i++)
			model->addDisease(*i);
	}
	else {
		// remove diseases from disease_model
		disease_model->clear();
		updateDiseaseMenu();
	}

	delete baseline;
	baseline = model->clone();

	adjustVisibleModelInputs(calculation_type);
	setupNewModelScene();
	updateInputsOutputs();

	delete old_model;
}

void MainWindow::dissimilarLungSelection(int id)
{
	int current = ui->dissimilarLungParameterStack->currentIndex();

	if (id == current)
		return;

	switch (id) {
	case 0:    // move dissimular => similar values
#define COPY_TEXT(name) do { \
	ui->name->blockSignals(true); \
	ui->name->setText(ui->name##_left->text()); \
	ui->name->blockSignals(false); \
	} while(false)

		COPY_TEXT(lungHt);
		COPY_TEXT(Vm);
		COPY_TEXT(Vrv);
		COPY_TEXT(Vtlc);
		COPY_TEXT(Vfrc);
#undef COPY_TEXT
		break;

	case 1:    // move similar => dissimilar values
#define COPY_TEXT(name) do { \
	ui->name##_left->blockSignals(true); \
	ui->name##_right->blockSignals(true); \
	ui->name##_left->setText(ui->name->text()); \
	ui->name##_right->setText(ui->name->text()); \
	ui->name##_left->blockSignals(false); \
	ui->name##_right->blockSignals(false); \
	} while(false)

		COPY_TEXT(lungHt);
		COPY_TEXT(Vm);
		COPY_TEXT(Vrv);
		COPY_TEXT(Vtlc);
		COPY_TEXT(Vfrc);

#undef COPY_TEXT
		break;

	default:
		qDebug() << "should not happen" << __FUNCTION__ << __LINE__ << id;
	}

	ui->dissimilarLungParameterStack->setCurrentIndex(id);
}

void MainWindow::patHtWtChanged()
{
	const Range ht_range(ui->patHt->text());
	Range wt_range(ui->patWt->text());
	const bool is_range = ht_range.sequenceCount() > 1 ||
	                      wt_range.sequenceCount() > 1;

	ui->CO->setDisabled(is_range);

	bool data_modified = false;
	if (ht_range.sequenceCount() == 1) {
		double val = ht_range.firstValue();
		data_modified = baseline->setData(Model::Pat_Ht_value, val);
		if (wt_range.sequenceCount() == 1 && data_modified) {
			// Only set ideal weight if we change height
			// and weight not a range

			double ideal_weight = Model::idealWeight(baseline->gender(),
			                                         val);
			ui->patWt->setText(doubleToString(ideal_weight));
			wt_range.setMin(ideal_weight);
			wt_range.setMax(ideal_weight);
		}
	}

	if (!data_modified && wt_range.sequenceCount() == 1) {
		double val = wt_range.firstValue();
		data_modified = baseline->setData(Model::Pat_Wt_value, val) || data_modified;
	}

	// redisplay CO/CL, PA and PV diameters, if it changed
	if (data_modified) {
		ui->CO->blockSignals(true);
		ui->PA_Diameter->blockSignals(true);
		ui->PV_Diameter->blockSignals(true);

		ui->CO->setText(doubleToString(baseline->getResult(Model::CO_value)));
		ui->PA_Diameter->setText(doubleToString(baseline->getResult(Model::PA_Diam_value)));
		ui->PV_Diameter->setText(doubleToString(baseline->getResult(Model::PV_Diam_value)));

		ui->CO->blockSignals(false);
		ui->PA_Diameter->blockSignals(false);
		ui->PV_Diameter->blockSignals(false);

		scene->update();
	}
}

void MainWindow::vesselPtpDependenciesChanged()
{
	const Range ppl(ui->Ppl->text());
	const Range pal(ui->Pal->text());

	bool data_modified = false;

	if (ui->similarLungsRadioButton->isChecked()) {
		const Range lung_height(ui->lungHt->text());

		if (lung_height.sequenceCount() == 1) {
			double val = lung_height.firstValue();
			data_modified = baseline->setData(Model::Lung_Ht_value, val) || data_modified;
		}
	}
	else {
		// dissimilar lungs!
		const Range lh_left(ui->lungHt_left->text());
		const Range lh_right(ui->lungHt_right->text());

		if (lh_left.sequenceCount() == 1) {
			double val = lh_left.firstValue();
			data_modified = baseline->setData(Model::Lung_Ht_L_value, val) || data_modified;
		}
		if (lh_right.sequenceCount() == 1) {
			double val = lh_right.firstValue();
			data_modified = baseline->setData(Model::Lung_Ht_R_value, val) || data_modified;
		}
	}


	if (ppl.sequenceCount() == 1) {
		double val = ppl.firstValue();
		data_modified = baseline->setData(Model::Ppl_value, val) || data_modified;
	}

	if (pal.sequenceCount() == 1) {
		double val = pal.firstValue();
		data_modified = baseline->setData(Model::Pal_value, val) || data_modified;
	}

	if (data_modified)
		scene->update();
}

void MainWindow::globalVolumesChanged()
{
	bool Vm_ok, Vrv_ok, Vfrc_ok, Vtlc_ok;
	const double new_Vm = ui->Vm->text().toDouble(&Vm_ok);
	const double new_Vrv = ui->Vrv->text().toDouble(&Vrv_ok);
	const double new_Vfrc = ui->Vfrc->text().toDouble(&Vfrc_ok);
	const double new_Vtlc = ui->Vtlc->text().toDouble(&Vtlc_ok);

	if (Vm_ok)
		baseline->setData(Model::Vm_value, new_Vm);
	if (Vrv_ok)
		baseline->setData(Model::Vrv_value, new_Vrv);
	if (Vfrc_ok)
		baseline->setData(Model::Vfrc_value, new_Vfrc);
	if (Vtlc_ok)
		baseline->setData(Model::Vtlc_value, new_Vtlc);

	scene->update();
}

void MainWindow::vesselDimsChanged()
{
	bool pa_diam_ok, pv_diam_ok, pa_evl_ok, pv_evl_ok;

	const double new_pa_diam = ui->PA_Diameter->text().toDouble(&pa_diam_ok);
	const double new_pv_diam = ui->PV_Diameter->text().toDouble(&pv_diam_ok);
	const double new_pa_evl = ui->PA_EVL->text().toDouble(&pa_evl_ok);
	const double new_pv_evl = ui->PV_EVL->text().toDouble(&pv_evl_ok);

	if (pa_diam_ok)
		baseline->setData(Model::PA_Diam_value, new_pa_diam);
	if (pv_diam_ok)
		baseline->setData(Model::PV_Diam_value, new_pv_diam);
	if (pa_evl_ok)
		baseline->setData(Model::PA_EVL_value, new_pa_evl);
	if (pv_evl_ok)
		baseline->setData(Model::PV_EVL_value, new_pv_evl);

	scene->update();
}

void MainWindow::animatePatHtLabel(bool fVisible)
{
	animateLabelVisibility(ui->PatHtModLabel, fVisible);
}

void MainWindow::animatePatWtLabel(bool fVisible)
{
	animateLabelVisibility(ui->PatWtModLabel, fVisible);
}

void MainWindow::animateLungHtLabel(bool fVisible)
{
	animateLabelVisibility(ui->lungHtModLabel, fVisible);
}

void MainWindow::animatePplModLabel(bool fVisible)
{
	animateLabelVisibility(ui->PplModLabel, fVisible);
}

void MainWindow::animatePalModLabel(bool fVisible)
{
	animateLabelVisibility(ui->PalModLabel, fVisible);
}

void MainWindow::animatePVParams(bool fVisible)
{
	qDebug("focus: %d", fVisible);
	animateLabelVisibility(ui->pv_params_label, fVisible);
}

void MainWindow::animationFinished()
{
	QPropertyAnimation *anim = qobject_cast<QPropertyAnimation*>(sender());
	if (anim == 0)
		return;

	std::list<QPropertyAnimation*>::iterator i =
	                std::find(animations.begin(), animations.end(), anim);
	if (i == animations.end())
		return;

	animations.erase(i);
	QWidget *w = static_cast<QWidget*>(anim->targetObject());
	w->setVisible(w->geometry().height() != 0);
	anim->deleteLater();
}
