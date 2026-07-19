/* The theme-driven, multi-line prompt box: user@host, icon + name,
 * cwd, a git status line when inside a repository, and the final
 * input arrow. */
#ifndef CALM_TERM_PROMPT_H
#define CALM_TERM_PROMPT_H

#include "theme/theme.h"

/* The current working directory, home-relative (`~/...`) when inside
 * $HOME. Shared with the terminal-title OSC sequence so both format
 * the cwd identically. Caller must free() the result. */
char *display_path(void);

/* Prints the prompt box, ending on the same line as the final arrow
 * with no trailing newline (the line editor starts typing right
 * there). */
void render_prompt_box(const Theme *theme, const char *icon);

#endif /* CALM_TERM_PROMPT_H */
