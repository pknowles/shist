
#pragma once

typedef void (*redisplay_callback)(const char* line, int cursor);

int readline_getch();
void readline_begin(const char* initialPattern, int initialCursorPos,  redisplay_callback redisplay);
const char *readline_step(int c);
void readline_end();
int getStreamAvailableChars(int inputStreamFD);
