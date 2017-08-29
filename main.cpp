
#include "input.h"
#include "screen.h"
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
static std::string g_pattern;

int escape(int a, int b) {g_action = ACTION_RESTORE_COMMAND; g_done = true; return 0;}
int tab(int a, int b) {g_action = ACTION_REPLACE_COMMAND; g_done = true; return 0;}
int enter(int a, int b) {g_action = ACTION_EXECUTE_SELECTION; g_done = true; return 0;}
int arrow_up(int a, int b) {move_selection(1, false, true); return 0;}
int arrow_down(int a, int b) {move_selection(-1, false, true); return 0;}
int page_up(int a, int b) {move_selection(1, true, false); return 0;}
int page_down(int a, int b) {move_selection(-1, true, false); return 0;}

int main(int argc, char** argv)
{
	int status = 0;

	int ret = screen_begin();
	if (ret != 0) {
		return 1;
	}

	const char* initialPattern = getenv("READLINE_LINE");
	if (initialPattern) {
		g_pattern = initialPattern;
	}

//	rl_unbind_command_in_map("Up", rl_get_keymap());
//	rl_unbind_command_in_map("Down", rl_get_keymap());
	rl_bind_key('\e', escape);
	rl_bind_key('\t', tab);
	rl_bind_key('\n', enter);
	rl_bind_keyseq("\\e[B", arrow_up);
	rl_bind_keyseq("\\e[D", arrow_down);

	readline_begin(g_pattern.c_str(), pattern_changed);

	while(true) {
		int c = screen_get_char();

//		switch (c) {
//		case '\e':
//			g_action = ACTION_RESTORE_COMMAND;
//			done = true;
//			break;
//		case '\t':
//			g_action = ACTION_REPLACE_COMMAND;
//			done = true;
//			break;
//		case '\n':
//			g_action = ACTION_EXECUTE_SELECTION;
//			done = true;
//		default:
//			break;
//		}

		g_pattern = readline_step(c);

		if (g_done) {
			break;
		}
	}

	readline_end();
	screen_end();

	const char* selection;
	switch (g_action) {
	case ACTION_EXECUTE_SELECTION:
		selection = screen_selection();
		if (selection) {
			term_replace_command(selection);
			term_execute();
			break;
		}
		// fall through
	case ACTION_REPLACE_COMMAND:
		term_replace_command(g_pattern.c_str());
		break;
	case ACTION_RESTORE_COMMAND:
		break;
	case ACTION_NONE:
	default:
		status = 1;
		break;
	};

	return status;
}
