/* Spite Mail data store API
 * See spitemail.c for license information */

#ifndef DATASTORE_H
#define DATASTORE_H

#ifdef __cplusplus
extern "C" {
#endif


typedef struct StoreMsgHeader {
	uint64_t id;
	wchar_t  subject[128];
	wchar_t  sender[128];
	wchar_t  date_str[32];
	uint32_t timestamp;
} StoreMsgHeader;

typedef struct MessageListItem {
	uint64_t id;
	wchar_t  subject[128];
	wchar_t  sender[128];
	wchar_t  date_str[32];
	uint32_t timestamp;
} MessageListItem;


int store_init(MailContext *ctx);
void store_cleanup(void);
int store_fetch_folder_messages(int32_t folder_idx, StoreMsgHeader **out_headers, uint32_t *out_count);
void store_free_folder_messages(StoreMsgHeader *headers);
int store_fetch_message_body(uint64_t msg_id, wchar_t **out_body_text);
void store_free_message_body(wchar_t *body_text);


#ifdef __cplusplus
}
#endif

#endif /* DATASTORE_H */
