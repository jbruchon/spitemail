/* Spite Mail data store - RAM-only/temporary
 * See spitemail.c for license information */

#ifndef STORE_RAM_H
#define STORE_RAM_H

#include <stdint.h>
#include "datastore.h"

int store_ram_init(MailContext *ctx);
void store_ram_cleanup(void);
int store_ram_fetch_folder_messages(int32_t folder_idx, StoreMsgHeader **out_headers, uint32_t *out_count);
void store_ram_free_folder_messages(StoreMsgHeader *headers);
int store_ram_fetch_message_range(int32_t folder_idx, uint64_t start_id, uint32_t max_count, StoreMsgHeader **out_headers, uint32_t *out_count);
int store_ram_fetch_message(uint64_t msg_id, wchar_t **out_eml_data, size_t *out_size);
void store_ram_free_message(void *msg_data);

#endif /* STORE_RAM_H */
