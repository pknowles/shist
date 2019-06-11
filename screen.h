#pragma once

#include <string>
#include <memory>

class History;
struct HistoryItem;

class Screen {
public:
	Screen();
	~Screen();
	int getChar();
	const char* selection();
	void moveSelection(int i, bool pages, bool wrap);
	void setFilter(const char* pattern, int cursor);

private:

	void onPostDraw();
	void onResize();
	int historyItemToLine(int itemIndex);
	void drawHistoryItem(const HistoryItem *item, int line);
	void drawHistory();
	void drawPrompt();

	std::unique_ptr<History> m_history;
	int m_promptLine;
	int m_histLineTop;
	int m_histLineCount;
	int m_histScroll;
	std::string m_prompt;
	std::string m_pattern;
	int m_cursor = 0;
	int m_selection = 0;
	void* m_newtermScreen = nullptr;
};

