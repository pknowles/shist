
#include "history.h"
#include <stdio.h>
#include <readline/history.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <pwd.h>
#include <string>
#include <vector>
#include <unordered_set>

static std::vector<HistoryItem> g_items;
static std::string g_pattern;
static std::unordered_set<std::string> g_itemSet;

int history_begin()
{
	std::string historyFilename;
	const char* histfile = getenv("HISTFILE");
	if (histfile) {
		historyFilename = histfile;
	} else {
		const char *homedir;
		if ((homedir = getenv("HOME")) == NULL) {
			homedir = getpwuid(getuid())->pw_dir;
		}
		historyFilename = std::string(homedir) + "/.bash_history";
	}

	using_history();

	if (read_history(historyFilename.c_str()) != 0) {
		fprintf(stderr, "Failed to read %s\n", historyFilename.c_str());
		return -ENOENT;
	}
	HISTORY_STATE *historyState=history_get_history_state();

	return 0;
}

void history_filter(const char *pattern)
{
	history_set_pos(0);
	g_pattern = pattern;
	g_items.resize(0);
	g_itemSet.clear();
}

void history_items(int max, int *count, const HistoryItem **items)
{
	while ((int)g_items.size() < max) {
		HIST_ENTRY *entry;
		if (g_pattern.size()) {
			if (history_search(g_pattern.c_str(), 0) < 0) {
				break;
			} else {
				entry = current_history();
				next_history();
			}
		} else {
			entry = next_history();
			if (!entry) {
				break;
			}
		}
		assert(entry);
		if (g_itemSet.find(entry->line) == g_itemSet.end()) {
			HistoryItem item;
			item.line = entry->line;
			g_itemSet.insert(entry->line);
			g_items.push_back(item);
		}
	}
	*count = (int)g_items.size();
	*items = &g_items[0];
}

void history_end()
{
}
