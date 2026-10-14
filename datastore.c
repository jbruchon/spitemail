/* Spite Mail data store API
 * license: GPLv2; (C) 2026 Jody Bruchon <jody@jodybruchon.com> */

#include <stdlib.h>
#include "spitemail.h"
#include "datastore.h"
#include "store_ram.h"


int store_init(MailContext *ctx)
{
	return store_ram_init(ctx);
}


void store_cleanup(void)
{
	store_ram_cleanup();
}


int store_fetch_folder_messages(int32_t folder_idx, StoreMsgHeader **out_headers, uint32_t *out_count)
{
	return store_ram_fetch_folder_messages(folder_idx, out_headers, out_count);
}


void store_free_folder_messages(StoreMsgHeader *headers)
{
	store_ram_free_folder_messages(headers);
}


int store_fetch_message_range(int32_t folder_idx, uint64_t start_id, uint32_t max_count, StoreMsgHeader **out_headers, uint32_t *out_count)
{
	return store_ram_fetch_message_range(folder_idx, start_id, max_count, out_headers, out_count);
}


int store_fetch_message(uint64_t msg_id, char **out_eml_data, size_t *out_size)
{
	return store_ram_fetch_message(msg_id, out_eml_data, out_size);
}


void store_free_message(void *msg_data)
{
	store_ram_free_message(msg_data);
}


int store_add_message(int32_t folder_idx, const char *subject, const char *participant, const char *date_str, uint32_t timestamp, const char *eml_data, uint64_t *out_msg_id)
{
	return store_ram_add_message(folder_idx, subject, participant, date_str, timestamp, eml_data, out_msg_id);
}


int store_delete_message(uint64_t msg_id)
{
	return store_ram_delete_message(msg_id);
}
