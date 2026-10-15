/* Spite Mail dummy test data provider
 * license: GPLv2; (C) 2026 Jody Bruchon <jody@jodybruchon.com> */

#include <stdlib.h>
#include <string.h>
#include "datastore.h"
#include "store_ram.h"
#include "dummy_data.h"


int populate_dummy_account_folders(StoreAccount *account)
{
	const char *acc_name = store_ram_get_account_name(account);
	uint32_t count = 4;
	FolderType *types;
	int32_t *ids;
	char **names;
	uint32_t *offsets;
	size_t i;
	uint32_t curr_off = 0;
	const char *temp_names[4];

	if (!acc_name) acc_name = "default";

	types = (FolderType *)malloc(count * sizeof(FolderType));
	ids = (int32_t *)malloc(count * sizeof(int32_t));
	names = (char **)malloc(count * sizeof(char *));
	offsets = (uint32_t *)malloc(count * sizeof(uint32_t));

	if (!types || !ids || !names || !offsets) {
		if (types) free(types);
		if (ids) free(ids);
		if (names) free(names);
		if (offsets) free(offsets);
		return -1;
	}

	/* Unique ID numbers and folder names assigned to temp_names */
	if (strcmp(acc_name, "default") == 0 || strstr(acc_name, "root") != NULL) {
		types[0] = FOLDER_TYPE_INBOX;  ids[0] = 101; temp_names[0] = "Inbox";
		types[1] = FOLDER_TYPE_SENT;   ids[1] = 102; temp_names[1] = "Sent";
		types[2] = FOLDER_TYPE_DRAFTS; ids[2] = 103; temp_names[2] = "Drafts";
		types[3] = FOLDER_TYPE_TRASH;  ids[3] = 104; temp_names[3] = "Trash";
	} else if (strstr(acc_name, "gazingcat") != NULL) {
		types[0] = FOLDER_TYPE_INBOX;  ids[0] = 201; temp_names[0] = "Inbox";
		types[1] = FOLDER_TYPE_SENT;   ids[1] = 202; temp_names[1] = "Sent";
		types[2] = FOLDER_TYPE_DRAFTS; ids[2] = 203; temp_names[2] = "Drafts";
		types[3] = FOLDER_TYPE_TRASH;  ids[3] = 204; temp_names[3] = "Trash";
	} else {
		types[0] = FOLDER_TYPE_INBOX;  ids[0] = 301; temp_names[0] = "Inbox";
		types[1] = FOLDER_TYPE_SENT;   ids[1] = 302; temp_names[1] = "Sent";
		types[2] = FOLDER_TYPE_DRAFTS; ids[2] = 303; temp_names[2] = "Drafts";
		types[3] = FOLDER_TYPE_TRASH;  ids[3] = 304; temp_names[3] = "Trash";
	}

	for (i = 0; i < count; i++) {
		size_t len = strlen(temp_names[i]) + 1;
		names[i] = (char *)malloc(len);
		if (!names[i]) {
			size_t j;
			for (j = 0; j < i; j++) free(names[j]);
			free(types); free(ids); free(names); free(offsets);
			return -1;
		}
		strcpy_s(names[i], len, temp_names[i]);
		offsets[i] = curr_off;
		curr_off += (uint32_t)len;
	}

	account->folder_count = count;
	account->folder_types = types;
	account->folder_ids = ids;
	account->folder_names = names;
	account->folder_name_offsets = offsets;

	return 0;
}


int populate_dummy_ram_store(StoreAccount *account)
{
	uint64_t new_id = 0;
	const char *acc_name = store_ram_get_account_name(account);
	int32_t inbox_id = 101;
	int32_t sent_id = 102;
	size_t i;

	if (!acc_name) acc_name = "";

	for (i = 0; i < account->folder_count; i++) {
		if (account->folder_types[i] == FOLDER_TYPE_INBOX) inbox_id = account->folder_ids[i];
		if (account->folder_types[i] == FOLDER_TYPE_SENT) sent_id = account->folder_ids[i];
	}

	if (strcmp(acc_name, "default") == 0 || strstr(acc_name, "root") != NULL) {
		/* First account: 2 in Inbox, 1 in Sent */
		if (store_ram_add_message(account, inbox_id, "Welcome to Spite Mail (RAM Store)", "Jody Bruchon <jody@jodybruchon.com>", 1791244800ULL,
			"From: Jody Bruchon <jody@jodybruchon.com>\r\n"
			"To: root@spitemail.com\r\n"
			"Subject: Welcome to Spite Mail\r\n"
			"Date: Tue, 6 Oct 2026\r\n\r\n"
			"Spite Mail is designed for speed, low overhead, and pure native Win32 controls.", &new_id) != 0) return -1;

		if (store_ram_add_message(account, inbox_id, "Second Inbox Message", "Test User <test@example.com>", 1791320900ULL,
			"From: Test User <test@example.com>\r\n"
			"To: root@spitemail.com\r\n"
			"Subject: Second Inbox Message\r\n"
			"Date: Tue, 6 Oct 2026\r\n\r\n"
			"This is the second test message in the inbox.", &new_id) != 0) return -1;

		if (store_ram_add_message(account, sent_id, "Re: Bug report on jdupes memory usage", "Bug Reporter <reporter@example.com>", 1791158400ULL,
			"From: root@spitemail.com\r\n"
			"To: Bug Reporter <reporter@example.com>\r\n"
			"Subject: Re: Bug report on jdupes memory usage\r\n"
			"Date: Mon, 5 Oct 2026\r\n\r\n"
			"Thanks for the report. Memory layout optimizations are being applied.", &new_id) != 0) return -1;

	} else {
		/* Subsequent accounts: 1 in Inbox, 1 in Sent */
		if (store_ram_add_message(account, inbox_id, "Inbox Message for Account", "Sender <sender@example.com>", 1791320000ULL,
			"From: Sender <sender@example.com>\r\n"
			"To: user@spitemail.com\r\n"
			"Subject: Inbox Message for Account\r\n"
			"Date: Wed, 7 Oct 2026\r\n\r\n"
			"Secondary account inbox test content.", &new_id) != 0) return -1;

		if (store_ram_add_message(account, sent_id, "Sent Message for Account", "Recipient <recipient@example.com>", 1791417600ULL,
			"From: user@spitemail.com\r\n"
			"To: Recipient <recipient@example.com>\r\n"
			"Subject: Sent Message for Account\r\n"
			"Date: Wed, 7 Oct 2026\r\n\r\n"
			"Secondary account sent test content.", &new_id) != 0) return -1;
	}
	return 0;
}
