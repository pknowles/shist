
#pragma once

int screen_begin();
int screen_get_char();
const char *screen_selection();
void move_selection(int i, bool pages, bool wrap);
void pattern_changed(const char* pattern, int cursor);
void screen_end();
