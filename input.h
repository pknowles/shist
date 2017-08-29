
#pragma once

typedef void (*redisplay_callback)(const char* line, int cursor);

int readline_getch();
void readline_begin(const char *initial,  redisplay_callback redisplay);
const char *readline_step(int c);
void readline_end();
void term_replace_command(const char* contents);
void term_execute();
