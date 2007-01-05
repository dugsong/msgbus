/* $Id$ */

#include <sys/types.h>
#include <sys/time.h>

#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <curl.h>

int got_alarm;

static void
usage(void)
{
	fprintf(stderr, "usage: test-curl [-c channel] [-l address] [-p port] "
	    "[-r msgs_per_sec] [-s]\n");
	exit(1);
}

static void
handle_alarm(int sig)
{
	got_alarm = 1;
}

int
main(int argc, char *argv[])
{
	CURL *curl;
	struct curl_slist *hdrs;
	char *server = "localhost", *channel = "flood";
	int c, i, port = 8888, use_ssl = 0, msg_cnt = 0;
	char msg[128], url[BUFSIZ], errbuf[CURL_ERROR_SIZE];
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

	snprintf(url, sizeof(url), "%s://%s:%d/msgbus/%s",
	    use_ssl ? "https" : "http", server, port, channel);
	hdrs = curl_slist_append(NULL, "Content-Type: text/html");
	
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

	signal(SIGALRM, handle_alarm);
	alarm(5);
	
	for (i = 0; ; i++) {
		if (got_alarm) {
			printf("%.1f msgs/s\n", (float)msg_cnt / 5);
			got_alarm = msg_cnt = 0;
			alarm(5);
		} else {
			snprintf(msg, sizeof(msg), "hello world %d!\n", i);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, msg);
			if (curl_easy_perform(curl) != 0)
				warnx("%s", errbuf);
			msg_cnt++;
			usleep(usecs);
		}
	}
	exit(0);
}
