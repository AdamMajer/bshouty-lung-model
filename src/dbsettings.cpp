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

#include <QRect>
#include <QRegExp>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTextStream>
#include "common.h"
#include "dbsettings.h"

static const QLatin1String string_key("@String");
static const QLatin1String rect_key("@Rect");

static std::map<QString,QVariant> value_map;

QVariant DbSettings::value(const QString &key, const QVariant &default_value)
{
	QSqlDatabase db = QSqlDatabase::database(settings_db);
	if (!db.isOpen())
		return default_value;

	std::map<QString,QVariant>::const_iterator cached_value = value_map.find(key);
	if (cached_value != value_map.end())
		return cached_value->second;

	QSqlQuery q(db);
	q.prepare("SELECT value FROM settings WHERE key=?");
	q.addBindValue(key);

	if (q.exec() && q.next()) {
		const QString string_value = q.value(0).toString();
		const int string_len = string_value.length();

		if (string_len > 8 && string_value.startsWith(string_key)) {
			return string_value.right(string_len - 8);
		}
		else if (string_len > 6 && string_value.startsWith(rect_key)) {
			const QRegExp rx("^" + rect_key + " (-?\\d+), (-?\\d+), (\\d+), (\\d+)$");
			if (rx.exactMatch(string_value)) {
				return QRect(rx.cap(1).toInt(), rx.cap(2).toInt(),
				             rx.cap(3).toInt(), rx.cap(4).toInt());
			}
		}
	}

	return default_value;
}

void DbSettings::setValue(const QString &key, const QVariant &value)
{
	QSqlDatabase db = QSqlDatabase::database(settings_db);
	if (!db.isOpen())
		return;

	QSqlQuery q(db);
	q.prepare("SELECT value FROM settings WHERE key=?");
	q.addBindValue(key);
	if (q.exec()) {
		QString string_value;

		if (value.canConvert<QString>()) {
			string_value = QLatin1String("@String ") + value.toString();
		}
		else if (value.canConvert<QRect>()) {
			QTextStream out(&string_value, QIODevice::WriteOnly);
			const QRect r = value.toRect();

			out << rect_key << " "
			    << r.left() << ", "
			    << r.top() << ", "
			    << r.width() << ", "
			    << r.height();
		}
		else
			Q_ASSERT(false);

		value_map[key] = value;

		if (q.next())
			// update query
			q.prepare("UPDATE settings SET value=? WHERE key=?");
		else
			// insert query
			q.prepare("INSERT INTO settings (value, key) VALUES (?,?)");

		q.addBindValue(string_value);
		q.addBindValue(key);
		q.exec();
	}
}
