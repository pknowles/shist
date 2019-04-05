
#include "input.h"
#include "output.h"
#include "screen.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <readline/readline.h>

enum Action {
	ACTION_NONE,
	ACTION_RESTORE_COMMAND,
	ACTION_REPLACE_COMMAND,
	ACTION_EXECUTE_SELECTION
};

bool g_done = false;
static Action g_action = ACTION_NONE;

int escape(int a, int b) {
g_action = ACTION_RESTORE_COMMAND; g_done = true; return 0;
}
int tab(int a, int b) {g_action = ACTION_REPLACE_COMMAND; g_done = true; return 0;}
int enter(int a, int b) {g_action = ACTION_EXECUTE_SELECTION; g_done = true; return 0;}
int arrow_up(int a, int b) {move_selection(1, false, false); return 0;}
int arrow_down(int a, int b) {move_selection(-1, false, false); return 0;}
int page_up(int a, int b) {move_selection(1, true, false); return 0;}
int page_down(int a, int b) {move_selection(-1, true, false); return 0;}

int main(int argc, char** argv)
{
	int status = 0;

	int ret = screen_begin();
	if (ret != 0) {
		return ret;
	}

	const char* initialPattern = "";
	int initialCursorPos = 0;
	const char* initialPatternEnv = getenv("READLINE_LINE");
	const char* initialCursorPosEnv = getenv("READLINE_POINT");
	if (initialPatternEnv && initialCursorPosEnv) {
		initialPattern = initialPatternEnv;
		initialCursorPos = strtol(initialCursorPosEnv, nullptr, 10);
	}

	rl_bind_key('\e', escape);
	rl_bind_key('\t', tab);
	rl_bind_key('\r', enter);
	rl_bind_keyseq("\\e[A", arrow_up);
	rl_bind_keyseq("\\C-r", arrow_up);
	rl_bind_keyseq("\\e[B", arrow_down);
	rl_bind_keyseq("\\C-s", arrow_up);

	readline_begin(initialPattern, initialCursorPos, pattern_changed);

	const char* lastPattern = nullptr;
	while(!g_done) {
		// This is needed so the bind for ESC works because we are still living in the dark ages.
		if (!getStreamAvailableChars(STDIN_FILENO)) {
			// TODO: Could rl_prep_terminal be an alternative to this stupid loop?
			readline_step(0);
			continue;
		}

		int c = screen_get_char();
		//int c = readline_getch();

		lastPattern = readline_step(c);
	}

	readline_end();
	screen_end();

	// Get the currently selected item from the history list
	const char* selection = screen_selection();

	// If there was no selected item, e.g. filtered history is empty,
	// use the entered pattern instead.
	if (!selection) {
		selection = lastPattern;
	}

	switch (g_action) {
	case ACTION_EXECUTE_SELECTION:
		if (selection) {
			term_replace_command(selection);
			term_execute();
			break;
		}
		status = 1;
		break;
	case ACTION_REPLACE_COMMAND:
		if (selection) {
			term_replace_command(selection);
			break;
		}
		status = 1;
		break;
	case ACTION_RESTORE_COMMAND:
		// Original should still be there. Nothing to do.
		break;
	case ACTION_NONE:
	default:
		status = 1;
		break;
	};

	return status;
}
