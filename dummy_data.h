/* Spite Mail dummy test data provider
 * See spitemail.c for license information */

#ifndef DUMMY_DATA_H
#define DUMMY_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "datastore.h"

int populate_dummy_account_folders(StoreAccount *account);
int populate_dummy_ram_store(StoreAccount *account);

#ifdef __cplusplus
}
#endif

#endif /* DUMMY_DATA_H */