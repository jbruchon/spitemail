/* Spite Mail data store API
 * license: GPLv2; (C) 2026 Jody Bruchon <jody@jodybruchon.com> */

#include <stdlib.h>
#include "spitemail.h"
#include "datastore.h"
#include "store_ram.h"


struct StoreAccount {
	char         account_name[128];
	StoreDriver  *driver;
	FolderInfo   *folders;
	uint32_t     folder_count;
	void         *store_priv;
};


static StoreDriver *available_stores[] = {
	&store_ram_driver,
	NULL
};


StoreAccount *store_open_account(const char *account_name)
{
	StoreDriver **d;
	if (!account_name) return NULL;

	for (d = available_stores; *d != NULL; d++) {
		if ((*d)->has_account && (*d)->has_account(account_name)) {
			if ((*d)->open_account) {
				return (*d)->open_account(account_name);
			}
			return NULL;
		}
	}
	return NULL;
}


void store_close_account(StoreAccount *account)
{
	if (!account) return;
	if (account->driver && account->driver->close_account) {
		account->driver->close_account(account);
	} else {
		free(account);
	}
}


int store_fetch_folders(StoreAccount *account, FolderInfo **out_folders, uint32_t *out_folder_count)
{
	if (!account || !account->driver || !account->driver->fetch_folders) return -1;
	return account->driver->fetch_folders(account, out_folders, out_folder_count);
}


void store_free_folders(FolderInfo *folders)
{
	if (folders) free(folders);
}


int store_fetch_folder_messages(StoreAccount *account, int32_t folder_idx, StoreMsgHeader **out_headers, uint32_t *out_count)
{
	if (!account || !account->driver || !account->driver->fetch_folder_messages) return -1;
	return account->driver->fetch_folder_messages(account, folder_idx, out_headers, out_count);
}


void store_free_folder_messages(StoreMsgHeader *headers)
{
	if (headers) free(headers);
}


int store_fetch_message_range(StoreAccount *account, int32_t folder_idx, uint64_t start_id, uint32_t max_count, StoreMsgHeader **out_headers, uint32_t *out_count)
{
	if (!account || !account->driver || !account->driver->fetch_message_range) return -1;
	return account->driver->fetch_message_range(account, folder_idx, start_id, max_count, out_headers, out_count);
}


int store_fetch_message(StoreAccount *account, uint64_t msg_id, char **out_eml_data, size_t *out_size)
{
	if (!account || !account->driver || !account->driver->fetch_message) return -1;
	return account->driver->fetch_message(account, msg_id, out_eml_data, out_size);
}


void store_free_message(void *msg_data)
{
	if (msg_data) free(msg_data);
}


int store_add_message(StoreAccount *account, int32_t folder_idx, const char *subject, const char *participant, const char *date_str, uint32_t timestamp, const char *eml_data, uint64_t *out_msg_id)
{
	if (!account || !account->driver || !account->driver->add_message) return -1;
	return account->driver->add_message(account, folder_idx, subject, participant, date_str, timestamp, eml_data, out_msg_id);
}


int store_delete_message(StoreAccount *account, uint64_t msg_id)
{
	if (!account || !account->driver || !account->driver->delete_message) return -1;
	return account->driver->delete_message(account, msg_id);
}