/* Render to HTML. Return value is allocated dynamically. If NULL, assume
 * ENOMEM. */
char *render_markup(const char *txt, int64_t len);
