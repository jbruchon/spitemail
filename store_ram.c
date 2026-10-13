/* Spite Mail data store - RAM-only/temporary dummy data store
 * license: GPLv2; (C) 2026 Jody Bruchon <jody@jodybruchon.com> */

/* Spite Mail RAM storage backend driver */
#include <stdlib.h>
#include <string.h>
#include "spitemail.h"
#include "datastore.h"
#include "store_ram.h"

typedef struct RamRecord {
	uint64_t id;
	int32_t  folder_idx;
	uint32_t mbox_offset;
	uint32_t mbox_length;
	wchar_t  subject[128];
	wchar_t  from_addr[128];
	wchar_t  to_addr[128];
	wchar_t  date_str[32];
	uint32_t timestamp;
	wchar_t  eml_blob[1024]; /* Combined EML format: headers + body */
} RamRecord;

static const RamRecord ram_db[] = {
	{
		1001, 0, 0, 450,
		L"Welcome to Spite Mail (RAM Store)",
		L"Jody Bruchon <jody@jodybruchon.com>",
		L"root@spitemail.com",
		L"Tue, 6 Oct 2026", 20261006,
		L"From: Jody Bruchon <jody@jodybruchon.com>\r\n"
		L"To: root@spitemail.com\r\n"
		L"Subject: Welcome to Spite Mail\r\n"
		L"Date: Tue, 6 Oct 2026\r\n\r\n"
		L"Spite Mail is designed for speed, low overhead, and pure native Win32 controls."
	},
	{
		1002, 0, 451, 320,
		L"Bend It Like Bloomberg: Micklethwait's Epic Soccer Mishap",
		L"Heat Street Staff <news@heatstreet.com>",
		L"root@spitemail.com",
		L"Fri, 29 Apr 2016", 20160429,
		L"From: Heat Street Staff <news@heatstreet.com>\r\n"
		L"To: root@spitemail.com\r\n"
		L"Subject: Bend It Like Bloomberg\r\n"
		L"Date: Fri, 29 Apr 2016\r\n\r\n"
		L"Archived news piece regarding John Micklethwait's soccer story."
	},
	{
		2001, 1, 772, 280,
		L"Re: Bug report on jdupes memory usage",
		L"root@spitemail.com",
		L"Bug Reporter <reporter@example.com>",
		L"Mon, 5 Oct 2026", 20261005,
		L"From: root@spitemail.com\r\n"
		L"To: Bug Reporter <reporter@example.com>\r\n"
		L"Subject: Re: Bug report on jdupes memory usage\r\n"
		L"Date: Mon, 5 Oct 2026\r\n\r\n"
		L"Thanks for the report. Memory layout optimizations are being applied."
	},
	{
		3001, 2, 1052, 260,
		L"Project Quark v0.1 released",
		L"Jody Bruchon <jody@gazingcat.com>",
		L"jody@gazingcat.com",
		L"Sun, 11 Oct 2026", 20261011,
		L"From: Jody Bruchon <jody@gazingcat.com>\r\n"
		L"To: jody@gazingcat.com\r\n"
		L"Subject: Project Quark v0.1 released\r\n"
		L"Date: Sun, 11 Oct 2026\r\n\r\n"
		L"Quark v0.1 source code and initial array iterators are now live."
	},
	{
		4001, 3, 1312, 290,
		L"Inquiry regarding CRUX Linux package update",
		L"jody@gazingcat.com",
		L"CRUX Maintainer <maintainer@crux.nu>, Dev Team <dev@crux.nu>",
		L"Sat, 10 Oct 2026", 20261010,
		L"From: jody@gazingcat.com\r\n"
		L"To: CRUX Maintainer <maintainer@crux.nu>, Dev Team <dev@crux.nu>\r\n"
		L"Subject: Inquiry regarding CRUX Linux package update\r\n"
		L"Date: Sat, 10 Oct 2026\r\n\r\n"
		L"Checking on the status of the latest toolchain updates for port packaging."
	}
};

static const size_t ram_db_count = sizeof(ram_db) / sizeof(ram_db[0]);


int store_ram_init(MailContext *ctx)
{
	mail_context_init(ctx);
	return 0;
}


void store_ram_cleanup(void)
{
	/* Nothing persistent to close */
}


int store_ram_fetch_folder_messages(int32_t folder_idx, StoreMsgHeader **out_headers, uint32_t *out_count)
{
	return store_ram_fetch_message_range(folder_idx, 0, 0, out_headers, out_count);
}


void store_ram_free_folder_messages(StoreMsgHeader *headers)
{
	if (headers) free(headers);
}


int store_ram_fetch_message_range(int32_t folder_idx, uint64_t start_id, uint32_t max_count, StoreMsgHeader **out_headers, uint32_t *out_count)
{
	StoreMsgHeader *headers = NULL;
	uint32_t count = 0;
	size_t i;
	int is_sent_folder;

	if (!out_headers || !out_count) return -1;

	is_sent_folder = (folder_idx == 1 || folder_idx == 3);

	/* Pass 1: Count matching records */
	for (i = 0; i < ram_db_count; i++) {
		if (ram_db[i].folder_idx == folder_idx) {
			if (start_id == 0 || ram_db[i].id >= start_id) {
				count++;
				if (max_count > 0 && count >= max_count) break;
			}
		}
	}

	/* XXX: Add extra slot for test error item in the Inbox */
	if (folder_idx == 0 && (start_id == 0 || start_id <= 9999)) {
		count++;
	}
	/* XXX: End of simulated error */

	if (count == 0) {
		*out_headers = NULL;
		*out_count = 0;
		return 0;
	}

	headers = (StoreMsgHeader *)malloc(count * sizeof(StoreMsgHeader));
	if (!headers) return -1;

	/* Pass 2: Populate headers */
	{
		uint32_t idx = 0;

		/* XXX: Inject test error item into Inbox */
		if (folder_idx == 0 && (start_id == 0 || start_id <= 9999)) {
			if (idx < count) {
				headers[idx].id = 9999;
				headers[idx].mbox_offset = 9999;
				headers[idx].mbox_length = 0;
				wcscpy_s(headers[idx].subject, 128, L"[TEST] Trigger Error State");
				wcscpy_s(headers[idx].participant, 128, L"Error Tester");
				wcscpy_s(headers[idx].date_str, 32, L"Today");
				headers[idx].timestamp = 20261013;
				idx++;
			}
		}
		/* XXX: End of simulated error */

		for (i = 0; i < ram_db_count; i++) {
			if (ram_db[i].folder_idx == folder_idx) {
				if (start_id == 0 || ram_db[i].id >= start_id) {
					headers[idx].id = ram_db[i].id;
					headers[idx].mbox_offset = ram_db[i].mbox_offset;
					headers[idx].mbox_length = ram_db[i].mbox_length;
					wcscpy_s(headers[idx].subject, 128, ram_db[i].subject);

					if (is_sent_folder) {
						wcscpy_s(headers[idx].participant, 128, ram_db[i].to_addr);
					} else {
						wcscpy_s(headers[idx].participant, 128, ram_db[i].from_addr);
					}

					wcscpy_s(headers[idx].date_str, 32, ram_db[i].date_str);
					headers[idx].timestamp = ram_db[i].timestamp;
					idx++;

					if (max_count > 0 && idx >= max_count) break;
				}
			}
		}
	}

	*out_headers = headers;
	*out_count = count;
	return 0;
}


int store_ram_fetch_message(uint64_t msg_id, wchar_t **out_eml_data, size_t *out_size)
{
	const wchar_t *src = NULL;
	wchar_t *eml_copy = NULL;
	size_t len;
	size_t i;

	if (!out_eml_data) return -1;

	/* XXX: Deliberately trigger an error for testing */
	if (msg_id == 9999) {
		*out_eml_data = NULL;
		if (out_size) *out_size = 0;
		return -1;
	}
	/* XXX: End of simulated error */

	for (i = 0; i < ram_db_count; i++) {
		if (ram_db[i].id == msg_id) {
			src = ram_db[i].eml_blob;
			break;
		}
	}

	if (!src) {
		*out_eml_data = NULL;
		if (out_size) *out_size = 0;
		return -1;
	}

	len = wcslen(src) + 1;
	eml_copy = (wchar_t *)malloc(len * sizeof(wchar_t));
	if (!eml_copy) return -1;

	wcscpy_s(eml_copy, len, src);
	*out_eml_data = eml_copy;
	if (out_size) *out_size = len * sizeof(wchar_t);
	return 0;
}

void store_ram_free_message(void *msg_data)
{
	if (msg_data) free(msg_data);
}
