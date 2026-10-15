/* Spite Mail data store - RAM-only/temporary dummy data store
 * license: GPLv2; (C) 2026 Jody Bruchon <jody@jodybruchon.com> */

#include <stdlib.h>
#include <string.h>
#include "spitemail.h"
#include "datastore.h"
#include "store_ram.h"
#include "dummy_data.h"


typedef struct RamRecord {
	uint64_t id;
	int32_t  folder_idx;
	uint32_t mbox_offset;
	uint32_t mbox_length;
	char     subject[128];
	char     from_addr[128];
	char     to_addr[128];
	char     date_str[32];
	uint32_t timestamp;
	char     eml_blob[2048];
} RamRecord;


typedef struct RamStorePriv {
	RamRecord *ram_db;
	size_t     ram_db_count;
	size_t     ram_db_capacity;
	uint64_t   next_msg_id;
} RamStorePriv;


struct StoreAccount {
	char         account_name[128];
	StoreDriver  *driver;
	FolderInfo   *folders;
	uint32_t     folder_count;
	void         *store_priv;
};


/*** Forward declarations ***/
void store_ram_free_folders(FolderInfo *folders);


/*** End definitions, begin code ***/


int store_ram_has_account(const char *account_name)
{
	if (!account_name) return 0;
	if (strcmp(account_name, "ram") == 0 || strcmp(account_name, "RAM") == 0 || strchr(account_name, '@') != NULL) return 1;
	return 0;
}


StoreAccount *store_ram_open_account(const char *account_name)
{
	StoreAccount *acc;
	RamStorePriv *priv;

	acc = (StoreAccount *)malloc(sizeof(StoreAccount));
	if (!acc) return NULL;
	memset(acc, 0, sizeof(StoreAccount));

	priv = (RamStorePriv *)malloc(sizeof(RamStorePriv));
	if (!priv) {
		free(acc);
		return NULL;
	}
	memset(priv, 0, sizeof(RamStorePriv));

	strcpy_s(acc->account_name, sizeof(acc->account_name), account_name ? account_name : "ram");
	acc->driver = &store_ram_driver;
	acc->store_priv = priv;

	priv->ram_db_capacity = 8;
	priv->ram_db = (RamRecord *)malloc(priv->ram_db_capacity * sizeof(RamRecord));
	if (!priv->ram_db) {
		free(priv);
		free(acc);
		return NULL;
	}
	priv->ram_db_count = 0;
	priv->next_msg_id = 1001;

	if (get_dummy_folders(&acc->folders, &acc->folder_count) != 0) {
		free(priv->ram_db);
		free(priv);
		free(acc);
		return NULL;
	}

	if (populate_dummy_ram_store(acc) != 0) {
		store_ram_free_folders(acc->folders);
		free(priv->ram_db);
		free(priv);
		free(acc);
		return NULL;
	}

	return acc;
}


const char *store_ram_get_account_name(StoreAccount *account)
{
	if (!account) return NULL;
	return account->account_name;
}


void store_ram_close_account(StoreAccount *account)
{
	if (!account) return;
	if (account->store_priv) {
		RamStorePriv *priv = (RamStorePriv *)account->store_priv;
		if (priv->ram_db) free(priv->ram_db);
		free(priv);
	}
	if (account->folders) free(account->folders);
	free(account);
}


int store_ram_fetch_folders(StoreAccount *account, FolderInfo **out_folders, uint32_t *out_folder_count)
{
	if (!account || !out_folders || !out_folder_count) return -1;
	return get_dummy_folders(out_folders, out_folder_count);
}


void store_ram_free_folders(FolderInfo *folders)
{
	if (folders) free(folders);
}


int store_ram_fetch_folder_messages(StoreAccount *account, int32_t folder_idx, StoreMsgHeader **out_headers, uint32_t *out_count)
{
	return store_ram_fetch_message_range(account, folder_idx, 0, 0, out_headers, out_count);
}


void store_ram_free_folder_messages(StoreMsgHeader *headers)
{
	if (headers) free(headers);
}


int store_ram_fetch_message_range(StoreAccount *account, int32_t folder_idx, uint64_t start_id, uint32_t max_count, StoreMsgHeader **out_headers, uint32_t *out_count)
{
	RamStorePriv *priv;
	StoreMsgHeader *headers = NULL;
	uint32_t count = 0;
	size_t i;
	int is_sent_folder;

	if (!account || !account->store_priv || !out_headers || !out_count) return -1;
	priv = (RamStorePriv *)account->store_priv;

	is_sent_folder = (folder_idx == 1 || folder_idx == 3);

	for (i = 0; i < priv->ram_db_count; i++) {
		if (priv->ram_db[i].folder_idx == folder_idx) {
			if (start_id == 0 || priv->ram_db[i].id >= start_id) {
				count++;
				if (max_count > 0 && count >= max_count) break;
			}
		}
	}

	if (count == 0) {
		*out_headers = NULL;
		*out_count = 0;
		return 0;
	}

	headers = (StoreMsgHeader *)malloc(count * sizeof(StoreMsgHeader));
	if (!headers) return -1;

	{
		uint32_t idx = 0;
		for (i = 0; i < priv->ram_db_count; i++) {
			if (priv->ram_db[i].folder_idx == folder_idx) {
				if (start_id == 0 || priv->ram_db[i].id >= start_id) {
					headers[idx].id = priv->ram_db[i].id;
					headers[idx].mbox_offset = priv->ram_db[i].mbox_offset;
					headers[idx].mbox_length = priv->ram_db[i].mbox_length;
					strcpy_s(headers[idx].subject, sizeof(headers[idx].subject), priv->ram_db[i].subject);
					if (is_sent_folder) {
						strcpy_s(headers[idx].participant, sizeof(headers[idx].participant), priv->ram_db[i].to_addr);
					} else {
						strcpy_s(headers[idx].participant, sizeof(headers[idx].participant), priv->ram_db[i].from_addr);
					}
					strcpy_s(headers[idx].date_str, sizeof(headers[idx].date_str), priv->ram_db[i].date_str);
					headers[idx].timestamp = priv->ram_db[i].timestamp;
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


int store_ram_fetch_message(StoreAccount *account, uint64_t msg_id, char **out_eml_data, size_t *out_size)
{
	RamStorePriv *priv;
	const char *src = NULL;
	char *eml_copy = NULL;
	size_t len;
	size_t i;

	if (!account || !account->store_priv || !out_eml_data) return -1;
	priv = (RamStorePriv *)account->store_priv;

	for (i = 0; i < priv->ram_db_count; i++) {
		if (priv->ram_db[i].id == msg_id) {
			src = priv->ram_db[i].eml_blob;
			break;
		}
	}

	if (!src) {
		*out_eml_data = NULL;
		if (out_size) *out_size = 0;
		return -1;
	}

	len = strlen(src) + 1;
	eml_copy = (char *)malloc(len);
	if (!eml_copy) return -1;

	strcpy_s(eml_copy, len, src);
	*out_eml_data = eml_copy;
	if (out_size) *out_size = len;
	return 0;
}


void store_ram_free_message(void *msg_data)
{
	if (msg_data) free(msg_data);
}


int store_ram_add_message(StoreAccount *account, int32_t folder_idx, const char *subject, const char *participant, const char *date_str, uint32_t timestamp, const char *eml_data, uint64_t *out_msg_id)
{
	RamStorePriv *priv;
	RamRecord *new_db;
	int is_sent_folder;

	if (!account || !account->store_priv || !subject || !eml_data) return -1;
	priv = (RamStorePriv *)account->store_priv;

	if (priv->ram_db_count >= priv->ram_db_capacity) {
		size_t new_cap = priv->ram_db_capacity * 2;
		new_db = (RamRecord *)realloc(priv->ram_db, new_cap * sizeof(RamRecord));
		if (!new_db) return -1;
		priv->ram_db = new_db;
		priv->ram_db_capacity = new_cap;
	}

	is_sent_folder = (folder_idx == 1 || folder_idx == 3);
	RamRecord *rec = &priv->ram_db[priv->ram_db_count];
	memset(rec, 0, sizeof(RamRecord));
	rec->id = priv->next_msg_id++;
	rec->folder_idx = folder_idx;
	rec->timestamp = timestamp;
	strcpy_s(rec->subject, sizeof(rec->subject), subject);
	if (date_str) {
		strcpy_s(rec->date_str, sizeof(rec->date_str), date_str);
	} else {
		strcpy_s(rec->date_str, sizeof(rec->date_str), "Today");
	}

	if (is_sent_folder) {
		strcpy_s(rec->to_addr, sizeof(rec->to_addr), participant ? participant : "recipient@spitemail.com");
		strcpy_s(rec->from_addr, sizeof(rec->from_addr), "root@spitemail.com");
	} else {
		strcpy_s(rec->from_addr, sizeof(rec->from_addr), participant ? participant : "sender@spitemail.com");
		strcpy_s(rec->to_addr, sizeof(rec->to_addr), "root@spitemail.com");
	}

	strcpy_s(rec->eml_blob, sizeof(rec->eml_blob), eml_data);
	rec->mbox_length = (uint32_t)strlen(eml_data);
	priv->ram_db_count++;

	if (out_msg_id) *out_msg_id = rec->id;
	return 0;
}


int store_ram_delete_message(StoreAccount *account, uint64_t msg_id)
{
	RamStorePriv *priv;
	size_t i, j;

	if (!account || !account->store_priv) return -1;
	priv = (RamStorePriv *)account->store_priv;

	for (i = 0; i < priv->ram_db_count; i++) {
		if (priv->ram_db[i].id == msg_id) {
			for (j = i; j < priv->ram_db_count - 1; j++) {
				priv->ram_db[j] = priv->ram_db[j + 1];
			}
			priv->ram_db_count--;
			return 0;
		}
	}
	return -1;
}


StoreDriver store_ram_driver = {
	.name = "ram",
	.has_account = store_ram_has_account,
	.open_account = store_ram_open_account,
	.close_account = store_ram_close_account,
	.fetch_folders = store_ram_fetch_folders,
	.free_folders = store_ram_free_folders,
	.fetch_folder_messages = store_ram_fetch_folder_messages,
	.free_folder_messages = store_ram_free_folder_messages,
	.fetch_message_range = store_ram_fetch_message_range,
	.fetch_message = store_ram_fetch_message,
	.free_message = store_ram_free_message,
	.add_message = store_ram_add_message,
	.delete_message = store_ram_delete_message
};
