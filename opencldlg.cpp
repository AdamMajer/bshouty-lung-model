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

#include <stdexcept>
#include <string>
#include "dbsettings.h"
#include "opencl.h"
#include "opencldlg.h"
#include "ui_opencldlg.h"

OpenCLDlg::OpenCLDlg(QWidget *parent)
        : QDialog(parent)
{
	ui = new Ui::OpenCLDlg;
	ui->setupUi(this);

	if (!cl->isAvailable()) {
		ui->device_list->setDisabled(true);
		ui->openCLEnabledCheckBox->setDisabled(true);
		ui->openCLEnabledCheckBox->setChecked(false);

		new QTreeWidgetItem(ui->device_list,
		                    QStringList() << "OpenCL not available");
		return;
	}

	ui->openCLEnabledCheckBox->setChecked(DbSettings::value(settings_opencl_enabled, true).toBool());

	// Add devices to tree
	OpenCLDeviceList devices = cl->devices();
	foreach (const OpenCL_device &dev, devices) {
		QTreeWidgetItem *item;

		item = new QTreeWidgetItem(ui->device_list,
		                           QStringList() << QString::fromLatin1(dev.name));

		const QStringList desc = QStringList()
		    << QString::fromLatin1("Memory Size: ") +
		       QString::number(dev.max_mem_size/1024/1024) +
		       QString::fromLatin1(" MB")

		    << QString::fromLatin1("Max Workgroup Size: ") +
		       QString::number(dev.max_work_group_size)

		    << QString::fromLatin1("Max Work Item Size: ") +
		       QString::number(dev.max_work_item_size[0]) + "x" +
		       QString::number(dev.max_work_item_size[1])

		    << QString::fromLatin1("Max Alloc Size: ") +
		       QString::number(dev.max_alloc_size/1024/1024) +
		       QString::fromLatin1(" MB")

		    << QString::fromLatin1("Cacheline Size: ") +
		       QString::number(dev.cacheline_size);

		foreach (const QString &i, desc)
			item->addChild(new QTreeWidgetItem(QStringList() << i));
	}

	ui->device_list->expandAll();
}

OpenCLDlg::~OpenCLDlg()
{
	delete ui;
}

void OpenCLDlg::accept()
{
	QDialog::accept();

	DbSettings::setValue(settings_opencl_enabled, ui->openCLEnabledCheckBox->isChecked());
}

void OpenCLDlg::setOpenCLEnabled(bool enabled)
{
	ui->openCLEnabledCheckBox->setChecked(enabled);
}

bool OpenCLDlg::isOpenCLEnabled() const
{
	return ui->openCLEnabledCheckBox->isChecked();
}
