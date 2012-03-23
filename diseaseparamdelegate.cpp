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

#include <QDoubleSpinBox>
#include "diseasemodel.h"
#include "diseaseparamdelegate.h"

DiseaseParamDelegate::DiseaseParamDelegate(QObject *parent)
        : QStyledItemDelegate(parent)
{

}

QWidget* DiseaseParamDelegate::createEditor(QWidget *parent,
                                            const QStyleOptionViewItem &option,
                                            const QModelIndex &index) const
{
	QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);

	QDoubleSpinBox *spin_box = qobject_cast<QDoubleSpinBox*>(editor);
	if (spin_box) {
		spin_box->setMinimum(index.data(DiseaseModel::RangeMin).toDouble());
		spin_box->setMaximum(index.data(DiseaseModel::RangeMax).toDouble());
	}

	return editor;
}
