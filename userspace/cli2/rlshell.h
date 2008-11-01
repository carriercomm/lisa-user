#ifndef _RLSHELL_H
#define _RLSHELL_H

#include "cli.h"

struct rlshell_context {
	/* Need to pass this to underlaying CLI Parser. This must be the
	 * first member,because upper layers must be able to directly cast
	 * a "struct cli_context *" into a "struct rlshell_context *".
	 *
	 * Explanation: since the CLI Parser is not readline-aware, it
	 * cannot pass "struct rlshell_context *" to command handlers. But
	 * command handlers are implemented in upper layers, which are
	 * readline-aware and therefore might need to access the whole
	 * rlshell_context structure.
	 */
	struct cli_context cc;

	/* Callback to build prompt string. Returned buffer must be
	 * alloc'ed malloc()-style and is automatically free()'ed by the
	 * rlshell functions.
	 */
	char *(*prompt)(struct rlshell_context *ctx);

	/* Exit condition from the shell main loop. Must be initialized to
	 * 0 and set to 1 to break the shell loop.
	 */
	int exit;

	/* Prompt length; used to correctly compute the offset to display
	 * the '^' marker on invalid commands. */
	int plen;
};

int rlshell_main(struct rlshell_context *ctx);

#define MAX_MENU_DEPTH 64

#endif
