/*
 *   Bshouty Lung Model - Pulmonary Circulation Simulation
 *    Copyright (c) 1989-2014 Zoheir Bshouty, MD, PhD, FRCPC
 *    Copyright (c) 2011-2014 Adam Majer
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

#include "common.h"
#include "mainwindow.h"
#include "opencl.h"
#include <QApplication>
#include <QDir>
#include <QDebug>
#include <QList>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

static void updateSettingsDb(QSqlDatabase db)
{
	/* NOTE: Do not change ids of PAH/PVOD diseases without changing them in common.h
	 *       These IDs are used by the wizard dialog.
	 */
	QList<QStringList> sql = QList<QStringList>() <<
	    (QStringList()
	     << "CREATE TABLE diseases(id INTEGER PRIMARY KEY ASC, name NOT NULL, script NOT NULL)"
	     << "CREATE TABLE disease_parameters(disease_id INTEGER NOT NULL REFERENCES diseases(id), param_name NOT NULL, value)"
	     << "CREATE TABLE version(ver INTEGER PRIMARY KEY)"
	     << "INSERT INTO version VALUES(1)") <<
	    (QStringList()
	     << "INSERT INTO diseases(id,name,script) VALUES(-1, 'PAH', '({ \n\nname: function() {\n        return [\"PAH\", \"Pulmonary Arterial Hypertension\", true];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nartery: function(compromise) {\n    if (gen > 4*n_gen/5) {\n            var area = (100.0 - compromise)/100.0;\n            R = R/area/area;\n            return true;\n    }\n\n    return false;\n},\n\n})')"
	     << "INSERT INTO diseases(id,name,script) VALUES(-2, 'PVOD', '({ \n\nname: function() {\n        return [\"PVOD\", \"Pulmonary Venous Hypertension\", true];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nvein: function(compromise) {\n    if (gen > 4*n_gen/5) {\n            var area = (100.0 - compromise)/100.0;\n            R = R/area/area;\n            return true;\n    }\n\n    return false;\n},\n\n})')"
	     << "UPDATE version SET ver=2") <<
            (QStringList()
	     << "CREATE TABLE settings (key PRIMARY KEY NOT NULL, value NOT NULL)"
	     << "UPDATE version SET ver=3") <<
            (QStringList()
	     << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PAH\", \"Pulmonary Arterial Hypertension\", true];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nartery: function(compromise) {\n    if (this.gen > 4*this.n_gen/5) {\n            var area = (100.0 - compromise)/100.0;\n            this.R = this.R/area/area;\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-1"
	     << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PVOD\", \"Pulmonary Venous Hypertension\", true];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nvein: function(compromise) {\n    if (this.gen > 4*this.n_gen/5) {\n            var area = (100.0 - compromise)/100.0;\n            this.R = this.R/area/area;\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-2"
	     << "UPDATE version SET ver=4") <<
	    (QStringList()
	     << "UPDATE diseases SET id=-3 WHERE id=-1"
	     << "UPDATE disease_parameters SET disease_id=-3 WHERE disease_id=-1"
	     << "UPDATE version SET ver=5") <<
	    (QStringList()
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PAH - Diffuse\", \"Pulmonary Arterial Hypertension\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nartery: function(compromise) {\n    if (this.gen > 4*this.n_gen/5) {\n            var area = (100.0 - compromise)/100.0;\n            this.R = this.R/area/area;\n            return true;\n    }\n\n    return false;\n},\n\n})', id=-20 WHERE id=-3"
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PVOD - Diffuse\", \"Pulmonary Venous Hypertension\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nvein: function(compromise) {\n    if (this.gen > 4*this.n_gen/5) {\n            var area = (100.0 - compromise)/100.0;\n            this.R = this.R/area/area;\n            return true;\n    }\n\n    return false;\n},\n\n})', id=-10 WHERE id=-2"
	        << "INSERT INTO diseases (id,name,script) VALUES(-19, 'PAH - Left Lung', '({ \n\nname: function() {\n        return [\"PAH - Left Lung\", \"Pulmonary Arterial Hypertension - in single lung, upper half of lung affected\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nartery: function(compromise) {\n    if (this.gen > 4*this.n_gen/5 && this.vessel_idx<this.n_vessels/2) {\n            var area = (100.0 - compromise)/100.0;\n            this.R = this.R/area/area;\n            return true;\n    }\n\n    return false;\n},\n\n})')"
	        << "INSERT INTO diseases (id,name,script) VALUES(-18, 'PAH - Right Lung', '({ \n\nname: function() {\n        return [\"PAH - Right Lung\", \"Pulmonary Arterial Hypertension - in single lung, lower half of lung affected\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nartery: function(compromise) {\n    if (this.gen > 4*this.n_gen/5 && this.vessel_idx>=this.n_vessels/2) {\n            var area = (100.0 - compromise)/100.0;\n            this.R = this.R/area/area;\n            return true;\n    }\n\n    return false;\n},\n\n})')"
	        << "INSERT INTO diseases (id,name,script) VALUES(-9, 'PVOD - Left Lung', '({ \n\nname: function() {\n        return [\"PVOD - Left Lung\", \"Pulmonary Venous Hypertension - in single lung, upper half of lung affected\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nvein: function(compromise) {\n    if (this.gen > 4*this.n_gen/5 && this.vessel_idx<this.n_vessels/2) {\n            var area = (100.0 - compromise)/100.0;\n            this.R = this.R/area/area;\n            return true;\n    }\n\n    return false;\n},\n\n})')"
	        << "INSERT INTO diseases (id,name,script) VALUES(-8, 'PVOD - Right Lung', '({ \n\nname: function() {\n        return [\"PVOD - Right Lung\", \"Pulmonary Venous Hypertension - in single lung, lower half of lung affected\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nvein: function(compromise) {\n    if (this.gen > 4*this.n_gen/5 && this.vessel_idx>=this.n_vessels/2) {\n            var area = (100.0 - compromise)/100.0;\n            this.R = this.R/area/area;\n            return true;\n    }\n\n    return false;\n},\n\n})')"
	        << "UPDATE version SET ver=6") <<
	    (QStringList()
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PAH - Diffuse\", \"Pulmonary Arterial Hypertension\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nartery: function(compromise) {\n    if (this.gen > 4*this.n_gen/5+this.n_gen%5) {\n            var area = (100.0 - compromise)/100.0;\n            this.R = this.R/area/area;\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-20"
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PVOD - Diffuse\", \"Pulmonary Venous Hypertension\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nvein: function(compromise) {\n    if (this.gen > 4*this.n_gen/5+this.n_gen%5) {\n            var area = (100.0 - compromise)/100.0;\n            this.R = this.R/area/area;\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-10"
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PAH - Left Lung\", \"Pulmonary Arterial Hypertension - in single lung, upper half of lung affected\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nartery: function(compromise) {\n    if (this.gen > 4*this.n_gen/5+this.n_gen%5 && this.vessel_idx<this.n_vessels/2) {\n            var area = (100.0 - compromise)/100.0;\n            this.R = this.R/area/area;\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-19"
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PAH - Right Lung\", \"Pulmonary Arterial Hypertension - in single lung, lower half of lung affected\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nartery: function(compromise) {\n    if (this.gen > 4*this.n_gen/5+this.n_gen%5 && this.vessel_idx>=this.n_vessels/2) {\n            var area = (100.0 - compromise)/100.0;\n            this.R = this.R/area/area;\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-18"
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PVOD - Left Lung\", \"Pulmonary Venous Hypertension - in single lung, upper half of lung affected\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nvein: function(compromise) {\n    if (this.gen > 4*this.n_gen/5+this.n_gen%5 && this.vessel_idx<this.n_vessels/2) {\n            var area = (100.0 - compromise)/100.0;\n            this.R = this.R/area/area;\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-9"
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PVOD - Right Lung\", \"Pulmonary Venous Hypertension - in single lung, lower half of lung affected\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nvein: function(compromise) {\n    if (this.gen > 4*this.n_gen/5+this.n_gen%5 && this.vessel_idx>=this.n_vessels/2) {\n            var area = (100.0 - compromise)/100.0;\n            this.R = this.R/area/area;\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-8"
	        << "UPDATE version SET ver=7")
	    << (QStringList()
	        << "DELETE FROM settings WHERE key LIKE '/settings/calibration/%'"
	        << "UPDATE version SET ver=8")
	    << (QStringList()
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PAH - Diffuse\", \"Pulmonary Arterial Hypertension\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nartery: function(compromise) {\n    if (this.gen > 4*this.n_gen/5+this.n_gen%5) {\n            var area = (100.0 - compromise)/100.0;\n            this.D = this.D*Math.sqrt(area);\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-20"
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PVOD - Diffuse\", \"Pulmonary Venous Hypertension\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nvein: function(compromise) {\n    if (this.gen > 4*this.n_gen/5+this.n_gen%5) {\n            var area = (100.0 - compromise)/100.0;\n            this.D = this.D*Math.sqrt(area);\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-10"
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PAH - Left Lung\", \"Pulmonary Arterial Hypertension - in single lung, upper half of lung affected\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nartery: function(compromise) {\n    if (this.gen > 4*this.n_gen/5+this.n_gen%5 && this.vessel_idx<this.n_vessels/2) {\n            var area = (100.0 - compromise)/100.0;\n            this.D = this.D*Math.sqrt(area);\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-19"
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PAH - Right Lung\", \"Pulmonary Arterial Hypertension - in single lung, lower half of lung affected\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nartery: function(compromise) {\n    if (this.gen > 4*this.n_gen/5+this.n_gen%5 && this.vessel_idx>=this.n_vessels/2) {\n            var area = (100.0 - compromise)/100.0;\n            this.D = this.D*Math.sqrt(area);\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-18"
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PVOD - Left Lung\", \"Pulmonary Venous Hypertension - in single lung, upper half of lung affected\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nvein: function(compromise) {\n    if (this.gen > 4*this.n_gen/5+this.n_gen%5 && this.vessel_idx<this.n_vessels/2) {\n            var area = (100.0 - compromise)/100.0;\n            this.D = this.D*Math.sqrt(area);\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-9"
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PVOD - Right Lung\", \"Pulmonary Venous Hypertension - in single lung, lower half of lung affected\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nvein: function(compromise) {\n    if (this.gen > 4*this.n_gen/5+this.n_gen%5 && this.vessel_idx>=this.n_vessels/2) {\n            var area = (100.0 - compromise)/100.0;\n            this.D = this.D*Math.sqrt(area);\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-8"
	        << "UPDATE version SET ver=9")
	    << (QStringList()
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PAH - Diffuse\", \"Pulmonary Arterial Hypertension\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nartery: function(compromise) {\n    if (this.gen > 4*this.n_gen/5+this.n_gen%5) {\n            var area = (100.0 - compromise)/100.0;\n            this.a = this.a*area;\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-20"
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PVOD - Diffuse\", \"Pulmonary Venous Hypertension\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nvein: function(compromise) {\n    if (this.gen > 4*this.n_gen/5+this.n_gen%5) {\n            var area = (100.0 - compromise)/100.0;\n            this.a = this.a*area;\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-10"
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PAH - Left Lung\", \"Pulmonary Arterial Hypertension - in single lung, upper half of lung affected\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nartery: function(compromise) {\n    if (this.gen > 4*this.n_gen/5+this.n_gen%5 && this.vessel_idx<this.n_vessels/2) {\n            var area = (100.0 - compromise)/100.0;\n            this.a = this.a*area;\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-19"
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PAH - Right Lung\", \"Pulmonary Arterial Hypertension - in single lung, lower half of lung affected\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nartery: function(compromise) {\n    if (this.gen > 4*this.n_gen/5+this.n_gen%5 && this.vessel_idx>=this.n_vessels/2) {\n            var area = (100.0 - compromise)/100.0;\n            this.a = this.a*area;\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-18"
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PVOD - Left Lung\", \"Pulmonary Venous Hypertension - in single lung, upper half of lung affected\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nvein: function(compromise) {\n    if (this.gen > 4*this.n_gen/5+this.n_gen%5 && this.vessel_idx<this.n_vessels/2) {\n            var area = (100.0 - compromise)/100.0;\n            this.a = this.a*area;\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-9"
	        << "UPDATE diseases SET script='({ \n\nname: function() {\n        return [\"PVOD - Right Lung\", \"Pulmonary Venous Hypertension - in single lung, lower half of lung affected\"];\n},\n\nparameters: function() {\n        return [[\"Compromise\", \"0 to 100\"]];\n},\n\nvein: function(compromise) {\n    if (this.gen > 4*this.n_gen/5+this.n_gen%5 && this.vessel_idx>=this.n_vessels/2) {\n            var area = (100.0 - compromise)/100.0;\n            this.a = this.a*area;\n            return true;\n    }\n\n    return false;\n},\n\n})' WHERE id=-8"
	        << "UPDATE version SET ver=10")
	    << (QStringList()
	        << "UPDATE diseases SET script='({\n\nname: function() {\n    return [\"PAH - Diffuse\", \"Pulmonary Arterial Hypertension\"];\n},\n\nparameters: function() {\n    return [[\"Compromise\", \"0 to 100\"]];\n},\n\nartery: function(compromise) {\n    if (this.gen > 13) {\n        var delta = 0.3\n        this.D = this.D * Math.sqrt(1-compromise/100);\n        this.gamma = (this.gamma-1.0)*delta/(1+delta-Math.sqrt(1-compromise/100))+1.0;\n        return true;\n    }\n\n    return false;\n},\n\n})\n' WHERE id=-20"
	        << "UPDATE diseases SET script='({\n\nname: function() {\n    return [\"PVOD - Diffuse\", \"Pulmonary Venous Hypertension\"];\n},\n\nparameters: function() {\n    return [[\"Compromise\", \"0 to 100\"]];\n},\n\nvein: function(compromise) {\n    if (this.gen > 13) {\n        var delta = 0.3\n        this.D = this.D * Math.sqrt(1-compromise/100);\n        this.gamma = (this.gamma-1.0)*delta/(1+delta-Math.sqrt(1-compromise/100))+1.0;\n        return true;\n    }\n\n    return false;\n},\n\n})\n' WHERE id=-10"
	        << "UPDATE diseases SET script='({\n\nname: function() {\n    return [\"PAH - Left Lung\", \"Pulmonary Arterial Hypertension - in single lung, upper half of lung affected\"];\n},\n\nparameters: function() {\n    return [[\"Compromise\", \"0 to 100\"]];\n},\n\nartery: function(compromise) {\n    if (this.gen>13 && this.vessel_idx<this.n_vessels/2) {\n        var delta = 0.3\n        this.D = this.D * Math.sqrt(1-compromise/100);\n        this.gamma = (this.gamma-1.0)*delta/(1+delta-Math.sqrt(1-compromise/100))+1.0;\n        return true;\n    }\n\n    return false;\n},\n\n})\n' WHERE id=-19"
	        << "UPDATE diseases SET script='({\n\nname: function() {\n    return [\"PAH - Right Lung\", \"Pulmonary Arterial Hypertension - in single lung, lower half of lung affected\"];\n},\n\nparameters: function() {\n    return [[\"Compromise\", \"0 to 100\"]];\n},\n\nartery: function(compromise) {\n    if (this.gen>13 && this.vessel_idx>=this.n_vessels/2) {\n        var delta = 0.3\n        this.D = this.D * Math.sqrt(1-compromise/100);\n        this.gamma = (this.gamma-1.0)*delta/(1+delta-Math.sqrt(1-compromise/100))+1.0;\n        return true;\n    }\n\n    return false;\n},\n\n})\n' WHERE id=-18"
	        << "UPDATE diseases SET script='({\n\nname: function() {\n    return [\"PVOD - Left Lung\", \"Pulmonary Venous Hypertension - in single lung, upper half of lung affected\"];\n},\n\nparameters: function() {\n    return [[\"Compromise\", \"0 to 100\"]];\n},\n\nvein: function(compromise) {\n    if (this.gen>13 && this.vessel_idx<this.n_vessels/2) {\n        var delta = 0.3\n        this.D = this.D * Math.sqrt(1-compromise/100);\n        this.gamma = (this.gamma-1.0)*delta/(1+delta-Math.sqrt(1-compromise/100))+1.0;\n        return true;\n    }\n\n    return false;\n},\n\n})\n' WHERE id=-9"
	        << "UPDATE diseases SET script='({\n\nname: function() {\n    return [\"PVOD - Right Lung\", \"Pulmonary Venous Hypertension - in single lung, lower half of lung affected\"];\n},\n\nparameters: function() {\n    return [[\"Compromise\", \"0 to 100\"]];\n},\n\nvein: function(compromise) {\n    if (this.gen>13 && this.vessel_idx>=this.n_vessels/2) {\n        var delta = 0.3\n        this.D = this.D * Math.sqrt(1-compromise/100);\n        this.gamma = (this.gamma-1.0)*delta/(1+delta-Math.sqrt(1-compromise/100))+1.0;\n        return true;\n    }\n\n    return false;\n},\n\n})\n' WHERE id=-8"
	        << "UPDATE version SET ver=11");

	unsigned ver = 0;
	QSqlQuery q(db);
	q.exec("SELECT ver FROM version");
	if (q.next())
		ver = q.value(0).toUInt();

	// Error
	if (ver > (unsigned)sql.size())
		exit(10);

	for (QList<QStringList>::ConstIterator i=sql.begin()+ver; i!=sql.end(); i++) {
		if (!db.transaction())
			exit(13);
		foreach(const QString &item, *i) {
			if (!q.exec(item)) {
				qDebug() << q.lastError().text();
				qDebug() << q.lastQuery();
				exit(11);
			}
		}
		if(!db.commit())
			exit(12);
	}
}


int main( int argc, char *argv[])
{
	QApplication app( argc, argv );
	QApplication::setApplicationName(app_name);
	QApplication::setApplicationVersion("1.0");
	QApplication::setOrganizationDomain("galacticasofware.com");
	QApplication::setOrganizationName("Galactica Software Corporation");

#ifdef _MSC_VER
	// Set plugins to current application path
	app.setLibraryPaths(QStringList() << app.applicationDirPath());
#endif

	cl = new OpenCL();

	QString setting_db_fn;
	QDir d = QDir::home();
#if defined(Q_OS_LINUX)
	if (!d.cd(".config")) {
		if (!d.mkdir(".config")) {
			qCritical() << "Unable to switch to ${HOME}/.config";
			return 1;
		}
	}
	if (!d.cd(app_name)) {
		if (!d.mkdir(app_name)) {
			qCritical() << "Unable to switch to ${HOME}/.config/" + app_name;
		}
		d.cd(app_name);
	}
#elif defined(Q_OS_WIN)
	d = QDir::home();
	if (!d.cd("Application Data")) {
		qCritical() << "Can't switch to Application Data";
	}
	if (!d.cd(app_name)) {
		if (!d.mkdir(app_name)) {
			qCritical() << "Unable to switch to ${HOME}/.config/" + app_name;
		}
		d.cd(app_name);
	}
#else
#error "Unknown filesystem"
#endif

	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", settings_db);
	QString settings_filename = d.absoluteFilePath("settings.db");
	qDebug() << settings_filename;
	db.setDatabaseName(settings_filename);
	if (!db.open()) {
		QMessageBox::critical(0, "Failed to open settings file",
		                      QString("Cannot open settings file at '%1'").arg(settings_filename));
		return 2;
	}
	updateSettingsDb(db);

	// QDir::setCurrent(app.applicationDirPath());
	qDebug("%s", qPrintable(QDir::currentPath()));

	MainWindow *w = new MainWindow;

	/* Load saved file, if passed as an arguments */
	const QStringList & arguments = app.arguments();
	if (arguments.size() > 1)
		w->load(arguments.last());

	w->show();
	app.connect( &app, SIGNAL(lastWindowClosed()), &app, SLOT(quit()));
	int ret = app.exec();

	delete w;
	delete cl;

	db.exec("VACUUM");
	db.close();
	return ret;
}

