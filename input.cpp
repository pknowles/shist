
// file modified from https://github.com/ulfalizer/readline-and-ncurses/blob/master/rlncurses.c
// from https://stackoverflow.com/a/28709979/1888983

#include "input.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <unistd.h>
#include <thread>
#include <deque>
#include <mutex>
#include <vector>

static int g_input = 0;
static redisplay_callback g_redisplay_callback;

static int g_realStdin;
static std::vector<int> g_stdinDuplicates;
static std::mutex g_stdinDuplicatesMutex;
static std::thread g_stdinProxyThread;

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

void stdin_proxy(int stdinReal, int stdinFake)
{
	std::vector<int> outFDs;
	std::vector<FILE*> outFiles;

	FILE* realStdinFile = fdopen(stdinReal, "r");
	if (!realStdinFile) {
		fprintf(stderr, "Failed to open real stdin");
		return;
	}

	FILE* fakeStdinFile = fdopen(stdinFake, "w");
	if (fakeStdinFile) {
		outFiles.push_back(fakeStdinFile);
	} else {
		fprintf(stderr, "Failed to open fake stdin");
		return;
	}

	const int bufferSize = 32;
	char buffer[bufferSize];
	while (true) {
		// FIXME: read lots at once!
		buffer[0] = getc(realStdinFile);

		{
			std::lock_guard<std::mutex> lock(g_stdinDuplicatesMutex);
			for (auto& dupe : g_stdinDuplicates) {
				FILE* outFile = fdopen(dupe, "w");
				if (outFile) {
					outFiles.push_back(outFile);
				} else {
					fprintf(stderr, "Failed to open stdin dupe %i", dupe);
				}
				outFDs.push_back(dupe);
			}
			g_stdinDuplicates.clear();
		}

		for (auto& f : outFiles) {
			putc(buffer[0], f);
		}
	}

	for (auto& f : outFiles) {
		fclose(f);
	}
	for (auto& fd : outFDs) {
		close(fd);
	}
	fclose(realStdinFile);
	fclose(fakeStdinFile);
	close(stdinReal);
	close(stdinFake);
}

void input_begin()
{
	// hook stdin. needed to keep raw input
	int stdinReal = dup(STDIN_FILENO);
	int stdinFake[2];
	pipe(stdinFake);
	dup2(stdinFake[0], STDIN_FILENO);
	close(stdinFake[0]);

	g_stdinProxyThread = std::thread(stdin_proxy, stdinReal, stdinFake[1]);
}

void input_end()
{
	// TODO: restore stdin (?), close streams and stop thread
}

int get_stdin_dupe()
{
	int stdinDupePipe[2];
	pipe(stdinDupePipe);
	std::lock_guard<std::mutex> lock(g_stdinDuplicatesMutex);
	g_stdinDuplicates.push_back(stdinDupePipe[1]);
	return stdinDupePipe[0];
}
