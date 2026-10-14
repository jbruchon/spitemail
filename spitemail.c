/* Spite Mail core data and configuration engine
 * license: GPLv2; (C) 2026 Jody Bruchon <jody@jodybruchon.com> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "version.h"
#include "spitemail.h"
#include "datastore.h"

MailContext g_mail_ctx;  /* global application mail context instance */


uint32_t pool_add_str(StringPool *pool, const char *str)
{
	uint32_t req_chars;
	uint32_t offset;
	uint32_t new_cap;
	char *new_data;

	if (!str || *str == '\0') return 0;

	req_chars = (uint32_t)strlen(str) + 1;

	if (pool->used + req_chars > pool->capacity) {
		new_cap = pool->capacity ? pool->capacity * 2 : 4096;
		while (pool->used + req_chars > new_cap) {
			new_cap *= 2;
		}

		new_data = (char *)realloc(pool->data, new_cap);
		if (!new_data) return 0;

		pool->data = new_data;
		pool->capacity = new_cap;
	}

	offset = pool->used;
	strcpy_s(pool->data + offset, pool->capacity - offset, str);
	pool->used += req_chars;

	return offset;
}


char *pool_get_str(const StringPool *pool, uint32_t offset)
{
	return pool->data + offset;
}


static void trim_whitespace(char *str)
{
	char *end;
	while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n') str++;
	if (*str == '\0') return;
	end = str + strlen(str) - 1;
	while (end > str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
		*end = '\0';
		end--;
	}
}


static int parse_hex_color_agnostic(const char *hex_str, ColorRGB *out_color)
{
	unsigned int r = 0, g = 0, b = 0;
	if (!hex_str || !out_color) return -1;
	if (*hex_str == '#') hex_str++;
	if (strlen(hex_str) != 6) return -1;

	if (sscanf(hex_str, "%2x%2x%2x", &r, &g, &b) == 3) {
		out_color->r = (uint8_t)r;
		out_color->g = (uint8_t)g;
		out_color->b = (uint8_t)b;
		return 0;
	}
	return -1;
}


static void add_parsed_account(MailContext *ctx, const char *name, const char *server,
                               const char *username, const char *store, int port,
                               uint32_t *acc_idx, uint32_t *folder_idx)
{
	uint32_t a_idx = *acc_idx;
	uint32_t f_idx = *folder_idx;

	/* Reallocate account arrays if needed */
	if (a_idx >= ctx->accounts.capacity) {
		uint32_t new_cap = ctx->accounts.capacity * 2;
		ctx->accounts.name_offsets = (uint32_t *)realloc(ctx->accounts.name_offsets, new_cap * sizeof(uint32_t));
		ctx->accounts.server_offsets = (uint32_t *)realloc(ctx->accounts.server_offsets, new_cap * sizeof(uint32_t));
		ctx->accounts.username_offsets = (uint32_t *)realloc(ctx->accounts.username_offsets, new_cap * sizeof(uint32_t));
		ctx->accounts.ports = (uint32_t *)realloc(ctx->accounts.ports, new_cap * sizeof(uint32_t));
		ctx->accounts.root_folder_idx = (int32_t *)realloc(ctx->accounts.root_folder_idx, new_cap * sizeof(int32_t));
		ctx->accounts.store_flags = (uint32_t *)realloc(ctx->accounts.store_flags, new_cap * sizeof(uint32_t));
		ctx->accounts.capacity = new_cap;
	}

	/* Reallocate folder arrays if needed */
	if (f_idx + 2 > ctx->folders.capacity) {
		uint32_t new_cap = ctx->folders.capacity * 2;
		while (f_idx + 2 > new_cap) new_cap *= 2;
		ctx->folders.name_offsets = (uint32_t *)realloc(ctx->folders.name_offsets, new_cap * sizeof(uint32_t));
		ctx->folders.path_offsets = (uint32_t *)realloc(ctx->folders.path_offsets, new_cap * sizeof(uint32_t));
		ctx->folders.unread_counts = (uint32_t *)realloc(ctx->folders.unread_counts, new_cap * sizeof(uint32_t));
		ctx->folders.flags = (uint32_t *)realloc(ctx->folders.flags, new_cap * sizeof(uint32_t));
		ctx->folders.parent_idx = (int32_t *)realloc(ctx->folders.parent_idx, new_cap * sizeof(int32_t));
		ctx->folders.first_child_idx = (int32_t *)realloc(ctx->folders.first_child_idx, new_cap * sizeof(uint32_t));
		ctx->folders.next_sibling_idx = (int32_t *)realloc(ctx->folders.next_sibling_idx, new_cap * sizeof(uint32_t));
		ctx->folders.capacity = new_cap;
	}

	uint32_t name_off = pool_add_str(&ctx->pool, name);
	uint32_t serv_off = pool_add_str(&ctx->pool, server);
	uint32_t user_off = pool_add_str(&ctx->pool, username);
	uint32_t inbox_off = pool_add_str(&ctx->pool, "Inbox");
	uint32_t sent_off = pool_add_str(&ctx->pool, "Sent");

	int32_t inbox_idx = (int32_t)f_idx++;
	int32_t sent_idx = (int32_t)f_idx++;

	ctx->accounts.name_offsets[a_idx] = name_off;
	ctx->accounts.server_offsets[a_idx] = serv_off;
	ctx->accounts.username_offsets[a_idx] = user_off;
	ctx->accounts.ports[a_idx] = (uint32_t)port;
	ctx->accounts.root_folder_idx[a_idx] = inbox_idx;
	ctx->accounts.store_flags[a_idx] = (_stricmp(store, "ram") == 0) ? 1 : 1;
	(*acc_idx)++;

	ctx->folders.name_offsets[inbox_idx] = inbox_off;
	ctx->folders.path_offsets[inbox_idx] = inbox_off;
	ctx->folders.unread_counts[inbox_idx] = 1;
	ctx->folders.flags[inbox_idx] = SM_FOLDER_INBOX;
	ctx->folders.parent_idx[inbox_idx] = INVALID_INDEX;
	ctx->folders.first_child_idx[inbox_idx] = INVALID_INDEX;
	ctx->folders.next_sibling_idx[inbox_idx] = sent_idx;
	ctx->folders.count++;

	ctx->folders.name_offsets[sent_idx] = sent_off;
	ctx->folders.path_offsets[sent_idx] = sent_off;
	ctx->folders.unread_counts[sent_idx] = 0;
	ctx->folders.flags[sent_idx] = SM_FOLDER_SENT;
	ctx->folders.parent_idx[sent_idx] = INVALID_INDEX;
	ctx->folders.first_child_idx[sent_idx] = INVALID_INDEX;
	ctx->folders.next_sibling_idx[sent_idx] = INVALID_INDEX;
	ctx->folders.count++;

	*folder_idx = f_idx;
}


void load_app_configuration(MailContext *ctx)
{
	FILE *fp = NULL;
	char line[512];
	char current_section[128] = "";

	char acc_name[128] = "user@spitemail.com";
	char acc_server[128] = "imap.spitemail.com";
	char acc_user[128] = "user@spitemail.com";
	char acc_store[32] = "ram";
	int acc_port = 993;
	int has_active_account = 0;

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

	/* Default platform-agnostic theme colors */
	ctx->theme.text         = (ColorRGB){  0,   0,   0 };
	ctx->theme.error_bg     = (ColorRGB){ 255, 192, 192 };
	ctx->theme.error_bg_hi  = (ColorRGB){ 255, 224, 224 };
	ctx->theme.error_hi     = (ColorRGB){ 255, 160, 160 };
	ctx->theme.error_text   = (ColorRGB){  0,   0,   0 };
	ctx->theme.inactive_sel = (ColorRGB){ 240, 240, 255 };
	ctx->theme.zebra_odd    = (ColorRGB){ 240, 240, 240 };
	ctx->theme.zebra_even   = (ColorRGB){ 255, 255, 255 };

	fp = fopen("spitemail.ini", "r");
	if (!fp) fp = fopen("./config/spitemail.ini", "r");

	if (!fp) {
		add_parsed_account(ctx, "Default Account", "localhost", "default", "ram", 993, &acc_idx, &folder_idx);
		ctx->accounts.count = acc_idx;
		return;
	}

	while (fgets(line, sizeof(line), fp) != NULL) {
		char *p = line;
		trim_whitespace(p);

		if (*p == '\0' || *p == ';' || *p == '#') continue;

		if (*p == '[') {
			if (has_active_account) {
				add_parsed_account(ctx, acc_name, acc_server, acc_user, acc_store, acc_port, &acc_idx, &folder_idx);
				has_active_account = 0;
			}

			char *end_bracket = strchr(p, ']');
			if (end_bracket) {
				*end_bracket = '\0';
				strcpy_s(current_section, sizeof(current_section), p + 1);
				trim_whitespace(current_section);
			}

			if (_stricmp(current_section, "Account") == 0 || _strnicmp(current_section, "Account ", 8) == 0) {
				has_active_account = 1;
				strcpy_s(acc_name, sizeof(acc_name), "user@spitemail.com");
				strcpy_s(acc_server, sizeof(acc_server), "imap.spitemail.com");
				strcpy_s(acc_user, sizeof(acc_user), "user@spitemail.com");
				strcpy_s(acc_store, sizeof(acc_store), "ram");
				acc_port = 993;
			}
			continue;
		}

		char *eq = strchr(p, '=');
		if (eq) {
			*eq = '\0';
			char *key = p;
			char *val = eq + 1;
			trim_whitespace(key);
			trim_whitespace(val);

			if (_stricmp(current_section, "Account") == 0 || _strnicmp(current_section, "Account ", 8) == 0) {
				has_active_account = 1;
				if (_stricmp(key, "Name") == 0) strcpy_s(acc_name, sizeof(acc_name), val);
				else if (_stricmp(key, "Server") == 0) strcpy_s(acc_server, sizeof(acc_server), val);
				else if (_stricmp(key, "Username") == 0) strcpy_s(acc_user, sizeof(acc_user), val);
				else if (_stricmp(key, "Store") == 0) strcpy_s(acc_store, sizeof(acc_store), val);
				else if (_stricmp(key, "Port") == 0) acc_port = atoi(val);
			}
			else if (_stricmp(current_section, "Preferences") == 0) {
				ColorRGB rgb;
				if (parse_hex_color_agnostic(val, &rgb) == 0) {
					if (_stricmp(key, "TextColor") == 0) ctx->theme.text = rgb;
					else if (_stricmp(key, "ErrorBgColor") == 0) ctx->theme.error_bg = rgb;
					else if (_stricmp(key, "ErrorBgHighlightColor") == 0) ctx->theme.error_bg_hi = rgb;
					else if (_stricmp(key, "ErrorHighlightColor") == 0) ctx->theme.error_hi = rgb;
					else if (_stricmp(key, "ErrorTextColor") == 0) ctx->theme.error_text = rgb;
					else if (_stricmp(key, "InactiveSelColor") == 0) ctx->theme.inactive_sel = rgb;
					else if (_stricmp(key, "ZebraOddColor") == 0) ctx->theme.zebra_odd = rgb;
					else if (_stricmp(key, "ZebraEvenColor") == 0) ctx->theme.zebra_even = rgb;
				}
			}
		}
	}

	if (has_active_account) {
		add_parsed_account(ctx, acc_name, acc_server, acc_user, acc_store, acc_port, &acc_idx, &folder_idx);
	}

	fclose(fp);
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
	memset(ctx, 0, sizeof(MailContext));
}
