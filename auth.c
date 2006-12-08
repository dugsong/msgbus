
#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/nameser.h>

#include <resolv.h>
#include <string.h>

const char *
auth_parse(const char *auth)
{
	static char buf[BUFSIZ];
	const char *sender = NULL;
	char *user, *pass;
	int n;
	
	if (auth != NULL && strncasecmp(auth, "Basic ", 6) == 0) {
		if ((n = b64_pton(auth + 6, (u_char *)buf, sizeof(buf))) > 0) {
			buf[n] = '\0';
			user = buf;
			if ((pass = index(buf, ':')) != NULL) {
				*pass++ = '\0';
				sender = user;
				/* XXX - check passwd */
			}
		}
	}
	return (sender);
}

