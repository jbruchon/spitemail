/* Spite Mail data store API
 * See spitemail.c for license information */

#ifndef DATASTORE_H
#define DATASTORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
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

typedef struct FolderInfo {
	int32_t    folder_id;      /* Numeric ID addressed under the hood */
	FolderType type;           /* Functional folder type */
	char       name[64];       /* Friendly display name */
} FolderInfo;

typedef struct StoreMsgHeader {
	uint64_t id;
	uint32_t mbox_offset;    /* Reserved for mbox/sidecar byte offsets */
	uint32_t mbox_length;    /* Reserved for mbox/sidecar message length */
	char     subject[128];
	char     participant[128];  /* From: (Inbox etc) or To: (Sent) */
	char     date_str[32];
	uint32_t timestamp;
} StoreMsgHeader;

typedef struct MessageListItem {
	uint64_t id;
	uint32_t mbox_offset;
	uint32_t mbox_length;
	char     subject[128];
	char     participant[128];
	char     date_str[32];
	uint32_t timestamp;
} MessageListItem;

typedef struct StoreAccount StoreAccount;

typedef struct StoreDriver {
	const char *name;
	int (*has_account)(const char *account_name);
	StoreAccount *(*open_account)(const char *account_name);
	void (*close_account)(StoreAccount *account);
	int (*fetch_folders)(StoreAccount *account, FolderInfo **out_folders, uint32_t *out_folder_count);
	void (*free_folders)(FolderInfo *folders);
	int (*fetch_folder_messages)(StoreAccount *account, int32_t folder_idx, StoreMsgHeader **out_headers, uint32_t *out_count);
	void (*free_folder_messages)(StoreMsgHeader *headers);
	int (*fetch_message_range)(StoreAccount *account, int32_t folder_idx, uint64_t start_id, uint32_t max_count, StoreMsgHeader **out_headers, uint32_t *out_count);
	int (*fetch_message)(StoreAccount *account, uint64_t msg_id, char **out_eml_data, size_t *out_size);
	void (*free_message)(void *msg_data);
	int (*add_message)(StoreAccount *account, int32_t folder_idx, const char *subject, const char *participant, const char *date_str, uint32_t timestamp, const char *eml_data, uint64_t *out_msg_id);
	int (*delete_message)(StoreAccount *account, uint64_t msg_id);
} StoreDriver;


StoreAccount *store_open_account(const char *account_name);
void store_close_account(StoreAccount *account);
int store_fetch_folders(StoreAccount *account, FolderInfo **out_folders, uint32_t *out_folder_count);
void store_free_folders(FolderInfo *folders);
int store_fetch_folder_messages(StoreAccount *account, int32_t folder_idx, StoreMsgHeader **out_headers, uint32_t *out_count);
void store_free_folder_messages(StoreMsgHeader *headers);
int store_fetch_message_range(StoreAccount *account, int32_t folder_idx, uint64_t start_id, uint32_t max_count, StoreMsgHeader **out_headers, uint32_t *out_count);
int store_fetch_message(StoreAccount *account, uint64_t msg_id, char **out_eml_data, size_t *out_size);
void store_free_message(void *msg_data);
int store_add_message(StoreAccount *account, int32_t folder_idx, const char *subject, const char *participant, const char *date_str, uint32_t timestamp, const char *eml_data, uint64_t *out_msg_id);
int store_delete_message(StoreAccount *account, uint64_t msg_id);

#ifdef __cplusplus
}
#endif

#endif /* DATASTORE_H */
