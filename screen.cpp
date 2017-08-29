

#include "screen.h"
#include "history.h"
#include <unistd.h>
#include <ncurses.h>
#include <assert.h>
#include <string>
#include <thread>
#include <deque>
#include <mutex>

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
static int g_realStdin;
static int g_myStdin[2];
static std::thread g_stdinProxyThread;
static std::deque<int> g_rawStdin;
static std::mutex g_rawStdinMutex;

void post_draw()
{
	move(g_layout.promptLine, g_prompt.size() + g_cursor);
}

int get_history_line(int item)
{
	return g_layout.histLineTop +
		g_layout.histLineCount - item - 1;
}

void draw_history_item(const HistoryItem *item, int line)
{
	if (item) {
		bool selLine = line == get_history_line(g_selection);
		mvprintw(line, 0, "%s%s", selLine ? "> " : "  ", item->line);
	} else {
		move(line, 0);
	}
	clrtoeol();
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

void stdin_proxy()
{
	FILE* realStdin = fdopen(g_realStdin, "r");
	FILE* myStdin = fdopen(g_myStdin[1], "w");
	while (true) {
		int c = getc(realStdin);
		//fprintf(stderr, "proxy %i\n", c);
		g_rawStdinMutex.lock();
		g_rawStdin.push_back(c);
		g_rawStdinMutex.unlock();
		putc(c, myStdin);
	}
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
	}
	cbreak();
	noecho();
	nonl();
	intrflush(NULL, FALSE);

//	keypad(stdscr, TRUE);

	// hook stdin. needed to keep raw input
//	pipe(g_myStdin);
//	g_realStdin = dup(STDIN_FILENO);
//	dup2(g_myStdin[0], STDIN_FILENO);
//	close(g_myStdin[0]);
//	g_stdinProxyThread = std::thread(stdin_proxy);

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

	int lastSelection = 0;

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

bool get_raw_char(int* c)
{
	std::lock_guard<std::mutex> lock(g_rawStdinMutex);
	if (g_rawStdin.size()) {
		*c = g_rawStdin[0];
		g_rawStdin.pop_front();
		return true;
	}
	return false;
}

int screen_get_char()
{
	int c;
	if (get_raw_char(&c)) {
		return c;
	}

	while (true) {
		c = wgetch(stdscr);
		//int c = rl_getc(stdin);

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
			//bool hasRaw = get_raw_char(&c);
			//assert(hasRaw);
			return c;
		}

		// sequence consumed. clear raw buffer

	}

}

void screen_end()
{
	endwin();
	history_end();
}
