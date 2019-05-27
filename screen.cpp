

#include "screen.h"
#include "history.h"
#include "input.h"
#include <ncurses.h>
#include <assert.h>
#include <string>
#include <memory>
#include <unistd.h>

#include <readline/readline.h>

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
	initscr();

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
		std::string margin(selLine ? "> " : "  ");
		std::string str = margin + item->line;
		//mvprintw(line, 0, "%s%s", selLine ? "> " : "  ", item->line);
		size_t lastMatch = 0;
		for (auto& match : item->matches) {
			size_t matchStart = static_cast<size_t>(match.start) + margin.size();
			mvaddnstr(line, lastMatch, str.substr(lastMatch).c_str(), std::max(static_cast<size_t>(0), std::min(width, matchStart) - lastMatch));
			attrset(COLOR_PAIR(1));
			mvaddnstr(line, matchStart, str.substr(matchStart).c_str(), std::max(static_cast<size_t>(0), std::min(width, matchStart + static_cast<size_t>(match.size)) - matchStart));
			attrset(A_NORMAL);
			lastMatch = matchStart + match.size;
		}
		mvaddnstr(line, lastMatch, str.substr(lastMatch).c_str(), std::min(str.size() - lastMatch, width));
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
	m_selection += i * (pages ? LINES : 1);

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
		m_histScroll = std::min(std::max(m_histScroll, m_selection + 1 - m_histLineCount), m_selection);
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

