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


int store_init(MailContext *ctx);
void store_cleanup(void);

/* Folder and message queries */
int store_fetch_folder_messages(int32_t folder_idx, StoreMsgHeader **out_headers, uint32_t *out_count);
void store_free_folder_messages(StoreMsgHeader *headers);

/* Range/ID-relative queries */
int store_fetch_message_range(int32_t folder_idx, uint64_t start_id, uint32_t max_count, StoreMsgHeader **out_headers, uint32_t *out_count);

/* Returns EML format headers + body; memory is allocated by the backing store */
int store_fetch_message(uint64_t msg_id, char **out_eml_data, size_t *out_size);

/* General-purpose one-shot cleanup for store-allocated message data */
void store_free_message(void *msg_data);

/* Message mutation operations */
int store_add_message(int32_t folder_idx, const char *subject, const char *participant, const char *date_str, uint32_t timestamp, const char *eml_data, uint64_t *out_msg_id);
int store_delete_message(uint64_t msg_id);

#ifdef __cplusplus
}
#endif

#endif /* DATASTORE_H */
