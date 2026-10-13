/* Spite Mail data store API
 * license: GPLv2; (C) 2026 Jody Bruchon <jody@jodybruchon.com> */


#include <stdlib.h>
#include <string.h>
#include "spitemail.h"
#include "datastore.h"


/* For now, this just returns a static data store. */

int store_init(MailContext *ctx)
{
	/* Populate accounts and folder hierarchy into application context */
	mail_context_init(ctx);
	return 0;
}


void store_cleanup(void)
{
	/* No persistent handles to close for static store */
}


void store_free_folder_messages(StoreMsgHeader *headers)
{
	if (headers) free(headers);
}


int store_fetch_folder_messages(int32_t folder_idx, StoreMsgHeader **out_headers, uint32_t *out_count)
{
	StoreMsgHeader *headers = NULL;
	uint32_t count = 0;

	if (!out_headers || !out_count) return -1;

	if (folder_idx == 0) { /* Account 0 Inbox */
		count = 2;
		headers = (StoreMsgHeader *)malloc(count * sizeof(StoreMsgHeader));
		if (!headers) return -1;

		headers[0].id = 1001;
		wcscpy_s(headers[0].subject, 128, L"Welcome to Spite Mail (RAM Store)");
		wcscpy_s(headers[0].sender, 128, L"Jody Bruchon");
		wcscpy_s(headers[0].date_str, 32, L"Tue, 6 Oct 2026");
		headers[0].timestamp = 20261006;

		headers[1].id = 1002;
		wcscpy_s(headers[1].subject, 128, L"Bend It Like Bloomberg: Micklethwait's Epic Soccer Mishap");
		wcscpy_s(headers[1].sender, 128, L"Heat Street Staff");
		wcscpy_s(headers[1].date_str, 32, L"Fri, 29 Apr 2016");
		headers[1].timestamp = 20160429;

	} else if (folder_idx == 1) { /* Account 0 Sent */
		count = 1;
		headers = (StoreMsgHeader *)malloc(count * sizeof(StoreMsgHeader));
		if (!headers) return -1;

		headers[0].id = 2001;
		wcscpy_s(headers[0].subject, 128, L"Re: Bug report on jdupes memory usage");
		wcscpy_s(headers[0].sender, 128, L"Jody Bruchon");
		wcscpy_s(headers[0].date_str, 32, L"Mon, 5 Oct 2026");
		headers[0].timestamp = 20261005;

	} else if (folder_idx == 2) { /* Account 1 Inbox */
		count = 1;
		headers = (StoreMsgHeader *)malloc(count * sizeof(StoreMsgHeader));
		if (!headers) return -1;

		headers[0].id = 3001;
		wcscpy_s(headers[0].subject, 128, L"Project Quark v0.1 released");
		wcscpy_s(headers[0].sender, 128, L"Jody Bruchon");
		wcscpy_s(headers[0].date_str, 32, L"Sun, 11 Oct 2026");
		headers[0].timestamp = 20261011;

	} else if (folder_idx == 3) { /* Account 1 Sent */
		count = 1;
		headers = (StoreMsgHeader *)malloc(count * sizeof(StoreMsgHeader));
		if (!headers) return -1;

		headers[0].id = 4001;
		wcscpy_s(headers[0].subject, 128, L"Inquiry regarding CRUX Linux package update");
		wcscpy_s(headers[0].sender, 128, L"Jody Bruchon");
		wcscpy_s(headers[0].date_str, 32, L"Sat, 10 Oct 2026");
		headers[0].timestamp = 20261010;
	}

	*out_headers = headers;
	*out_count = count;
	return 0;
}

int store_fetch_message_body(uint64_t msg_id, wchar_t **out_body_text)
{
	const wchar_t *src = NULL;
	wchar_t *body = NULL;
	size_t len;

	if (!out_body_text) return -1;

	switch (msg_id) {
		case 1001:
			src = L"From: Jody Bruchon <jody@jodybruchon.com>\r\n"
			      L"Subject: Welcome to Spite Mail\r\n\r\n"
			      L"Spite Mail is designed for speed, low overhead, and pure native Win32 controls.";
			break;
		case 1002:
			src = L"From: Heat Street Staff <news@heatstreet.com>\r\n"
			      L"Subject: Bend It Like Bloomberg\r\n\r\n"
			      L"Archived news piece regarding John Micklethwait's soccer story.";
			break;
		case 2001:
			src = L"To: Bug Reporter <reporter@example.com>\r\n"
			      L"Subject: Re: Bug report on jdupes memory usage\r\n\r\n"
			      L"Thanks for the report. Memory layout optimizations are being applied.";
			break;
		case 3001:
			src = L"From: Jody Bruchon <jody@gazingcat.com>\r\n"
			      L"Subject: Project Quark v0.1 released\r\n\r\n"
			      L"Quark v0.1 source code and initial array iterators are now live.";
			break;
		case 4001:
			src = L"To: CRUX Maintainer <maintainer@crux.nu>\r\n"
			      L"Subject: Inquiry regarding CRUX Linux package update\r\n\r\n"
			      L"Checking on the status of the latest toolchain updates for port packaging.";
			break;
		default:
			src = L"[No body content found for message]";
			break;
	}

	len = wcslen(src) + 1;
	body = (wchar_t *)malloc(len * sizeof(wchar_t));
	if (!body) return -1;

	wcscpy_s(body, len, src);
	*out_body_text = body;
	return 0;
}



void store_free_message_body(wchar_t *body_text)
{
	if (body_text) free(body_text);
}
