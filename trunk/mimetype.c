
#include <sys/types.h>
#include <sys/tree.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mimetype.h"

struct mime_entry {
	char			*ext;
	char			*type;
	RB_ENTRY(mime_entry)	 next;
};

RB_HEAD(mime_tree, mime_entry) mimetypes = RB_INITIALIZER(&mimetypes);

static int
_compare(struct mime_entry *a, struct mime_entry *b)
{
	return (strcmp(a->ext, b->ext));
}

static char *
_strlower(char *str)
{
	char *p;
	
	for (p = str; (*p = tolower(*p)); p++)
		;
	return (str);
}

RB_GENERATE(mime_tree, mime_entry, next, _compare);

void
mimetype_add(const char *ext, const char *type)
{
	struct mime_entry *tmp, *m = malloc(sizeof(*m));

	m->ext = _strlower(strdup(ext));
	m->type = _strlower(strdup(type));
	if ((tmp = RB_INSERT(mime_tree, &mimetypes, m)) != NULL) {
		free(m->ext);
		free(m->type);
		free(m);
	}
}

const char *
_mimetype_guess_file(const char *path)
{
	static char line[BUFSIZ];
	FILE *f;
	char *p, *type = NULL;

	if (snprintf(line, sizeof(line), "file -i %s", path) < sizeof(line)) {
		if ((f = popen(line, "r")) != NULL) {
			if ((p = fgets(line, sizeof(line), f)) != NULL) {
				strsep(&p, " ");
				if (strncmp(p, "cannot", 6) != 0 &&
				    strchr(p, '/') != NULL)
					type = _strlower(strsep(&p, "\n"));
			}
			fclose(f);
		}
	}
	return (type);
}

const char *
mimetype_guess(const char *path)
{
	struct mime_entry *m, find;
	const char *type = "application/octet-stream";
	char ext[32];

	if ((find.ext = strrchr(path, '.')) != NULL &&
	    strlcpy(ext, find.ext + 1, sizeof(ext)) < sizeof(ext)) {
		find.ext = _strlower(ext);
	} else
		find.ext = NULL;
	
	if ((m = RB_FIND(mime_tree, &mimetypes, &find)) != NULL) {
		type = m->type;
	} else if ((type = _mimetype_guess_file(path)) != NULL &&
	    find.ext != NULL) {
		mimetype_add(find.ext, type);
	}
	return (type);
}
