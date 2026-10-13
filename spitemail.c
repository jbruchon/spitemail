/* Spite Mail core data and configuration engine
 * license: GPLv2; (C) 2026 Jody Bruchon <jody@jodybruchon.com> */

#include "version.h"
#include "spitemail.h"
#include "datastore.h"

MailContext g_mail_ctx;  /* global application mail context instance */


uint32_t pool_add_wstr(StringPool *pool, const wchar_t *w_str)
{
	uint32_t req_chars;
	uint32_t offset;
	uint32_t new_cap;
	wchar_t *new_data;

	if (!w_str || *w_str == L'\0') return 0;

	req_chars = (uint32_t)wcslen(w_str) + 1;

	if (pool->used + req_chars > pool->capacity) {
		new_cap = pool->capacity ? pool->capacity * 2 : 4096;
		while (pool->used + req_chars > new_cap) {
			new_cap *= 2;
		}

		new_data = (wchar_t *)realloc(pool->data, new_cap * sizeof(wchar_t));
		if (!new_data) return 0;

		pool->data = new_data;
		pool->capacity = new_cap;
	}

	offset = pool->used;
	wcscpy_s(pool->data + offset, pool->capacity - offset, w_str);
	pool->used += req_chars;

	return offset;
}


wchar_t *pool_get_wstr(const StringPool *pool, uint32_t offset)
{
	return pool->data + offset;
}


COLORREF parse_hex_color(const wchar_t *hex_str, COLORREF default_color)
{
	unsigned int r = 0, g = 0, b = 0;
	if (!hex_str || *hex_str == L'\0') return default_color;
	if (*hex_str == L'#') hex_str++;
	if (wcslen(hex_str) != 6) return default_color;
	if (swscanf_s(hex_str, L"%2x%2x%2x", &r, &g, &b) == 3) return RGB(r, g, b);
	return default_color;
}


void load_app_configuration(MailContext *ctx)
{
	wchar_t config_path[MAX_PATH];
	wchar_t section_names[4096];
	DWORD sec_len;
	wchar_t *pSec;
	uint32_t acc_capacity = 4;
	uint32_t fld_capacity = 8;
	uint32_t acc_idx = 0;
	uint32_t folder_idx = 0;

	memset(ctx, 0, sizeof(MailContext));

	ctx->accounts.capacity = acc_capacity;
	ctx->accounts.name_offsets = (uint32_t *)malloc(acc_capacity * sizeof(uint32_t));
	ctx->accounts.server_offsets = (uint32_t *)malloc(acc_capacity * sizeof(uint32_t));
	ctx->accounts.username_offsets = (uint32_t *)malloc(acc_capacity * sizeof(uint32_t));
	ctx->accounts.ports = (uint32_t *)malloc(acc_capacity * sizeof(uint32_t));
	ctx->accounts.root_folder_idx = (int32_t *)malloc(acc_capacity * sizeof(int32_t));
	ctx->accounts.store_flags = (uint32_t *)malloc(acc_capacity * sizeof(uint32_t));

	ctx->folders.capacity = fld_capacity;
	ctx->folders.name_offsets = (uint32_t *)malloc(fld_capacity * sizeof(uint32_t));
	ctx->folders.path_offsets = (uint32_t *)malloc(fld_capacity * sizeof(uint32_t));
	ctx->folders.unread_counts = (uint32_t *)malloc(fld_capacity * sizeof(uint32_t));
	ctx->folders.flags = (uint32_t *)malloc(fld_capacity * sizeof(uint32_t));
	ctx->folders.parent_idx = (int32_t *)malloc(fld_capacity * sizeof(int32_t));
	ctx->folders.first_child_idx = (int32_t *)malloc(fld_capacity * sizeof(int32_t));
	ctx->folders.next_sibling_idx = (int32_t *)malloc(fld_capacity * sizeof(int32_t));
	ctx->folders.htree_items = (HTREEITEM *)malloc(fld_capacity * sizeof(HTREEITEM));

	GetModuleFileNameW(NULL, config_path, MAX_PATH);
	{
		wchar_t *last_slash = wcsrchr(config_path, L'\\');
		if (last_slash) {
			wcscpy_s(last_slash + 1, MAX_PATH - (last_slash + 1 - config_path), L"spitemail.ini");
		} else {
			wcscpy_s(config_path, MAX_PATH, L".\\spitemail.ini");
		}
	}

	sec_len = GetPrivateProfileSectionNamesW(section_names, sizeof(section_names) / sizeof(wchar_t), config_path);
	if (sec_len > 0) {
		pSec = section_names;
		while (*pSec != L'\0') {
			if (_wcsnicmp(pSec, L"Account", 7) == 0) {
				wchar_t name_buf[128];
				wchar_t serv_buf[128];
				wchar_t user_buf[128];
				wchar_t store_buf[32];
				int port;
				uint32_t name_off, serv_off, user_off;
				uint32_t inbox_name_off, sent_name_off;
				int32_t inbox_idx, sent_idx;

				GetPrivateProfileStringW(pSec, L"Name", L"user@spitemail.com", name_buf, 128, config_path);
				GetPrivateProfileStringW(pSec, L"Server", L"imap.spitemail.com", serv_buf, 128, config_path);
				GetPrivateProfileStringW(pSec, L"Username", name_buf, user_buf, 128, config_path);
				GetPrivateProfileStringW(pSec, L"Store", L"ram", store_buf, 32, config_path);
				port = GetPrivateProfileIntW(pSec, L"Port", 993, config_path);

				if (acc_idx >= ctx->accounts.capacity) {
					uint32_t new_cap = ctx->accounts.capacity * 2;
					ctx->accounts.name_offsets = (uint32_t *)realloc(ctx->accounts.name_offsets, new_cap * sizeof(uint32_t));
					ctx->accounts.server_offsets = (uint32_t *)realloc(ctx->accounts.server_offsets, new_cap * sizeof(uint32_t));
					ctx->accounts.username_offsets = (uint32_t *)realloc(ctx->accounts.username_offsets, new_cap * sizeof(uint32_t));
					ctx->accounts.ports = (uint32_t *)realloc(ctx->accounts.ports, new_cap * sizeof(uint32_t));
					ctx->accounts.root_folder_idx = (int32_t *)realloc(ctx->accounts.root_folder_idx, new_cap * sizeof(int32_t));
					ctx->accounts.store_flags = (uint32_t *)realloc(ctx->accounts.store_flags, new_cap * sizeof(uint32_t));
					ctx->accounts.capacity = new_cap;
				}

				if (folder_idx + 2 > ctx->folders.capacity) {
					uint32_t new_cap = ctx->folders.capacity * 2;
					while (folder_idx + 2 > new_cap) new_cap *= 2;
					ctx->folders.name_offsets = (uint32_t *)realloc(ctx->folders.name_offsets, new_cap * sizeof(uint32_t));
					ctx->folders.path_offsets = (uint32_t *)realloc(ctx->folders.path_offsets, new_cap * sizeof(uint32_t));
					ctx->folders.unread_counts = (uint32_t *)realloc(ctx->folders.unread_counts, new_cap * sizeof(uint32_t));
					ctx->folders.flags = (uint32_t *)realloc(ctx->folders.flags, new_cap * sizeof(uint32_t));
					ctx->folders.parent_idx = (int32_t *)realloc(ctx->folders.parent_idx, new_cap * sizeof(int32_t));
					ctx->folders.first_child_idx = (int32_t *)realloc(ctx->folders.first_child_idx, new_cap * sizeof(uint32_t));
					ctx->folders.next_sibling_idx = (int32_t *)realloc(ctx->folders.next_sibling_idx, new_cap * sizeof(uint32_t));
					ctx->folders.htree_items = (HTREEITEM *)realloc(ctx->folders.htree_items, new_cap * sizeof(HTREEITEM));
					ctx->folders.capacity = new_cap;
				}

				name_off = pool_add_wstr(&ctx->pool, name_buf);
				serv_off = pool_add_wstr(&ctx->pool, serv_buf);
				user_off = pool_add_wstr(&ctx->pool, user_buf);

				inbox_name_off = pool_add_wstr(&ctx->pool, L"Inbox");
				sent_name_off = pool_add_wstr(&ctx->pool, L"Sent");

				inbox_idx = (int32_t)folder_idx++;
				sent_idx = (int32_t)folder_idx++;

				ctx->accounts.name_offsets[acc_idx] = name_off;
				ctx->accounts.server_offsets[acc_idx] = serv_off;
				ctx->accounts.username_offsets[acc_idx] = user_off;
				ctx->accounts.ports[acc_idx] = (uint32_t)port;
				ctx->accounts.root_folder_idx[acc_idx] = inbox_idx;
				ctx->accounts.store_flags[acc_idx] = (_wcsicmp(store_buf, L"ram") == 0) ? 1 : 1;
				acc_idx++;

				ctx->folders.name_offsets[inbox_idx] = inbox_name_off;
				ctx->folders.path_offsets[inbox_idx] = inbox_name_off;
				ctx->folders.unread_counts[inbox_idx] = 1;
				ctx->folders.flags[inbox_idx] = SM_FOLDER_INBOX;
				ctx->folders.parent_idx[inbox_idx] = INVALID_INDEX;
				ctx->folders.first_child_idx[inbox_idx] = INVALID_INDEX;
				ctx->folders.next_sibling_idx[inbox_idx] = sent_idx;
				ctx->folders.count++;

				ctx->folders.name_offsets[sent_idx] = sent_name_off;
				ctx->folders.path_offsets[sent_idx] = sent_name_off;
				ctx->folders.unread_counts[sent_idx] = 0;
				ctx->folders.flags[sent_idx] = SM_FOLDER_SENT;
				ctx->folders.parent_idx[sent_idx] = INVALID_INDEX;
				ctx->folders.first_child_idx[sent_idx] = INVALID_INDEX;
				ctx->folders.next_sibling_idx[sent_idx] = INVALID_INDEX;
				ctx->folders.count++;
			}
			pSec += wcslen(pSec) + 1;
		}
	}

	ctx->accounts.count = acc_idx;
}


void mail_context_free(MailContext *ctx)
{
	if (!ctx) return;

	free(ctx->pool.data);
	free(ctx->accounts.name_offsets);
	free(ctx->accounts.server_offsets);
	free(ctx->accounts.username_offsets);
	free(ctx->accounts.ports);
	free(ctx->accounts.root_folder_idx);
	free(ctx->accounts.store_flags);
	free(ctx->folders.name_offsets);
	free(ctx->folders.path_offsets);
	free(ctx->folders.unread_counts);
	free(ctx->folders.flags);
	free(ctx->folders.parent_idx);
	free(ctx->folders.first_child_idx);
	free(ctx->folders.next_sibling_idx);
	free(ctx->folders.htree_items);
	memset(ctx, 0, sizeof(MailContext));
}
