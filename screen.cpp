

#include "screen.h"
#include "history.h"
#include "input.h"
#include <ncurses.h>
#include <assert.h>
#include <string>
#include <unistd.h>

#include <readline/readline.h>

struct WindowLayout {
	int promptLine;
	int histLineTop;
	int histLineCount;
	int histScroll;
};

static WindowLayout g_layout;
static std::string g_prompt;
static std::string g_pattern;
static int g_cursor = 0;
static int g_selection = 0;

void post_draw()
{
	move(g_layout.promptLine, g_prompt.size() + g_cursor);
	refresh();
}

int get_history_line(int item)
{
	return g_layout.histLineTop +
		g_layout.histLineCount - item - 1;
}

void draw_history_item(const HistoryItem *item, int line)
{
	move(line, 0);
	if (item) {
		size_t width = static_cast<size_t>(getmaxx(stdscr));
		bool selLine = line == get_history_line(g_selection);
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

void draw_history()
{
	int count;
	const HistoryItem* items;
	int topOfScreen = g_layout.histScroll + g_layout.histLineCount;
	history_items(topOfScreen, &count, &items);
	int maxItem = std::min(count, topOfScreen);
	for (int i = g_layout.histScroll; i < topOfScreen; ++i) {
		int line = get_history_line(i);
		draw_history_item(i < count ? &items[i] : NULL, line);
	}
}

void draw_prompt()
{
	mvprintw(g_layout.promptLine, 0, "%s%s", g_prompt.c_str(), g_pattern.c_str());
	clrtoeol();
}

void on_resize()
{
	int top = 0;
	int bottom = LINES-1;
	g_layout.promptLine = bottom--;
	g_layout.histLineTop = top;
	g_layout.histLineCount = bottom - top + 1;

	draw_history();
	draw_prompt();
	post_draw();
}

int screen_begin()
{
	int ret = history_begin();
	if (ret != 0) {
		return ret;
	}

	g_prompt = "$ ";

	g_layout = {};
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

	on_resize();
	return 0;
}

const char *screen_selection()
{
	int count;
	const HistoryItem* items;
	history_items(0, &count, &items);
	if (count == 0) {
		return NULL;
	}
	assert(g_selection >= 0 && g_selection < count);
	return items[g_selection].line;
}

void pattern_changed(const char *pattern, int cursor)
{
	// TODO: ideally don't call this if it hasn't changed
	if (g_pattern == pattern && g_cursor == cursor) {
		return;
	}

	g_layout.histScroll = 0;
	g_selection	= 0;
	history_filter(pattern);
	g_pattern = pattern;
	g_cursor = cursor;
	draw_history();
	draw_prompt();
	post_draw();
}

void move_selection(int i, bool pages, bool wrap)
{
	int count;
	const HistoryItem* items;
	history_items(0, &count, &items);

	if (!count) {
		return;
	}

	int lastSelection = g_selection;

	g_selection += i * (pages ? LINES : 1);
	if (wrap) {
		g_selection = g_selection % count;
	} else {
		g_selection = std::min(std::max(g_selection, 0), count);
	}
	if (g_selection < 0) {
		g_selection += count;
	}

	if (g_selection < g_layout.histScroll ||
		g_selection >= g_layout.histScroll +
		g_layout.histLineCount) {
		//scroll_to(g_selection);
		assert(false);
		draw_history();
		post_draw();
	} else if (lastSelection != g_selection) {
		draw_history_item(&items[lastSelection],
			get_history_line(lastSelection));
		draw_history_item(&items[g_selection],
			get_history_line(g_selection));
		post_draw();
	}
}

int screen_get_char()
{
	int c;
	// Consume screen related actions first
	while (true) {
		c = wgetch(stdscr);

		switch (c) {
		case KEY_RESIZE:
			on_resize();
			break;
		case KEY_PPAGE:
			move_selection(1, true, false);
			break;
		case KEY_NPAGE:
			move_selection(-1, true, false);
			break;
		case KEY_UP:
			move_selection(1, false, true);
			break;
		case KEY_DOWN:
			move_selection(-1, false, true);
			break;
		default:
			return c;
		}
	}

}

void screen_end()
{
	endwin();
	history_end();
}
