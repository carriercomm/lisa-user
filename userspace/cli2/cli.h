#ifndef _CLI_H
#define _CLI_H

#include <stdio.h>
#include <sys/types.h>

struct menu_node;

#define MAX_MENU_DEPTH 64
#define TOKENIZE_MAX_MATCHES 128

#ifndef whitespace
#define whitespace(c) ((c) == ' ' || (c) == '\t')
#endif

/* Tokenizer output */
struct tokenize_out {
	/* Offset of the current token in the input buffer */
	int offset;
	/* Length of the current token */
	int len;
	/* Length of the initial token substring that would produce at least
	 * one match */
	int ok_len;
	/* Array of matching menu nodes */
	struct menu_node *matches[TOKENIZE_MAX_MATCHES+1];
	/* Exact match (token length == node name length) if any */
	struct menu_node *exact_match;
};

#define MATCHES(out) ((out)->matches[0] == NULL ? 0 : ((out)->matches[1] == NULL ? 1 : 2))

struct cli_context {
	/* Bitwise selector against menu node masks (nodes that don't match
	 * are filtered) */
	int node_filter;

	/* Additional hints about last command execution failure */
	union {
		/* Parse error offset for INVALID commands */
		int offset;

		/* Error description for REJECTED commands */
		char *reason;
	} ex_status;

	/* Current menu node root */
	struct menu_node *root;
};

/* Command tree menu node */
struct menu_node {
	/* Complete name of the menu node */
	const char *name;

	/* Help message */
	const char *help;

	/* Bitwise mask for filtering */
	int mask;

	/* Custom tokenize function for the node */
	int (*tokenize)(struct cli_context *ctx, const char *buf, struct menu_node **tree, struct tokenize_out *out);

	/* Command handler for runnable nodes; ctx is a passed-back pointer,
	 * initially sent as argument to cli_exec(); argc is the number of
	 * tokens in the command; tokv is the array of tokens (exactly as
	 * they appear in the input command); nodev is the array of matching
	 * menu nodes, starting from root.
	 */
	int (*run)(struct cli_context *ctx, int argc, char **tokv, struct menu_node **nodev);

	/* Points to the sub menu of the node */
	struct menu_node **subtree;
};

#define NULL_MENU_NODE {\
	.name		= NULL,\
	.help		= NULL,\
	.mask		= 0,\
	.tokenize	= NULL,\
	.run		= NULL,\
	.subtree	= NULL\
}

enum {
	/* Codes returned by parser */
	CLI_EX_AMBIGUOUS		= 1,
	CLI_EX_INCOMPLETE		= 2,
	CLI_EX_INVALID			= 3,

	/* Codes returned by handlers */
	CLI_EX_OK				= 0,
	CLI_EX_REJECTED			= 4,
	CLI_EX_NOTIMPL			= 5
};

int cli_next_token(const char *buf, struct tokenize_out *out);
int cli_tokenize(struct cli_context *ctx, const char *buf, struct menu_node **tree, struct tokenize_out *out);
int cli_exec(struct cli_context *ctx, char *cmd);

#endif
