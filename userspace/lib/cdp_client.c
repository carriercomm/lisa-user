#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "cdp_client.h"

/* Initiates a cdp client session */
int cdp_init_ipc(struct cdp_session_info *s) {
	struct mq_attr attr;
	int    status = 0;

	/* Be paranoid about user input */
	assert(s);
	memset(s, 0, sizeof(struct cdp_session_info));

	/* Try to open the send message queue on which we send requests to cdpd */
	memset(s->sq_name, 0, sizeof(s->sq_name));
	snprintf(s->sq_name, sizeof(s->sq_name), CDP_QUEUE_NAME, 0);
	s->sq_name[sizeof(s->sq_name)-1] = '\0';

	if ((s->sq = mq_open(s->sq_name, O_WRONLY)) < 0)
		return s->sq;

	/* Open the message queue for receiving responses from cdpd */
	memset(s->rq_name, 0, sizeof(s->rq_name));
	snprintf(s->rq_name, sizeof(s->rq_name), CDP_QUEUE_NAME, getpid());
	s->rq_name[sizeof(s->rq_name)-1] = '\0';

	if ((s->rq = mq_open(s->rq_name, O_CREAT|O_RDONLY, 0666, NULL)) < 0)
		return s->rq;

	/* Get the max message size attribute */
	if ((status = mq_getattr(s->rq, &attr)) < 0)
		return status;

	s->max_msg_len = attr.mq_msgsize;

	/* Alloc the receive message buffer */
	if (!(s->cdp_response = malloc(s->max_msg_len)))
		return -ENOMEM;

	s->enabled = 1;

	return status;
}

/* Ends a cdp client session */
void cdp_destroy_ipc(struct cdp_session_info *s) {
	assert(s);

	if (!s->enabled)
		return;

	if (s->cdp_response)
		free(s->cdp_response);
	mq_close(s->rq);
	mq_unlink(s->rq_name);

	return;
}

/* timed receive for a message from the client queue */
int cdp_ipc_receive(struct cdp_session_info *s) {
	struct timespec ts;
	time_t ns;

	assert(s);

	memset(&ts, 0, sizeof(ts));
	time(&ns);
	ts.tv_sec = ns + CDP_CLIENT_TIMEOUT_S;

	if (mq_timedreceive(s->rq, s->cdp_response,  s->max_msg_len, NULL, &ts) < 0) {
		fprintf(stderr, "No message received from cdpd in %d seconds.\n"
			"Disabling cdp in this session.\n", CDP_CLIENT_TIMEOUT_S);
		s->enabled = 0;
		return 1;
	}

	return 0;
}
