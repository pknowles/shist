
#pragma once
#include <stdint.h>
#include <vector>
#include <stdexcept>
#include <unordered_set>

#define HISTORY_MAX_MATCHES 4

struct LineRange {
	uint8_t start, size;
};

struct HistoryItem {
	const char* line;
	std::vector<LineRange> matches;
};

class History {
public:
	History();
	~History();

	class NoHistoryException : public std::runtime_error {
		using std::runtime_error::runtime_error;
	};

	void filter(const char *pattern);
	void getItems(int max, int *count, const HistoryItem **items);

private:
	HistoryItem makeHistoryItem(const char* line);

	std::vector<HistoryItem> m_items;
	std::string m_pattern;
	std::unordered_set<std::string> m_itemSet;                 
};
