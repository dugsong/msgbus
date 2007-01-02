/* $Id$ */

#include <sys/types.h>
#include <sys/time.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <curl.h>

static void
usage(void)
{
	fprintf(stderr, "usage: test-curl [-c channel] [-l address] [-p port] "
	    "[-r msgs_per_sec] [-s]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	CURL *curl;
	struct curl_slist *hdrs;
	char *server = "localhost", *channel = "flood";
	int c, i, port = 8888, use_ssl = 0;
	char buf[BUFSIZ], errbuf[CURL_ERROR_SIZE];
	useconds_t usecs = 1000000;


	while ((c = getopt(argc, argv, "c:l:p:r:sh?")) != -1) {
		switch (c) {
		case 'c':
			channel = optarg;
			break;
		case 'l':
			server = optarg;
			break;
		case 'p':
			port = atoi(argv[1]);
			break;
		case 'r':
			usecs = 1000000 / atoi(optarg);
			break;
		case 's':
			use_ssl = 1;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		usage();

	if ((curl = curl_easy_init()) == NULL)
		errx(1, "curl_easy_init");

	snprintf(buf, sizeof(buf), "%s://%s:%d/msgbus/%s",
	    use_ssl ? "https" : "http", server, port, channel);
	hdrs = curl_slist_append(NULL, "Content-Type: text/html");
	
	curl_easy_setopt(curl, CURLOPT_URL, buf);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
	
	for (i = 0; ; i++) {
		snprintf(buf, sizeof(buf), "hello world %d!\n", i);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buf);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
		if (curl_easy_perform(curl) != 0)
			warnx("%s", errbuf);
		usleep(usecs);
	}
	exit(0);
}
