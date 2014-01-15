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

#include <QSyntaxHighlighter>
#include <QTextEdit>

struct TextFormat {
	QTextCharFormat fmt;
	bool is_multiline;

	// following only applies if multiline capable highlighting
	QString eol_marker; // must match EOL if this is to terminate without going to next block
	int block_id;
	QRegExp continuationRx; // when matching on next block, as continuation of previous

	TextFormat()
	        : fmt(), is_multiline(false), eol_marker(),
	        block_id(0), continuationRx() {
		continuationRx.setPatternSyntax(QRegExp::RegExp2);
	}
};

typedef std::pair<QRegExp,struct TextFormat> FormatRx;

class ScriptHighlighter : public QSyntaxHighlighter
{
public:
	ScriptHighlighter(QTextEdit *parent);
	virtual ~ScriptHighlighter();

protected:
	virtual void highlightBlock(const QString &text);
	bool eolCheck(const struct TextFormat &fmt, const QString &txt);

private:
	std::list<FormatRx> formats;
};
