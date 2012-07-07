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

#include "scripthighlighter.h"

static QStringList reserved_words = QStringList()
        << "abstract"
        << "boolean" << "break" << "byte"
        << "case" << "catch" << "char" << "class" << "const" << "continue"
        << "debugger" << "default" << "delete" << "do" << "double"
        << "else" << "enum" << "export" << "extends" << "false"
        << "final" << "finally" << "float" << "for" << "function"
        << "goto"
        << "if" << "implements" << "import" << "in" << "instanceof" << "intinterface"
        << "long"
        << "native" << "new" << "null"
        << "package" << "private" << "protected" << "public"
        << "return"
        << "short" << "static" << "super" << "switch" << "synchronized"
        << "this" << "throw" << "throws" << "transient" << "true" << "try" << "typeof"
        << "var" << "volatile" << "void"
        << "while" << "with";

static QStringList lung_vars = QStringList()
        << "gen" << "vessel_idx" << "n_gen"
        // Vessel Parameters
        << "R" << "a" << "b" << "c" << "tone"
        << "GP" << "Ppl" << "Ptp" << "Kz"
        << "perivascular_press_a" << "perivascular_press_b" << "perivascular_press_c"
        << "Alpha" << "Ho"
        << "D"
        // Model parameters
        << "Lung_Ht" << "Flow" << "LAP" << "Pal" << "Ppl" << "Ptp" << "PAP"
        << "Tlrns" << "Pat_Ht" << "Pat_Wt";

ScriptHighlighter::ScriptHighlighter(QTextEdit *parent)
        : QSyntaxHighlighter(parent)
{
	/* TODO: for better syntax highlighting, we would need a lexical matcher
	 * instead of using regular expression
	 */

	// reserved keywords
	struct TextFormat f;
	f = TextFormat();
	f.fmt.setForeground(Qt::blue);
	f.is_multiline = false;
	f.eol_marker.clear();
	f.block_id = 0;
	formats.push_back(FormatRx(QRegExp("\\b(" + reserved_words.join("|") + ")\\b",
	                                   Qt::CaseSensitive, QRegExp::RegExp2), f));

	// properties set to the vessel
	f = TextFormat();
	f.fmt.setForeground(Qt::darkBlue);
	f.fmt.setFontWeight(QFont::Bold);
	f.is_multiline = false;
	f.eol_marker.clear();
	f.block_id = 0;
	formats.push_back(FormatRx(QRegExp("\\b(" + lung_vars.join("|") + ")\\b",
	                                   Qt::CaseSensitive, QRegExp::RegExp2), f));

	// string matching
	f = TextFormat();
	f.fmt.setForeground(Qt::darkGreen);
	f.is_multiline = true;
	f.eol_marker = "\"";
	f.block_id = 1;
	f.continuationRx = QRegExp("([^\\\"]|\\\\\\\")*(\\\")?");
	formats.push_back(FormatRx(QRegExp("\\\"([^\\\"]|\\\\\\\")*(\\\")?",
	                                   Qt::CaseSensitive, QRegExp::RegExp2), f));

	// comment matching
	f = TextFormat();
	f.fmt.setForeground(Qt::gray);
	f.is_multiline = false;
	f.eol_marker.clear();
	f.block_id = 0;
	formats.push_back(FormatRx(QRegExp("//.*$",
	                                   Qt::CaseSensitive, QRegExp::RegExp2), f));

	// multiline comment matching
	f = TextFormat();
	f.fmt.setForeground(Qt::gray);
	f.is_multiline = true;
	f.eol_marker = "*/";
	f.block_id = 2;
	f.continuationRx = QRegExp("(.(?!\\*/))*.{0,1}(\\*/)?");
	formats.push_back(FormatRx(QRegExp("/\\*(.(?!\\*/))*.{0,1}(\\*/)?",
	                                   Qt::CaseSensitive, QRegExp::RegExp2), f));
}

ScriptHighlighter::~ScriptHighlighter()
{

}

void ScriptHighlighter::highlightBlock(const QString &text)
{
	const int len = text.length();
	const int prev_block_state = previousBlockState();
	int start_pos = 0;

	if (len==0) {
		setCurrentBlockState(prev_block_state);
		return;
	}

	if (prev_block_state > 0) {
		for (std::list<FormatRx>::const_iterator i=formats.begin();
		     i!=formats.end();
		     ++i) {

			if (i->second.block_id!=prev_block_state)
				continue;

			const QRegExp &rx = i->second.continuationRx;
			start_pos = rx.indexIn(text);

			if (start_pos==0) {
				setFormat(0 ,rx.matchedLength(), i->second.fmt);
				start_pos += rx.matchedLength();

				// continued to next block?
				if (start_pos==len && eolCheck(i->second, text))
					return;
			}
		}
	}

	for (std::list<FormatRx>::const_iterator i=formats.begin(); i!=formats.end(); ++i) {
		const QRegExp &rx = i->first;
		const struct TextFormat &f = i->second;
		int pos = start_pos;

		while (pos<len && (pos=rx.indexIn(text, pos))>=0) {
			setFormat(pos, rx.matchedLength(), f.fmt);
			pos += rx.matchedLength();
		}

		if (pos==len && len>0)
			eolCheck(f, text);
	}
}

bool ScriptHighlighter::eolCheck(const TextFormat &fmt, const QString &txt)
{
	if (!fmt.is_multiline)
		return false;

	if (txt.endsWith(fmt.eol_marker)) {
		setCurrentBlockState(-1);
		return false;
	}

	setCurrentBlockState(fmt.block_id);
	return true;
}
