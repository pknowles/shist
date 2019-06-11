

#include "screen.h"
#include "history.h"
#include "input.h"
#include <ncurses.h>
#include <assert.h>
#include <string>
#include <memory>
#include <unistd.h>

#include <readline/readline.h>

struct LinePart {
	std::string text;
	int colour;
	int scoreMultiplier;

	enum Type {
		UNIQUE,
		START,
		END,
		MATCH,
		COMMON,
	};

	Type type;

	LinePart(const char *str, size_t start, size_t len, int partColour, Type partType)
		: text(str, start, len), colour(partColour), type(partType)
	{
	}

	int score() const
	{
		// Desirable score features:
		// - Show the matches in the string
		// - Show the start of differences between adjacent strings
		// - Show the start of the string
		// - Show the end of the string
		// - Hide long common substrings

		switch (type) {
		case UNIQUE: return text.size() * 2;
		case START: return text.size();
		case END: return text.size();
		case MATCH: return 0;
		case COMMON: return text.size() * 4;
		}

		assert(!"Invalid type");
	}

	size_t collapse(size_t numCharsToRemove)
	{
		const std::string replaceWith = "...";

		size_t contextChars = 1;
		if (text.size() > numCharsToRemove + replaceWith.size()) {
			contextChars = (text.size() - replaceWith.size() - numCharsToRemove) / 2;
		}

		const size_t minRemoveChars = replaceWith.size() + contextChars * 2;

		if (text.size() <= minRemoveChars) {
			return 0;
		}

		numCharsToRemove = std::min(numCharsToRemove, text.size() - minRemoveChars);
		text = text.substr(0, text.size() - numCharsToRemove - replaceWith.size() - contextChars) + replaceWith + text.substr(text.size() - contextChars, contextChars);
		return numCharsToRemove;
	}
};

struct LineText {
	std::vector<LinePart> parts;
	size_t size;
	int score;

	void update()
	{
		score = 0;
		size = 0;
		for (size_t i = 0; i < parts.size(); ++i) {
			size += parts[i].text.size();
			score += parts[i].score();;
		}
	}
};

Screen::Screen()
	: m_history(std::make_unique<History>())
	, m_promptLine(0)
	, m_histLineTop(0)
	, m_histLineCount(0)
	, m_histScroll(0)
	, m_prompt("$ ")
	, m_pattern("")
	, m_cursor(0)
	, m_selection(0)
{

	// start curses mode
	if(!isatty(fileno(stdout))) {
		// Handle the case when stdout has been redirected.
		// https://stackoverflow.com/questions/17450014/ncurses-program-not-working-correctly-when-used-for-command-substitution
		// https://stackoverflow.com/questions/8371877/ncurses-and-linux-pipeline
		FILE* out = stdout;
		out = fopen("/dev/tty", "w");

		// Should really test `out` to make sure that worked.
		setbuf(out, NULL);

		// Here, we don't worry about the case where stdin has been
		// redirected, but we could do something similar to out
		// for input, opening "/dev/tty" in mode "r" for in if necessary.
		m_newtermScreen = newterm(NULL, out, stdin);
	} else {
		initscr();
	}

	if (has_colors()) {
		start_color();
		use_default_colors();
		init_pair(1, COLOR_RED, -1);
	}
	cbreak();
	noecho();
	nonl();
	intrflush(NULL, FALSE);

//	keypad(stdscr, TRUE);

	onResize();
}

Screen::~Screen()
{
	endwin();

	if (m_newtermScreen) {
		delscreen(reinterpret_cast<SCREEN*>(m_newtermScreen));
	}
}

void Screen::onPostDraw()
{
	move(m_promptLine, m_prompt.size() + m_cursor);
	refresh();
}

void Screen::onResize()
{
	int top = 0;
	int bottom = LINES;

	// Reserve space for the prompt
	// Decrement bottom to shrink the remaining free lines
	m_promptLine = --bottom;

	// Use everything that's left for the history list
	m_histLineTop = top;
	m_histLineCount = bottom - top;

	drawHistory();
	drawPrompt();
	onPostDraw();
}

std::pair<size_t, size_t> longestCommonSubstring(const std::string& a, const std::string& b)
{

}

LineText fitLine(LineText line, size_t maxWidth, size_t startPart = 0)
{
	while (line.size > maxWidth) {
		LineText best = line;
		size_t charsToRemove = line.size - maxWidth;
		for (size_t i = startPart; i < line.parts.size(); ++i) {
			LineText tmp = line;
			size_t charsRemoved = tmp.parts[i].collapse(charsToRemove);
			tmp.update();
			if (tmp.score < best.score) {
				best = tmp;
			}
		}
		if (line.score == best.score) {
			// Line cannot be made any shorter
			return line;
		}
		line = best;
	}
	return line;
}

LineText makeLineFromHistory(const HistoryItem& item)
{
	LineText lineText;
	size_t lastPos = 0;
	size_t lineLen = strlen(item.line);

	// TODO: split line into common substrings

	LinePart::Type previousType = LinePart::START;
	for (auto& match : item.matches) {
		// Add bits between lastPos and each match
		lineText.parts.push_back(LinePart(item.line, lastPos, match.start - lastPos, 0, previousType));

		// Add the matches
		lineText.parts.push_back(LinePart(item.line, match.start, match.size, 1, LinePart::MATCH));
		lastPos = match.start + match.size;

		previousType = LinePart::UNIQUE;
	}

	// Add any remainder
	if (lastPos	< lineLen) {
		lineText.parts.push_back(LinePart(item.line, lastPos, lineLen - lastPos, 0, LinePart::END));
	}

	// Compute the total size etc.
	lineText.update();

	return lineText;
}

int Screen::historyItemToLine(int itemIndex)
{
	return m_histLineTop +
		m_histLineCount - (itemIndex - m_histScroll) - 1;
}

void Screen::drawHistoryItem(const HistoryItem *item, int line)
{
	move(line, 0);
	if (item) {
		size_t width = static_cast<size_t>(getmaxx(stdscr));
		bool selLine = line == historyItemToLine(m_selection);
		std::string prefix(selLine ? "> " : "  ");
		auto lineText = makeLineFromHistory(*item);
		lineText = fitLine(lineText, width - prefix.size());

		mvaddnstr(line, 0, prefix.c_str(), prefix.size());

		for (auto& part : lineText.parts) {
			attrset(COLOR_PAIR(part.colour));
			addnstr(part.text.c_str(), part.text.size());
		}

		attrset(COLOR_PAIR(0));
	}

	// Clear the rest of the line only if we didn't just wrap by writing to the last column
	if (getcury(stdscr) == line) {
		clrtoeol();
	}
}

void Screen::drawHistory()
{
	int count;
	const HistoryItem* items;

	// Request enough history to fill the screen
	int topOfScreen = m_histScroll + m_histLineCount;
	m_history->getItems(topOfScreen, &count, &items);

	int maxItem = std::min(count, topOfScreen);
	for (int i = m_histScroll; i < topOfScreen; ++i) {
		int line = historyItemToLine(i);
		drawHistoryItem(i < count ? &items[i] : NULL, line);
	}
}

void Screen::drawPrompt()
{
	mvprintw(m_promptLine, 0, "%s%s", m_prompt.c_str(), m_pattern.c_str());
	clrtoeol();
}

const char *Screen::selection()
{
	int count;
	const HistoryItem* items;
	m_history->getItems(0, &count, &items);
	if (count == 0) {
		return NULL;
	}
	assert(m_selection >= 0 && m_selection < count);
	return items[m_selection].line;
}

void Screen::setFilter(const char *pattern, int cursor)
{
	// TODO: ideally don't call this if it hasn't changed
	if (m_pattern == pattern && m_cursor == cursor) {
		return;
	}

	m_histScroll = 0;
	m_selection = 0;
	m_pattern = pattern;
	m_cursor = cursor;

	m_history->filter(pattern);
	drawHistory();
	drawPrompt();
	onPostDraw();
}

void Screen::moveSelection(int i, bool pages, bool wrap)
{
	int lastSelection = m_selection;

	// Move the selection
	m_selection += i * (pages ? m_histLineCount : 1);

	if (wrap) {
		// Wrap between top/bottom of screen
		m_selection = m_histScroll + ((m_selection - m_histScroll + m_histLineCount) % m_histLineCount);
	}

	// Request history up to the new selection. Will mostly just get cached results.
	int count;
	const HistoryItem* items;
	m_history->getItems(m_selection + 1, &count, &items);
	if (!count) {
		m_selection = 0;
		return;
	}

	// Wrap between top/bottom of history
	if (m_selection >= count) {
		assert(!wrap);
		m_selection = 0;
	}
	if (m_selection < 0) {
		// Need to request all of the history :(
		// TODO: Ideally we start searching backwards instead
		m_history->getItems(9999999, &count, &items);
		m_selection = count - 1;
	}

	if (m_selection < m_histScroll ||
		m_selection >= m_histScroll +
		m_histLineCount) {
		assert(!wrap);
		//scroll_to(m_selection);
		int margin = pages ? std::min(m_histLineCount / 2, 5) : 0;

		// Scroll must keep the selection visible
		int scrollMax = std::max(0, std::min(count - m_histLineCount, m_selection - margin));
		int scrollMin = std::min(count - 1, std::max(0, m_selection + 1 - m_histLineCount + margin));
		m_histScroll = std::min(std::max(m_histScroll, scrollMin), scrollMax);

		drawHistory();
		onPostDraw();
	} else if (lastSelection != m_selection) {
		// Optimization - only need to re-render the previous and currently selected lines
		drawHistoryItem(&items[lastSelection],
			historyItemToLine(lastSelection));
		drawHistoryItem(&items[m_selection],
			historyItemToLine(m_selection));
		onPostDraw();
	}
}

int Screen::getChar()
{
	int c;
	// Consume screen related actions first
	while (true) {
		c = wgetch(stdscr);

		switch (c) {
		case KEY_RESIZE:
			onResize();
			break;
		case KEY_PPAGE:
			moveSelection(1, true, false);
			break;
		case KEY_NPAGE:
			moveSelection(-1, true, false);
			break;
		case KEY_UP:
			moveSelection(1, false, true);
			break;
		case KEY_DOWN:
			moveSelection(-1, false, true);
			break;
		default:
			return c;
		}
	}

}

