
#include "input.h"
#include "output.h"
#include "screen.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <iostream>
#include <string>
#include <readline/readline.h>
#include <execinfo.h>
#include <signal.h>
#include <assert.h>
#include <cxxopts.hpp>

std::unique_ptr<Screen> gScreen;

enum Action {
	ACTION_NONE,
	ACTION_RESTORE_COMMAND,
	ACTION_REPLACE_COMMAND,
	ACTION_EXECUTE_SELECTION
};

bool g_done = false;
int g_lastCursor = 0;
static Action g_action = ACTION_NONE;

int escape(int a, int b) {g_action = ACTION_RESTORE_COMMAND; g_done = true; return 0;}
int tab(int a, int b) {g_action = ACTION_REPLACE_COMMAND; g_done = true; return 0;}
int enter(int a, int b) {g_action = ACTION_EXECUTE_SELECTION; g_done = true; return 0;}
int arrow_up(int a, int b) {gScreen->moveSelection(1, false, false); return 0;}
int arrow_down(int a, int b) {gScreen->moveSelection(-1, false, false); return 0;}
int page_up(int a, int b) {gScreen->moveSelection(1, true, false); return 0;}
int page_down(int a, int b) {gScreen->moveSelection(-1, true, false); return 0;}
void pattern_changed(const char* pattern, int cursor) {gScreen->setFilter(pattern, cursor); g_lastCursor = cursor;}

int printBindCommand(std::string exe, std::string shell, bool iocsti)
{
	if (shell == "bash") {
		if (iocsti) {
			std::cout << "bind -x '\"\\C-r\":\"" << exe << "\"'" << std::endl;
		} else {
			std::cout << "bind '\"\\C-r\": \"\\ev\\eb\"' ;"
				<< " bind -x '\"\\ev\": eval \"$(" << exe << " --iocsti=false)\" ; $SHIST_BIND'" << std::endl;
		}
	} else {
		std::cerr << "Unsupported shell" << std::endl;
		return 1;
	}
	return 0;
}

// Print a backtrace when compiled with debug mode
void segfault_handler(int sig) {
	// Exit ncurses and restore the terminal screen.
	gScreen.reset();

	const int maxStack = 64;
	void *array[maxStack];
	size_t size;

	// get void*'s for all entries on the stack
	size = backtrace(array, maxStack);

	// print out all the frames to stderr
	fprintf(stderr, "Error: signal %d:\n", sig);
	backtrace_symbols_fd(array, size, STDERR_FILENO);
	exit(1);
}

std::string getExePath(std::string failString)
{
	char buff[PATH_MAX];
	ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
	if (len != -1) {
		buff[len] = '\0';
		return buff;
	}
	return failString;
}

int main(int argc, char** argv)
{
#if !NDEBUG
	signal(SIGSEGV, segfault_handler);
#endif

	bool iocsti = false;

	cxxopts::Options options("shist", "Shell history selector - a replacement for standard reverse search.");
	try {
		options.add_options()
			("h,help", "Prints the usage.")
			("iocsti", "Use TIOCSTI to inject commands into the shell", cxxopts::value<bool>()->default_value("true"))
			("b,bind", "Print bind replacement command for the given shell.", cxxopts::value<std::string>()->implicit_value("bash"))
		;
		auto result = options.parse(argc, argv);

		if (result["help"].as<bool>()) {
			std::cout << options.help();
			return 0;
		}

		iocsti = result["iocsti"].as<bool>();

		if (result["bind"].count()) {
			auto bindCommandShell = result["bind"].as<std::string>();
			assert(argc > 0);
			return printBindCommand(getExePath(argv[0]), bindCommandShell, iocsti);
		}
	} catch (cxxopts::OptionException e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}

	try {
		gScreen = std::make_unique<Screen>();
	} catch (std::runtime_error err) {
		std::cerr << err.what() << std::endl;
		return 2;
	}

	int status = 0;

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
	rl_bind_keyseq("\\e[5~", page_up);
	rl_bind_keyseq("\\e[6~", page_down);

	readline_begin(initialPattern, initialCursorPos, pattern_changed);

	const char* lastPattern = nullptr;
	while(!g_done) {
		// This is needed so the bind for ESC works because we are still living in the dark ages.
		if (!getStreamAvailableChars(STDIN_FILENO)) {
			// TODO: Could rl_prep_terminal be an alternative to this stupid loop?
			readline_step(0);
			continue;
		}

		int c = gScreen->getChar();
		//int c = readline_getch();

		lastPattern = readline_step(c);
	}

	readline_end();

	// Get the currently selected item from the history list
	std::string selection = gScreen->selection() ? gScreen->selection() : "";
	gScreen.reset();

	// If there was no selected item, e.g. filtered history is empty,
	// use the entered pattern instead.
	if (!selection.size()) {
		selection = lastPattern;
		g_lastCursor = selection.size();
	}

	switch (g_action) {
	case ACTION_EXECUTE_SELECTION:
		if (selection.size()) {
			if (iocsti) {
				term_replace_command(selection.c_str());
				term_execute();
			} else {
				printf("READLINE_POINT=%i READLINE_LINE=\"%s\" SHIST_BIND=\"bind '\\\"\\eb\\\":\\\"\\C-j\\\"'\"", g_lastCursor, selection.c_str());
				//printf("%s", selection);
			}
			break;
		}
		status = 1;
		break;
	case ACTION_REPLACE_COMMAND:
		if (selection.size()) {
			if (iocsti) {
				term_replace_command(selection.c_str());
			} else {
				//printf("SHIST_BIND=\"bind '\\\"\\C-b\\\":\\\"\\C-j\\\"'\"");
				printf("READLINE_POINT=%i READLINE_LINE=\"%s\" SHIST_BIND=\"bind '\\\"\\eb\\\":\\\"here\\\"'\"", g_lastCursor, selection.c_str());
			}
			break;
		}
		status = 1;
		break;
	case ACTION_RESTORE_COMMAND:
		// Original should still be there. Nothing to do.
		// NOTE: This is lost without the iocsti injection method.
		break;
	case ACTION_NONE:
	default:
		status = 1;
		break;
	};

	return status;
}
