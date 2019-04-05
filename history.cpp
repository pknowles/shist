
#include "history.h"
#include <stdio.h>
#include <readline/history.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <pwd.h>
#include <string>
#include <vector>
#include <cstring>
#include <limits>
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

	history_filter(g_pattern.c_str());

	return 0;
}

void history_filter(const char *pattern)
{
	history_set_pos(history_length); // no -1? doesn't smell right
	g_pattern = pattern;
	g_items.resize(0);
	g_itemSet.clear();
}

HistoryItem newHistoryItem(const char* line)
{
	HistoryItem item = {};
	item.line = line;

	if (g_pattern.size()) {
		size_t matches = 0;
		const char* match = std::strstr(line, g_pattern.c_str());
		while (matches < HISTORY_MAX_MATCHES && match != nullptr) {
			// Don't store matches beyond 255 bytes
			if (match + g_pattern.size() - line > std::numeric_limits<uint8_t>::max()) {
				break;
			}
			item.matches.push_back({static_cast<uint8_t>(match - line), static_cast<uint8_t>(g_pattern.size())});
			match = std::strstr(match + g_pattern.size(), g_pattern.c_str());
		}
	}

	return item;
}

void history_items(int max, int *count, const HistoryItem **items)
{
	while ((int)g_items.size() < max) {
		HIST_ENTRY *entry;
		if (g_pattern.size()) {
			if (history_search(g_pattern.c_str(), -1) < 0) {
				break;
			} else {
				entry = current_history();
				previous_history();
			}
		} else {
			entry = previous_history();
			if (!entry) {
				break;
			}
		}
		assert(entry);
		if (g_itemSet.find(entry->line) == g_itemSet.end()) {
			g_itemSet.insert(entry->line);
			g_items.push_back(newHistoryItem(entry->line));
		}
	}
	*count = (int)g_items.size();
	*items = &g_items[0];
}

void history_end()
{
}
