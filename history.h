
#pragma once
#include <stdint.h>
#include <vector>

#define HISTORY_MAX_MATCHES 4

struct LineRange {
	uint8_t start, size;
};

struct HistoryItem {
	const char* line;
	std::vector<LineRange> matches;
};

int history_begin();
void history_filter(const char* pattern);
void history_items(int max, int* count, const HistoryItem **items);
void history_end();
