/* Spite Mail data store - RAM-only/temporary
 * See spitemail.c for license information */

#ifndef STORE_RAM_H
#define STORE_RAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "datastore.h"

extern StoreDriver store_ram_driver;
int store_ram_has_account(const char *account_name);
StoreAccount *store_ram_open_account(const char *account_name);
const char *store_ram_get_account_name(StoreAccount *account);
void store_ram_close_account(StoreAccount *account);
int store_ram_fetch_folders(StoreAccount *account, FolderType **out_types, int32_t **out_folder_ids, char ***out_names, uint32_t **out_name_offsets, uint32_t *out_folder_count);
int store_ram_fetch_folder_messages(StoreAccount *account, int32_t folder_id, StoreMsgHeader **out_headers, uint32_t *out_count);
void store_ram_free_folder_messages(StoreMsgHeader *headers);
int store_ram_fetch_message_range(StoreAccount *account, int32_t folder_id, uint64_t start_id, uint32_t max_count, StoreMsgHeader **out_headers, uint32_t *out_count);
int store_ram_fetch_message(StoreAccount *account, uint64_t msg_id, char **out_eml_data, size_t *out_size);
void store_ram_free_message(void *msg_data);
int store_ram_add_message(StoreAccount *account, int32_t folder_id, const char *subject, const char *participant, const char *date_str, uint32_t timestamp, const char *eml_data, uint64_t *out_msg_id);
int store_ram_delete_message(StoreAccount *account, uint64_t msg_id);


#ifdef __cplusplus
}
#endif

#endif /* STORE_RAM_H */
