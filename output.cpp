
#include "input.h"
#include <sys/ioctl.h>

static inline void term_send(const char *str)
{
	for (const char* c = str; *c != '\0'; ++c) {
		ioctl(0, TIOCSTI, c);
	}
}

void term_replace_command(const char *contents)
{
	// ansi escape sequences (?)
	// http://www.expandinghead.net/keycode.html
	// http://jkorpela.fi/chars/c0.html
	// https://stackoverflow.com/questions/1508490/erase-the-current-printed-console-line
	// https://unix.stackexchange.com/questions/76566/where-do-i-find-a-list-of-terminal-key-codes-to-remap-shortcuts-in-bash
	// Just had to use a debugger and see what constants to use here.
	// ^A - cursor to beginning of line
	// ^K - clear rest of line
	const char clear_line[] = {1, 11, 0};
	term_send(clear_line);

	// print string
	term_send(contents);
}

void term_execute()
{
	// ^J - run the command
	const char execute[] = {10, 0};
	term_send(execute);
}
