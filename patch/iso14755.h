#define NUMMAXLEN(x)		((int)(sizeof(x) * 2.56 + 0.5) + 1)

/* constants */
#define ISO14755CMD		"dmenu -w \"$WINDOWID\" -p codepoint: </dev/null"

void iso14755(const Arg *);