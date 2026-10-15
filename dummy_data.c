/* Spite Mail dummy test data provider
 * license: GPLv2; (C) 2026 Jody Bruchon <jody@jodybruchon.com> */

#include <stdlib.h>
#include <string.h>
#include "datastore.h"
#include "store_ram.h"
#include "dummy_data.h"


static const FolderInfo dummy_folders[] = {
	{ 0, FOLDER_TYPE_INBOX, "Inbox" },
	{ 1, FOLDER_TYPE_SENT, "Sent" },
	{ 2, FOLDER_TYPE_DRAFTS, "Drafts" },
	{ 3, FOLDER_TYPE_TRASH, "Trash" }
};


int get_dummy_folders(FolderInfo **out_folders, uint32_t *out_count)
{
	size_t count = sizeof(dummy_folders) / sizeof(dummy_folders[0]);
	FolderInfo *copy = (FolderInfo *)malloc(count * sizeof(FolderInfo));
	if (!copy) return -1;

	memcpy(copy, dummy_folders, count * sizeof(FolderInfo));
	*out_folders = copy;
	*out_count = (uint32_t)count;
	return 0;
}


int populate_dummy_ram_store(StoreAccount *account)
{
	uint64_t new_id = 0;
	const char *acc_name = store_ram_get_account_name(account);

	if (!acc_name) acc_name = "";

	if (strcmp(acc_name, "default") == 0 || strstr(acc_name, "root") != NULL) {
		/* First account: 2 in Inbox, 1 in Sent */
		if (store_ram_add_message(account, 0, "Welcome to Spite Mail (RAM Store)", "Jody Bruchon <jody@jodybruchon.com>", "Tue, 6 Oct 2026", 20261006,
			"From: Jody Bruchon <jody@jodybruchon.com>\r\n"
			"To: root@spitemail.com\r\n"
			"Subject: Welcome to Spite Mail\r\n"
			"Date: Tue, 6 Oct 2026\r\n\r\n"
			"Spite Mail is designed for speed, low overhead, and pure native Win32 controls.", &new_id) != 0) return -1;

		if (store_ram_add_message(account, 0, "Second Inbox Message", "Test User <test@example.com>", "Tue, 6 Oct 2026", 20261007,
			"From: Test User <test@example.com>\r\n"
			"To: root@spitemail.com\r\n"
			"Subject: Second Inbox Message\r\n"
			"Date: Tue, 6 Oct 2026\r\n\r\n"
			"This is the second test message in the inbox.", &new_id) != 0) return -1;

		if (store_ram_add_message(account, 1, "Re: Bug report on jdupes memory usage", "Bug Reporter <reporter@example.com>", "Mon, 5 Oct 2026", 20261005,
			"From: root@spitemail.com\r\n"
			"To: Bug Reporter <reporter@example.com>\r\n"
			"Subject: Re: Bug report on jdupes memory usage\r\n"
			"Date: Mon, 5 Oct 2026\r\n\r\n"
			"Thanks for the report. Memory layout optimizations are being applied.", &new_id) != 0) return -1;

	} else {
		/* Subsequent accounts: 1 in Inbox, 1 in Sent */
		if (store_ram_add_message(account, 0, "Inbox Message for Account", "Sender <sender@example.com>", "Wed, 7 Oct 2026", 20261007,
			"From: Sender <sender@example.com>\r\n"
			"To: user@spitemail.com\r\n"
			"Subject: Inbox Message for Account\r\n"
			"Date: Wed, 7 Oct 2026\r\n\r\n"
			"Secondary account inbox test content.", &new_id) != 0) return -1;

		if (store_ram_add_message(account, 1, "Sent Message for Account", "Recipient <recipient@example.com>", "Wed, 7 Oct 2026", 20261008,
			"From: user@spitemail.com\r\n"
			"To: Recipient <recipient@example.com>\r\n"
			"Subject: Sent Message for Account\r\n"
			"Date: Wed, 7 Oct 2026\r\n\r\n"
			"Secondary account sent test content.", &new_id) != 0) return -1;
	}

	return 0;
}
