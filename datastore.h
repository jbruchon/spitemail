/* Spite Mail data store API
 * See spitemail.c for license information */

#ifndef DATASTORE_H
#define DATASTORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* Forward declare or fully define StoreAccount BEFORE StoreDriver references it */
typedef struct StoreAccount StoreAccount;

#include "spitemail.h"

typedef enum FolderType {
	FOLDER_TYPE_OTHER = 0,
	FOLDER_TYPE_INBOX,
	FOLDER_TYPE_SENT,
	FOLDER_TYPE_OUTBOX,
	FOLDER_TYPE_DRAFTS,
	FOLDER_TYPE_JUNK,
	FOLDER_TYPE_TRASH
} FolderType;

typedef struct StoreMsgHeader {
	uint64_t id;
	uint32_t mbox_offset;
	uint32_t mbox_length;
	char     subject[128];
	char     participant[128];
	uint64_t timestamp;
} StoreMsgHeader;

typedef struct MessageListItem {
	uint64_t id;
	uint32_t mbox_offset;
	uint32_t mbox_length;
	char     subject[128];
	char     participant[128];
	uint64_t timestamp;
} MessageListItem;

typedef struct StoreDriver {
	const char *name;
	int (*has_account)(const char *account_name);
	StoreAccount *(*open_account)(const char *account_name);
	void (*close_account)(StoreAccount *account);
	int (*fetch_folders)(StoreAccount *account, FolderType **out_types, int32_t **out_folder_ids, char ***out_names, uint32_t **out_name_offsets, uint32_t *out_folder_count);
	int (*fetch_folder_messages)(StoreAccount *account, int32_t folder_id, StoreMsgHeader **out_headers, uint32_t *out_count);
	void (*free_folder_messages)(StoreMsgHeader *headers);
	int (*fetch_message_range)(StoreAccount *account, int32_t folder_id, uint64_t start_id, uint32_t max_count, StoreMsgHeader **out_headers, uint32_t *out_count);
	int (*fetch_message)(StoreAccount *account, uint64_t msg_id, char **out_eml_data, size_t *out_size);
	void (*free_message)(void *msg_data);
	int (*add_message)(StoreAccount *account, int32_t folder_id, const char *subject, const char *participant, uint64_t timestamp, const char *eml_data, uint64_t *out_msg_id);
	int (*delete_message)(StoreAccount *account, uint64_t msg_id);
} StoreDriver;

struct StoreAccount {
	StoreDriver  *driver;                 /* Responsible storage driver reference[cite: 13] */
	char         account_name[128];       /* Account's unique name string used for ID (e.g. foo@bar.com)[cite: 13] */
	char         display_name[128];       /* Display name used as treeview root per-account in Win32 UI[cite: 13] */
	uint32_t     folder_count;            /* Folder count[cite: 13] */
	FolderType   *folder_types;           /* SoA: flat array of type IDs[cite: 13, 14] */
	int32_t      *folder_ids;             /* SoA: flat array of unique-per-account IDs[cite: 13, 14] */
	char         **folder_names;          /* SoA: flat array of folder names[cite: 13, 14] */
	uint32_t     *folder_name_offsets;    /* SoA: flat array of name offsets[cite: 13, 14] */
	void         *store_priv;             /* Driver private storage data */
};

StoreAccount *store_open_account(const char *account_name);
void store_close_account(StoreAccount *account);
int store_fetch_folders(StoreAccount *account, FolderType **out_types, int32_t **out_folder_ids, char ***out_names, uint32_t **out_name_offsets, uint32_t *out_folder_count);
int store_fetch_folder_messages(StoreAccount *account, int32_t folder_id, StoreMsgHeader **out_headers, uint32_t *out_count);
void store_free_folder_messages(StoreMsgHeader *headers);
int store_fetch_message_range(StoreAccount *account, int32_t folder_id, uint64_t start_id, uint32_t max_count, StoreMsgHeader **out_headers, uint32_t *out_count);
int store_fetch_message(StoreAccount *account, uint64_t msg_id, char **out_eml_data, size_t *out_size);
void store_free_message(void *msg_data);
int store_add_message(StoreAccount *account, int32_t folder_id, const char *subject, const char *participant, uint64_t timestamp, const char *eml_data, uint64_t *out_msg_id);
int store_delete_message(StoreAccount *account, uint64_t msg_id);

#ifdef __cplusplus
}
#endif

#endif /* DATASTORE_H */
