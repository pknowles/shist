
// file modified from https://github.com/ulfalizer/readline-and-ncurses/blob/master/rlncurses.c
// from https://stackoverflow.com/a/28709979/1888983

#include "input.h"
#include <assert.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <unistd.h>

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

void readline_begin(const char* initialPattern, int initialCursorPos, redisplay_callback redisplay)
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

	rl_tty_set_default_bindings(rl_get_keymap());

	rl_bind_keyseq("\\e[C", rl_forward_char);
	rl_bind_keyseq("\\e[D", rl_backward_char);
	rl_bind_keyseq("\\e[1;5C", rl_forward_word);
	rl_bind_keyseq("\\e[1;5D", rl_backward_word);
	rl_bind_keyseq("\\e[H", rl_beg_of_line);
	rl_bind_keyseq("\\e[F", rl_end_of_line);
	rl_bind_keyseq("\\eOH", rl_beg_of_line);
	rl_bind_keyseq("\\eOF", rl_end_of_line);
	rl_bind_keyseq("\\e[1~", rl_beg_of_line);
	rl_bind_keyseq("\\e[4~", rl_end_of_line);

	rl_replace_line(initialPattern, 1);
	rl_forward_char(initialCursorPos, 0);
}

const char *readline_step(int c)
{
	assert(g_input == 0);
	g_input = c;
	// NOTE: needs to be called many times without input for esc only bind to work
	rl_callback_read_char();
	assert(g_input == 0);
	return rl_line_buffer;
}

void readline_end()
{
	rl_callback_handler_remove();
}

int getStreamAvailableChars(int inputStreamFD)
{
	const useconds_t timeout = 10000;

#if defined (HAVE_SELECT)

	fd_set readfds, exceptfds;
	struct timeval timeout;

	FD_ZERO (&readfds);
	FD_ZERO (&exceptfds);
	FD_SET (tty, &readfds);
	FD_SET (tty, &exceptfds);
	timeout.tv_sec = 0;
	timeout.tv_usec = _keyboard_input_timeout;
	select(inputStreamFD + 1, &readfds, (fd_set *)NULL, &exceptfds, &timeout);
	return FD_ISSET(inputStreamFD, &readfds) ? 1 : 0;

#elif defined(FIONREAD)

	int chars_avail = 0;
	if (ioctl (inputStreamFD, FIONREAD, &chars_avail) == 0) {
		if (chars_avail == 0) {
			usleep(timeout);
		}
		return (chars_avail);
	}

#endif

  return 0;
}
