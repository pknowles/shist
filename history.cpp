
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

History::History()
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
		throw NoHistoryException("Failed to read history file " + historyFilename);
	}
	HISTORY_STATE *historyState=history_get_history_state();

	filter(m_pattern.c_str());
}

History::~History()
{
	clear_history();
}

void History::filter(const char *pattern)
{
	history_set_pos(history_length); // no -1? doesn't smell right
	m_pattern = pattern;
	m_items.resize(0);
	m_itemSet.clear();
}

HistoryItem History::makeHistoryItem(const char* line)
{
	HistoryItem item = {};
	item.line = line;

	if (m_pattern.size()) {
		size_t matches = 0;
		const char* match = std::strstr(line, m_pattern.c_str());
		while (matches < HISTORY_MAX_MATCHES && match != nullptr) {
			// Don't store matches beyond 255 bytes
			if (match + m_pattern.size() - line > std::numeric_limits<uint8_t>::max()) {
				break;
			}
			item.matches.push_back({static_cast<uint8_t>(match - line), static_cast<uint8_t>(m_pattern.size())});
			match = std::strstr(match + m_pattern.size(), m_pattern.c_str());
		}
	}

	return item;
}

void History::getItems(int max, int *count, const HistoryItem **items)
{
	while ((int)m_items.size() < max) {
		HIST_ENTRY *entry;
		if (m_pattern.size()) {
			if (history_search(m_pattern.c_str(), -1) < 0) {
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
		if (m_itemSet.find(entry->line) == m_itemSet.end()) {
			m_itemSet.insert(entry->line);
			m_items.push_back(makeHistoryItem(entry->line));
		}
	}
	*count = (int)m_items.size();
	*items = &m_items[0];
}

