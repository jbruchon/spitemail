/* Spite Mail dummy test data provider
 * See spitemail.c for license information */

#ifndef DUMMY_DATA_H
#define DUMMY_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "datastore.h"

int get_dummy_folders(FolderInfo **out_folders, uint32_t *out_count);
int populate_dummy_ram_store(StoreAccount *account);

#ifdef __cplusplus
}
#endif

#endif /* DUMMY_DATA_H */
