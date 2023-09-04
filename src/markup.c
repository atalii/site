#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "markup.h"

static int markup_render_line(char *, int, int, const char *);

/* The HTML prior to the <title> tag's content. */
static const char *fragment_pre_title =
	"<!DOCTYPE html><html><head>"
	"<link rel =\"stylesheet\" type=\"text/css\" href=\"styles.css\"/>"
	"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\"/>"
	"<title>";

/* The HTML immediately after the <title> tag's content and before the rendered
 * markup. */
static const char *fragment_pre_text = "</title></head><body><main>";

/* The HTML concluding the document after the rendered markup. */
const char *fragment_post =
	"<p id=\"footer\">"
	"<span>(C) 2023</span>"
	"<span>:3</span>"
	"<span><a href=\"https://github.com/atalii/site/\">source</a></span>"
	"<span><a href=\"/\">home</a></span>"
	"🏳️‍⚧️</p></main></body></html>";

char *
render_markup(const char *txt, int64_t len)
{
	int64_t buffer_size = 8192; // TODO: resize on overflow
	char *buf = malloc(buffer_size);
	if (!buf) return NULL;

	/* isolate the page title */
	int title_start, title_end;
	title_start = 0;
	while (isspace(txt[++title_start]));
	title_end = title_start;
	while (txt[++title_end] != '\n');

	char *title = strndup(txt + title_start, title_end - title_start);

	/* begin rendering. TODO: don't allocate title. */
	strcpy(buf, fragment_pre_title); // rip bounds checks
	strcat(buf, title);
	strcat(buf, fragment_pre_text);
	free(title);

	/* move on to the doc. */
	int cursor = strlen(buf);
	int par_start = 0;

	for (int i = 0; i < len; i++) {
		if ((i == len - 1) || (txt[i] == '\n' && txt[i + 1] == '\n')) {
			// TODO: check for NULL or avoid allocation entirely
			char *line = strndup(&txt[par_start], i - par_start);
			cursor += markup_render_line(buf, cursor, buffer_size, line);
			free(line);

			par_start = i + 2;
			i += 2;
		}
	}

	memcpy(buf + cursor, fragment_post, strlen(fragment_post) + 1); // ouch my bounds checks
	return buf;
}

static int
markup_render_line(char *buf, int cursor, int maxlen, const char *line)
{
	int old_cursor = cursor;

	int lstart = 0;
	char head[128] = "";
	char tail[128] = "";

	char lead = line[0];
	switch (lead) {
	case '*':
		strcat(head, "<h1>");
		strcat(tail, "</h1>");
		lstart++;
		break;
	case '!':
		strcat(head, "<h2>");
		strcat(tail, "</h2>");
		lstart++;
		break;
	case '>':
		strcat(head, "<p><a href=\"");
		while (!isspace(line[++lstart]));
		strncat(head, line + 1, lstart);
		strcat(head, "\">");

		strcat(tail, "</a></p>");
		break;
	case '_':
		strcat(head, "<hr/>");
		lstart++;
		break;
	default:
		strcat(head, "<p>");
		strcat(tail, "</p>");
		break;
	}

	strcpy(buf + cursor, head);
	cursor += strlen(head);

	// we're writing c, don't expect nice algorithms from me. insert the
	// characters into memory one at a time, escaping as necessary.
	for (const char *c = line + lstart; *c != '\0'; c++) {
		switch (*c) {
		case '&':
			strcpy(buf + cursor, "&amp;");
			cursor += strlen("&amp;");
			break;
		case '<':
			strcpy(buf + cursor, "&lt;");
			cursor += strlen("&lt;");
			break;
		case '>':
			strcpy(buf + cursor, "&gt;");
			cursor += strlen("&gt;");
			break;
		case '"':
			strcpy(buf + cursor, "&quot;");
			cursor += strlen("&quot;");
			break;
		case '\'':
			strcpy(buf + cursor, "&#39;");
			cursor += strlen("&#39;");
			break;
		default:
			buf[cursor++] = *c;
		}
	}

	strcpy(buf + cursor, tail);
	cursor += strlen(tail);

	return cursor - old_cursor;
}
