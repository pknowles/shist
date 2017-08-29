
#pragma once
#include <stdint.h>

struct HistoryItem {
	const char* line;
	uint8_t matches[4];
};

int history_begin();
void history_filter(const char* pattern);
void history_items(int max, int* count, const HistoryItem **items);
void history_end();
