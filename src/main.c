#include <archive.h>
#include <archive_entry.h>

#include "mongoose.h"

#define MAX_ROUTES 1024

struct response_body {
	char *mime_type;
	char *text;

	// a len < 0 indicates that text is null-terminated. otherwise, text is
	// arbitrary data and may contain a null pointer. the allocation is len
	// bytes long.
	ssize_t len;
};

/* Do a bit of a static slab-allocation for routes. Also, we just do a linear
 * search each time we want to find a route. ouch, i know. */
struct {
	int counter;
	struct {
		char *route;
		struct response_body body;
	} routes[MAX_ROUTES];
} routes = {
	.counter = 0,
};

static const struct response_body *
get_route(const char *ref)
{
	int len = strlen(ref);
	while (len > 0 && isspace(ref[len - 1]))
		len--;

	for (int i = 0; i < routes.counter; i++) {
		const char *candidate = routes.routes[i].route;

		if (strlen(candidate) != len)
			continue;

		if (strncmp(ref, candidate, len) == 0)
			return &routes.routes[i].body;
	}

	return NULL;
}

int
markup_render_line(char *buf, int cursor, int maxlen, const char *line)
{
	int old_cursor = cursor;
	char lead = line[0];

	int lstart = 0;
	char head[128] = "";
	char tail[128] = "";

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

static char *
render_markup(const char *txt, int64_t len)
{
	// parse my quirky markup language, which is sort of like gemtext but
	// without the resemblance to markdown.

	int64_t buffer_size = 8192; // TODO: resize the buffer if additions would overflow
	char *buf = malloc(buffer_size);
	if (!buf) return NULL; // TODO: error out properly

	/* isolate the page title. */
	int title_start, title_end;
	title_start = 0;
	while (isspace(txt[++title_start]));
	title_end = title_start;
	while (txt[++title_end] != '\n');

	const char *head_pre =
		"<!DOCTYPE html><html><head>"
		"<link rel=\"stylesheet\" type=\"text/css\" href=\"/styles.css\"/>"
		"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\"/>"
		"<title>";
	
	const char *head_post =
		"</title></head><body><main>";

	const char *tail =
		"<p id=\"footer\">"
		"<span>(C) 2023</span>"
		"<span>:3</span>"
		"<span><a href=\"https://github.com/atalii/site/\">source</a></span>"
		"<span><a href=\"/\">home</a></span>"
		"🏳️‍⚧️</p></main></body></html>";

	char *title = strndup(txt + title_start, title_end - title_start);
	strcpy(buf, head_pre); // rip bounds checks
	strcat(buf, title);
	strcat(buf, head_post);
	free(title);

	int cursor = strlen(buf);
	int par_start = 0;

	for (int i = 0; i < len; i++) {
		if ((i == len - 1) || (txt[i] == '\n' && txt[i + 1] == '\n')) {
			char *line = strndup(&txt[par_start], i - par_start);
			cursor += markup_render_line(buf, cursor, buffer_size, line);
			free(line);

			par_start = i + 2;
			i += 2;
		}
	}

	memcpy(buf + cursor, tail, strlen(tail) + 1); // also rip bounds checks
	return buf;
}

static void
hnd(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
	if (ev != MG_EV_HTTP_MSG) return;

	struct mg_http_message *hm = (struct mg_http_message *) ev_data;

	// TODO: verify that the method is GET

	char *rqpath = strndup(hm->uri.ptr + (hm->uri.ptr[0] == '/'), hm->uri.len);

	const struct response_body *resp = get_route(rqpath);
	if (!resp) {
		mg_http_reply(
			c, 404,
			"Content-Type: text/html; charset=utf-8\r\n",
			"%s", get_route("404")->text);

		goto cleanup;
	}

	if (resp->len < 0) {
		int clen = sizeof("Content-Type: \r\n") + strlen(resp->mime_type);
		char *cbuf = malloc(clen);
		strcpy(cbuf, "Content-Type: ");
		strcat(cbuf, resp->mime_type);
		strcat(cbuf, "\r\n");

		mg_http_reply(c, 200, cbuf, "%s", resp->text);
		free(cbuf);
	} else {
		mg_printf(c,
			"HTTP/1.1 200 OK\r\n"
			"Transfer-Encoding: chunked\r\n"
			"Content-Type: %s\r\n\n",
			resp->mime_type);

		mg_http_write_chunk(c, resp->text, resp->len);
		mg_http_write_chunk(c, "", 0);
	}

cleanup:
	free(rqpath);
}

static void
serve(void)
{
	struct mg_mgr mgr;
	mg_mgr_init(&mgr);

	mg_http_listen(&mgr, "http://0.0.0.0:8080", hnd, NULL);
	printf("listening on 0.0.0.0:8080\n");

	for (;;)
		mg_mgr_poll(&mgr, 1000);
}

static void
set_route(const char *route, const char *block, int64_t len)
{
	int index = routes.counter++;

	if (index >= MAX_ROUTES)
		return; // TODO: error out
	
	int is_css = strlen(route) > 4
		&& strcmp(route + strlen(route) - 4, ".css") == 0;

	int is_ttf = strlen(route) > 4
		&& strcmp(route + strlen(route) - 4, ".ttf") == 0;
	
	// for now, all routes return utf-8.
	routes.routes[index].route = strdup(route);
	routes.routes[index].body.len = -1;
	routes.routes[index].body.mime_type = is_css
		? "text/css; charset=utf-8"
		: is_ttf
			? "font/ttf"
			: "text/html; charset=utf-8";

	// ttf is the only binary data type we have to deal with.
	if (!is_ttf) {
		routes.routes[index].body.text = is_css
			? strndup(block, len)
			: render_markup(block, len);
	} else {
		char *dup = malloc(len);
		memcpy(dup, block, len);
		routes.routes[index].body.text = dup;
		routes.routes[index].body.len = len;
	}
	
	if (strcmp(route, "index") == 0) set_route("", block, len);
}

static int
read_zip_for_routes(void)
{
	int r;

	struct archive *a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_zip(a);

	r = archive_read_open_filename(a, "/proc/self/exe", 10240);
	if (r != ARCHIVE_OK) {
		printf("read_zip_for_routes: archive_read_open_filename: %d\n", r);
		return 1;
	}

	struct archive_entry *aent;
	for (;;) {
		r = archive_read_next_header(a, &aent);

		if (r == ARCHIVE_EOF) break;
		if (r != ARCHIVE_OK) {
			printf("read_zip_for_routes: archive_read_next_header: %d\n", r);
			return 1;
		}

		const char *pathname = archive_entry_pathname(aent);
		int64_t len = archive_entry_size(aent);

		char *buf = malloc(len);
		if (!buf) {
			printf("read_zip_for_routes: OOM\n");
			return 1;
		}

		archive_read_data(a, buf, len); // TODO: check for errs
		set_route(pathname, buf, len);
		free(buf);
	}

	archive_read_free(a);
	return 0;
}

int
main(int argc, char **argv)
{
	if (read_zip_for_routes() != 0)
		return 1;

	serve();
	return 0;
}
