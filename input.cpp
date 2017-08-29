
// file modified from https://github.com/ulfalizer/readline-and-ncurses/blob/master/rlncurses.c
// from https://stackoverflow.com/a/28709979/1888983

#include "input.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <readline/history.h>
#include <readline/readline.h>

static int g_input = 0;
static redisplay_callback g_redisplay_callback;

int readline_getch()
{
	return rl_getc(stdin);
}

static void dummy_select(char* line)
{
}

static int readline_input_avail(void)
{
	return g_input != 0;
}

static int readline_getc(FILE *dummy)
{
	int tmp = g_input;
	g_input = 0;
	return tmp;
}

static void redisplay_proxy()
{
	g_redisplay_callback(rl_line_buffer, rl_point);
}

void readline_begin(const char* initial, redisplay_callback redisplay)
{
	g_redisplay_callback = redisplay;

	rl_catch_signals = 0;
	rl_catch_sigwinch = 0;
	rl_deprep_term_function = NULL;
	rl_prep_term_function = NULL;
	rl_change_environment = 0;

	rl_getc_function = readline_getc;
	rl_input_available_hook = readline_input_avail;
	rl_redisplay_function = redisplay_proxy;

	rl_callback_handler_install("", dummy_select);

	rl_replace_line(initial, 1);
}

const char *readline_step(int c)
{
	g_input = c;
	rl_callback_read_char();
	return rl_line_buffer;
}

void readline_end()
{
	rl_callback_handler_remove();
}

static inline void term_send(const char *str)
{
	for (const char* c = str; *c != '\0'; ++c) {
		ioctl(0, TIOCSTI, c);
	}
}

void term_replace_command(const char *contents)
{
	// ^A - cursor to beginning of line
	// ^K - clear rest of line
	const char clear_line[] = {1, 13, 0};
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
