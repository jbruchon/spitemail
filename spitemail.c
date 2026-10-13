/* Spite Mail main program
 * license: GPLv2; (C) 2026 Jody Bruchon <jody@jodybruchon.com> */

#include "version.h"
#include "spitemail.h"
#include "datastore.h"


static HWND hwndToolbar = NULL;
static HWND hwndFolderTree = NULL;
static HWND hwndMessageList = NULL;
static HWND hwndReadingPane = NULL;
static MailContext g_mail_ctx;  /* global application mail context instance */
static int g_reading_pane_has_error = 0;
static HBRUSH hbrErrorBg = NULL;

/* Default color preferences (overridden by INI if present) */
static COLORREF g_color_text         = RGB(  0,   0,   0);
static COLORREF g_color_error_bg     = RGB(255, 192, 192);
static COLORREF g_color_error_bg_hi  = RGB(255, 224, 224);
static COLORREF g_color_error_hi     = RGB(255, 160, 160);
static COLORREF g_color_error_text   = RGB(  0,   0,   0);
static COLORREF g_color_inactive_sel = RGB(240, 240, 255);
static COLORREF g_color_zebra_odd    = RGB(240, 240, 240);
static COLORREF g_color_zebra_even   = RGB(255, 255, 255);


/* Add wide strings directly to the UTF-16 string pool */
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


/* Translate hex color to Windows RGB values */
static COLORREF parse_hex_color(const wchar_t *hex_str, COLORREF default_color)
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

	/* Allocate initial capacities for SoA tables */
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

	/* Resolve path to spitemail.ini beside spitemail.exe */
	GetModuleFileNameW(NULL, config_path, MAX_PATH);
	{
		wchar_t *last_slash = wcsrchr(config_path, L'\\');

		if (last_slash) {
			wcscpy_s(last_slash + 1, MAX_PATH - (last_slash + 1 - config_path), L"spitemail.ini");
		} else {
			wcscpy_s(config_path, MAX_PATH, L".\\spitemail.ini");
		}
	}

	/* Read global color preferences once */
	{
		wchar_t color_buf[32];

		GetPrivateProfileStringW(L"Preferences", L"TextColor", L"", color_buf, 32, config_path);
		if (color_buf[0] != L'\0') g_color_text = parse_hex_color(color_buf, g_color_text);

		GetPrivateProfileStringW(L"Preferences", L"ErrorBgColor", L"", color_buf, 32, config_path);
		if (color_buf[0] != L'\0') g_color_error_bg = parse_hex_color(color_buf, g_color_error_bg);

		GetPrivateProfileStringW(L"Preferences", L"ErrorBgHighlightColor", L"", color_buf, 32, config_path);
		if (color_buf[0] != L'\0') g_color_error_bg_hi = parse_hex_color(color_buf, g_color_error_bg_hi);

		GetPrivateProfileStringW(L"Preferences", L"ErrorHighlightColor", L"", color_buf, 32, config_path);
		if (color_buf[0] != L'\0') g_color_error_hi = parse_hex_color(color_buf, g_color_error_hi);

		GetPrivateProfileStringW(L"Preferences", L"ErrorTextColor", L"", color_buf, 32, config_path);
		if (color_buf[0] != L'\0') g_color_error_text = parse_hex_color(color_buf, g_color_error_text);

		GetPrivateProfileStringW(L"Preferences", L"InactiveSelColor", L"", color_buf, 32, config_path);
		if (color_buf[0] != L'\0') g_color_inactive_sel = parse_hex_color(color_buf, g_color_inactive_sel);

		GetPrivateProfileStringW(L"Preferences", L"ZebraOddColor", L"", color_buf, 32, config_path);
		if (color_buf[0] != L'\0') g_color_zebra_odd = parse_hex_color(color_buf, g_color_zebra_odd);

		GetPrivateProfileStringW(L"Preferences", L"ZebraEvenColor", L"", color_buf, 32, config_path);
		if (color_buf[0] != L'\0') g_color_zebra_even = parse_hex_color(color_buf, g_color_zebra_even);
	}

	/* Read section names */
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

				/* Reallocate account arrays if needed */
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

				/* Reallocate folder arrays if needed */
				if (folder_idx + 2 > ctx->folders.capacity) {
					uint32_t new_cap = ctx->folders.capacity * 2;
					while (folder_idx + 2 > new_cap) new_cap *= 2;
					ctx->folders.name_offsets = (uint32_t *)realloc(ctx->folders.name_offsets, new_cap * sizeof(uint32_t));
					ctx->folders.path_offsets = (uint32_t *)realloc(ctx->folders.path_offsets, new_cap * sizeof(uint32_t));
					ctx->folders.unread_counts = (uint32_t *)realloc(ctx->folders.unread_counts, new_cap * sizeof(uint32_t));
					ctx->folders.flags = (uint32_t *)realloc(ctx->folders.flags, new_cap * sizeof(uint32_t));
					ctx->folders.parent_idx = (int32_t *)realloc(ctx->folders.parent_idx, new_cap * sizeof(uint32_t));
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

				/* Populate Account SoA */
				ctx->accounts.name_offsets[acc_idx] = name_off;
				ctx->accounts.server_offsets[acc_idx] = serv_off;
				ctx->accounts.username_offsets[acc_idx] = user_off;
				ctx->accounts.ports[acc_idx] = (uint32_t)port;
				ctx->accounts.root_folder_idx[acc_idx] = inbox_idx;
				ctx->accounts.store_flags[acc_idx] = (_wcsicmp(store_buf, L"ram") == 0) ? 1 : 1;
				acc_idx++;

				/* Populate Inbox Folder */
				ctx->folders.name_offsets[inbox_idx] = inbox_name_off;
				ctx->folders.path_offsets[inbox_idx] = inbox_name_off;
				ctx->folders.unread_counts[inbox_idx] = 1;
				ctx->folders.flags[inbox_idx] = SM_FOLDER_INBOX;
				ctx->folders.parent_idx[inbox_idx] = INVALID_INDEX;
				ctx->folders.first_child_idx[inbox_idx] = INVALID_INDEX;
				ctx->folders.next_sibling_idx[inbox_idx] = sent_idx;
				ctx->folders.count++;

				/* Populate Sent Folder */
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


static HTREEITEM populate_folder_branch_w(HWND hwndTree, HTREEITEM hParent, 
                                          MailContext *ctx, int32_t folder_idx) 
{
	wchar_t *wname;
	TVINSERTSTRUCTW tvis;
	HTREEITEM hItem;
	int32_t child_idx;

	if (folder_idx == INVALID_INDEX) return NULL;

	wname = pool_get_wstr(&ctx->pool, ctx->folders.name_offsets[folder_idx]);

	memset(&tvis, 0, sizeof(tvis));
	tvis.hParent = hParent;
	tvis.hInsertAfter = TVI_LAST;
	tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
	tvis.item.pszText = (LPWSTR)wname;
	tvis.item.lParam = (LPARAM)(intptr_t)folder_idx; /* Index stored directly in lParam */

	hItem = TreeView_InsertItem(hwndTree, &tvis);
	ctx->folders.htree_items[folder_idx] = hItem;

	child_idx = ctx->folders.first_child_idx[folder_idx];
	while (child_idx != INVALID_INDEX) {
		populate_folder_branch_w(hwndTree, hItem, ctx, child_idx);
		child_idx = ctx->folders.next_sibling_idx[child_idx];
	}

	return hItem;
}


void tree_view_refresh(HWND hwndTree, MailContext *ctx) 
{
	uint32_t i;
	wchar_t *acc_name;
	TVINSERTSTRUCTW tvis_acc;
	HTREEITEM hAccItem;
	int32_t curr_f_idx;

	TreeView_DeleteAllItems(hwndTree);

	if (ctx->accounts.count == 0) {
		memset(&tvis_acc, 0, sizeof(tvis_acc));
		tvis_acc.hParent = TVI_ROOT;
		tvis_acc.hInsertAfter = TVI_LAST;
		tvis_acc.item.mask = TVIF_TEXT | TVIF_PARAM;
		tvis_acc.item.pszText = (LPWSTR)(uintptr_t)L"(no accounts)";
		tvis_acc.item.lParam = (LPARAM)(intptr_t)-1;

		TreeView_InsertItem(hwndTree, &tvis_acc);
		return;
	}


	for (i = 0; i < ctx->accounts.count; i++) {
		acc_name = pool_get_wstr(&ctx->pool, ctx->accounts.name_offsets[i]);

		memset(&tvis_acc, 0, sizeof(tvis_acc));
		tvis_acc.hParent = TVI_ROOT;
		tvis_acc.hInsertAfter = TVI_LAST;
		tvis_acc.item.mask = TVIF_TEXT | TVIF_PARAM;
		tvis_acc.item.pszText = (LPWSTR)acc_name;
		tvis_acc.item.lParam = (LPARAM)(intptr_t)-1;

		hAccItem = TreeView_InsertItem(hwndTree, &tvis_acc);

		curr_f_idx = ctx->accounts.root_folder_idx[i];
		while (curr_f_idx != INVALID_INDEX) {
			populate_folder_branch_w(hwndTree, hAccItem, ctx, curr_f_idx);
			curr_f_idx = ctx->folders.next_sibling_idx[curr_f_idx];
		}

		TreeView_Expand(hwndTree, hAccItem, TVE_EXPAND);
	}
}


/* For sorting the message list */
static int CALLBACK MessageListCompare(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	MessageListItem *item1 = (MessageListItem *)lParam1;
	MessageListItem *item2 = (MessageListItem *)lParam2;

	int col = (lParamSort >> 1) & 0xFF;
	int ascending = lParamSort & 1;
	int cmp = 0;

	if (!item1 || !item2) return 0;

	switch (col) {
		case 0:
			cmp = _wcsicmp(item1->subject, item2->subject);
			break;
		case 1:
			cmp = _wcsicmp(item1->participant, item2->participant);
			break;
		case 2:
			if (item1->timestamp < item2->timestamp) cmp = -1;
			else if (item1->timestamp > item2->timestamp) cmp = 1;
			else cmp = 0;
			break;
		default:
			return 0;
	}

	return ascending ? cmp : -cmp;
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CREATESTRUCT *pCreate;
	HINSTANCE hInstance;
	int wmId;
	RECT rc;
	int cx;
	int cy;
	int tbHeight;
	int contentTop;
	int contentHeight;
	static int col1Width;
	static int col2Width;
	static int col3Width;
	static int mp_subject_width = SM_MP_SUBJECT_WIDTH;
	static int mp_fromto_width = SM_MP_FROMTO_WIDTH;
	static int mp_date_width = SM_MP_DATE_WIDTH;
	HDWP hdwp;
	MINMAXINFO *minMax;
	LVCOLUMNW lvc;
	LVITEMW lvi;

	switch (uMsg) {
		case WM_GETMINMAXINFO:
			minMax = (MINMAXINFO *)lParam;
			minMax->ptMinTrackSize.x = SM_MIN_WINDOW_WIDTH;
			minMax->ptMinTrackSize.y = SM_MIN_WINDOW_HEIGHT;
			return 0;
			/* End of WM_GETMINMAXINFO */

		case WM_CREATE:
			pCreate = (CREATESTRUCT *)lParam;
			hInstance = pCreate->hInstance;

			hwndToolbar = CreateWindowEx(
				0, L"STATIC", L"Toolbar", WS_CHILD | WS_VISIBLE | WS_BORDER,
				0, 0, 0, 0, hwnd, (HMENU)1, hInstance, NULL
			);

			hwndFolderTree = CreateWindowEx(
				0, WC_TREEVIEW, L"Folders",
				WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP,
				0, 0, 0, 0, hwnd, (HMENU)2, hInstance, NULL
			);

			hwndMessageList = CreateWindowEx(
				0, WC_LISTVIEW, L"Messages",
				WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP,
				0, 0, 0, 0, hwnd, (HMENU)3, hInstance, NULL
			);
			/* Message list selections should span all columns */
			ListView_SetExtendedListViewStyle(hwndMessageList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
 
			/* Disable visual styles on the header control to restore classic 3D appearance */
			{
				HWND hwndHeader = ListView_GetHeader(hwndMessageList);
				if (hwndHeader) SetWindowTheme(hwndHeader, L"", L"");
			}

			memset(&lvc, 0, sizeof(lvc));
			lvc.mask = LVCF_TEXT | LVCF_WIDTH;

			lvc.pszText = (LPWSTR)(uintptr_t)L"Subject";
			lvc.cx = mp_subject_width;
			ListView_InsertColumn(hwndMessageList, 0, &lvc);

			lvc.pszText = (LPWSTR)(uintptr_t)L"From";
			lvc.cx = mp_fromto_width;
			ListView_InsertColumn(hwndMessageList, 1, &lvc);

			lvc.pszText = (LPWSTR)(uintptr_t)L"Date";
			lvc.cx = mp_date_width;
			ListView_InsertColumn(hwndMessageList, 2, &lvc);

			hwndReadingPane = CreateWindowEx(
				0, L"EDIT", L"Reading Pane",
				WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_BORDER | WS_TABSTOP,
				0, 0, 0, 0, hwnd, (HMENU)4, hInstance, NULL
			);

			/* Load INI configuration */
			load_app_configuration(&g_mail_ctx);

			/* Create a pink brush for highlighting error text */
			hbrErrorBg = CreateSolidBrush(g_color_error_bg);

			/* Initialize data backing store */
			store_init(&g_mail_ctx);
			tree_view_refresh(hwndFolderTree, &g_mail_ctx);

			/* Select the Inbox (index 0) by default on launch if accounts exist */
			if (g_mail_ctx.accounts.count > 0) {
				TreeView_SelectItem(hwndFolderTree, g_mail_ctx.folders.htree_items[0]);
			}

			return 0;
			/* End of WM_CREATE */

		case WM_CTLCOLORSTATIC:
			if ((HWND)lParam == hwndReadingPane && g_reading_pane_has_error) {
				SetBkColor((HDC)wParam, g_color_error_bg_hi);
				return (INT_PTR)hbrErrorBg;
			}
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
			/* End of WM_CTLCOLORSTATIC */

		case WM_COMMAND:
			wmId = LOWORD(wParam);
			switch (wmId) {
				case IDM_FILE_EXIT:
					DestroyWindow(hwnd);
					break;
				case IDM_HELP_ABOUT:
					wchar_t aboutText[512];
					swprintf(aboutText, sizeof(aboutText) / sizeof(wchar_t),
						L"Spite Mail: made with hate\nVersion %S (%S)\n(C) 2026 Jody Bruchon <jody@jodybruchon.com>\nPublished under GNU GPL v2",
						VER, VERDATE
						);
					MessageBox(hwnd, aboutText, L"About Spite Mail", MB_OK | MB_ICONINFORMATION);
					break;
				default:
					return DefWindowProc(hwnd, uMsg, wParam, lParam);
			}
			return 0;
			/* End of WM_COMMAND */

		case WM_SIZE:
			GetClientRect(hwnd, &rc);
			cx = rc.right - rc.left;
			cy = rc.bottom - rc.top;

			/* Position the top toolbar, 30px */
			tbHeight = 30;
			/* Calculate remaining vertical space for panes */
			contentTop = tbHeight;
			contentHeight = cy - contentTop;

			/* Divide horizontal across three vertical panes */
			col1Width = SM_FOLDER_PANE_WIDTH;
			col2Width = SM_MESSAGE_PANE_WIDTH;
			/* Flexible resize if width drops below standard width */
			if (cx < SM_WINDOW_WIDTH) {
				col1Width = (int)(((unsigned long)cx * SM_FOLDER_PANE_WIDTH) / SM_WINDOW_WIDTH);
				col2Width = (int)(((unsigned long)cx * SM_MESSAGE_PANE_WIDTH) / SM_WINDOW_WIDTH);
				if (col1Width < SM_MIN_FOLDER_PANE_WIDTH) col1Width = SM_MIN_FOLDER_PANE_WIDTH;
				if (col2Width < SM_MIN_MESSAGE_PANE_WIDTH) col2Width = SM_MIN_MESSAGE_PANE_WIDTH;
			}

			/* Size the reading pane columns */


			col3Width = cx - col1Width - col2Width;
			if (col3Width < SM_MIN_READING_PANE_WIDTH) col3Width = SM_MIN_READING_PANE_WIDTH;

			hdwp = BeginDeferWindowPos(3);
			MoveWindow(hwndToolbar, 0, 0, cx, tbHeight, TRUE);
			MoveWindow(hwndFolderTree, 0, contentTop, col1Width, contentHeight, TRUE);
			MoveWindow(hwndMessageList, col1Width, contentTop, col2Width, contentHeight, TRUE);
			MoveWindow(hwndReadingPane, col1Width + col2Width, contentTop, col3Width, contentHeight, TRUE);
			EndDeferWindowPos(hdwp);
			return 0;
			/* End of WM_SIZE */

		case WM_NOTIFY:
			{
				NMHDR *pNmhdr;
				NMTREEVIEWW *pnmtv;
				NMLISTVIEW *pnmv;
				int32_t folder_idx;
				uint32_t folder_flags;
				LVCOLUMNW lvcCol;
				StoreMsgHeader *headers = NULL;
				uint32_t msg_count = 0;
				uint32_t i;
				int row;

				pNmhdr = (NMHDR *)lParam;

				/* Free MessageListItem allocations when items are deleted */
				if (pNmhdr->hwndFrom == hwndMessageList && pNmhdr->code == LVN_DELETEITEM) {
					pnmv = (NMLISTVIEW *)lParam;
					if (pnmv->lParam) free((void *)pnmv->lParam);
					return 0;
				}

				/* Handle message selection change to update reading pane */
				if (pNmhdr->hwndFrom == hwndMessageList && pNmhdr->code == LVN_ITEMCHANGED) {
					pnmv = (NMLISTVIEW *)lParam;

					if ((pnmv->uChanged & LVIF_STATE) && (pnmv->uNewState & LVIS_SELECTED) && !(pnmv->uOldState & LVIS_SELECTED)) {
						MessageListItem *item = (MessageListItem *)pnmv->lParam;
						if (item) {
							wchar_t *eml_data = NULL;
							size_t eml_size = 0;
							if (store_fetch_message(item->id, &eml_data, &eml_size) == 0 && eml_data) {
								g_reading_pane_has_error = 0;
								SetWindowTextW(hwndReadingPane, eml_data);
								store_free_message(eml_data);
							} else {
								g_reading_pane_has_error = 1;
								SetWindowTextW(hwndReadingPane, L"\r\n  <error: can't load message content>\r\n");
							}
						}
					} else if ((pnmv->uChanged & LVIF_STATE) && !(pnmv->uNewState & LVIS_SELECTED) && (pnmv->uOldState & LVIS_SELECTED)) {
						/* Clear pane if item is deselected */
						g_reading_pane_has_error = 0;
						SetWindowTextW(hwndReadingPane, L"");
						InvalidateRect(hwndReadingPane, NULL, TRUE);
					}
					return 0;
				}

				/* Column header clicks for sorting */
				if (pNmhdr->hwndFrom == hwndMessageList && pNmhdr->code == LVN_COLUMNCLICK) {
					pnmv = (NMLISTVIEW *)lParam;
					static int last_col = -1;
					static int ascending = 1;

					if (last_col == pnmv->iSubItem) {
						ascending = !ascending;
					} else {
						last_col = pnmv->iSubItem;
						ascending = 1;
					}

					LPARAM sortParam = (last_col << 1) | (ascending ? 1 : 0);
					ListView_SortItems(hwndMessageList, MessageListCompare, sortParam);

					HWND hwndHeader = ListView_GetHeader(hwndMessageList);
					if (hwndHeader) {
						int colCount = Header_GetItemCount(hwndHeader);
						for (i = 0; i < (uint32_t)colCount; i++) {
							HDITEMW hdi;
							memset(&hdi, 0, sizeof(hdi));
							hdi.mask = HDI_FORMAT;
							Header_GetItem(hwndHeader, i, &hdi);

							if ((int)i == last_col) {
								hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
								hdi.fmt |= ascending ? HDF_SORTUP : HDF_SORTDOWN;
							} else {
								hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
							}
							Header_SetItem(hwndHeader, i, &hdi);
						}
					}
					return 0;
				}

				/* Folder tree selection change: dynamic message list load */
				if (pNmhdr->hwndFrom == hwndFolderTree && pNmhdr->code == TVN_SELCHANGEDW) {
					pnmtv = (NMTREEVIEWW *)lParam;
					folder_idx = (int32_t)(intptr_t)pnmtv->itemNew.lParam;

					if (folder_idx == INVALID_INDEX) return 0;

					folder_flags = g_mail_ctx.folders.flags[folder_idx];

					/* Clear list view items and reading pane text */
					ListView_DeleteAllItems(hwndMessageList);
					SetWindowTextW(hwndReadingPane, L"");

					/* Dynamic column labeling */
					memset(&lvcCol, 0, sizeof(lvcCol));
					lvcCol.mask = LVCF_TEXT;
					lvcCol.pszText = ISFLAG(folder_flags, SM_FOLDER_SENT) ? (LPWSTR)(uintptr_t)L"To" : (LPWSTR)(uintptr_t)L"From";
					ListView_SetColumn(hwndMessageList, 1, &lvcCol);

					/* Query data store for messages in folder */
					if (store_fetch_folder_messages(folder_idx, &headers, &msg_count) == 0 && headers) {
						for (i = 0; i < msg_count; i++) {
							MessageListItem *mItem = (MessageListItem *)malloc(sizeof(MessageListItem));
							if (!mItem) continue;

							mItem->id = headers[i].id;
							wcscpy_s(mItem->subject, 128, headers[i].subject);
							wcscpy_s(mItem->participant, 128, headers[i].participant);
							wcscpy_s(mItem->date_str, 32, headers[i].date_str);
							mItem->timestamp = headers[i].timestamp;

							memset(&lvi, 0, sizeof(lvi));
							lvi.mask = LVIF_TEXT | LVIF_PARAM;
							lvi.iItem = (int)i;
							lvi.iSubItem = 0;
							lvi.pszText = mItem->subject;
							lvi.lParam = (LPARAM)mItem;

							row = ListView_InsertItem(hwndMessageList, &lvi);
							ListView_SetItemText(hwndMessageList, row, 1, mItem->participant);
							ListView_SetItemText(hwndMessageList, row, 2, mItem->date_str);
						}

						store_free_folder_messages(headers);
					}

					/* Default sort: Date column (index 2), Descending */
					{
						LPARAM defaultSortParam = (2 << 1) | 0;
						ListView_SortItems(hwndMessageList, MessageListCompare, defaultSortParam);

						HWND hwndHeader = ListView_GetHeader(hwndMessageList);
						if (hwndHeader) {
							int colCount = Header_GetItemCount(hwndHeader);
							for (i = 0; i < (uint32_t)colCount; i++) {
								HDITEMW hdi;
								memset(&hdi, 0, sizeof(hdi));
								hdi.mask = HDI_FORMAT;
								Header_GetItem(hwndHeader, i, &hdi);

								if (i == 2) {
									hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
									hdi.fmt |= HDF_SORTDOWN;
								} else {
									hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
								}
								Header_SetItem(hwndHeader, i, &hdi);
							}
						}
					}

					/* Automatically select the first item in the sorted list */
					if (ListView_GetItemCount(hwndMessageList) > 0) {
						ListView_SetItemState(hwndMessageList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
					}
					return 0;
				}

				/* Alternating row colors and custom inactive selection color via Custom Draw */
				if (pNmhdr->hwndFrom == hwndMessageList && pNmhdr->code == NM_CUSTOMDRAW) {
					NMLVCUSTOMDRAW *pLVCD = (NMLVCUSTOMDRAW *)lParam;
					switch (pLVCD->nmcd.dwDrawStage) {
						case CDDS_PREPAINT:
							return CDRF_NOTIFYITEMDRAW;

						case CDDS_ITEMPREPAINT:
							return CDRF_NOTIFYSUBITEMDRAW;

						case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
						{
							int is_selected = (ListView_GetItemState(hwndMessageList, pLVCD->nmcd.dwItemSpec, LVIS_SELECTED) != 0);
							int is_inactive = (GetFocus() != hwndMessageList);

							/* If loading a message returns an error, change list highlight color */
							if (is_selected && g_reading_pane_has_error) {
								pLVCD->nmcd.uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS);
								pLVCD->clrTextBk = g_color_error_hi;
								pLVCD->clrText = g_color_error_text;
								return CDRF_NEWFONT;
							}

							if (is_selected && is_inactive) {
								/* Custom inactive selection background (e.g., light gray/silver) */
								pLVCD->nmcd.uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS);
								pLVCD->clrTextBk = g_color_inactive_sel;
								pLVCD->clrText = g_color_text;
								return CDRF_NEWFONT;
							} else if (!is_selected) {
								/* Zebra striping for unselected rows */
								if (pLVCD->nmcd.dwItemSpec % 2 != 0) {
									pLVCD->clrTextBk = g_color_zebra_odd;
								} else {
									pLVCD->clrTextBk = g_color_zebra_even;
								}
								pLVCD->clrText = g_color_text;
								return CDRF_NEWFONT;
							}
							return CDRF_DODEFAULT;
						}

						default:
							break;
					}
					return CDRF_DODEFAULT;
				}
			}  /* End of WM_NOTIFY */
			return 0;

		case WM_DESTROY:
			if (hbrErrorBg) DeleteObject(hbrErrorBg);
			mail_context_free(&g_mail_ctx);
			store_cleanup();
			PostQuitMessage(0);
			return 0;

		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}


int WINAPI wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
	const wchar_t CLASS_NAME[] = L"SpiteMailWindowClass";
	HMENU hMenu;
	INITCOMMONCONTROLSEX icc;
	MSG msg;
	HWND hwndFocus;
	BOOL bShift;
	HWND hwndNext;

	(void)hPrevInstance;
	(void)lpCmdLine;

	icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icc.dwICC = ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&icc);

	WNDCLASSEX wc = { 0 };
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.hIcon   = LoadIcon(hInstance, MAKEINTRESOURCE(2));
	wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(2));

	if (!RegisterClassEx(&wc)) return 0;

	HWND hwnd = CreateWindowEx(
		0, CLASS_NAME, L"Spite Mail", WS_OVERLAPPEDWINDOW,
		0, 0, SM_WINDOW_WIDTH, SM_WINDOW_HEIGHT,
		NULL, NULL, hInstance, NULL
	);

	if (hwnd == NULL) return 0;

	/* Connect the menu */
	hMenu = LoadMenu(hInstance, MAKEINTRESOURCE(3));
	SetMenu(hwnd, hMenu);

	/* Force explicit icon assignment */
	HICON hIconLarge = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(2), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
	HICON hIconSmall = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(2), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconLarge);
	SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

	ShowWindow(hwnd, nShowCmd);
	UpdateWindow(hwnd);

	memset(&msg, 0, sizeof(msg));
	while (GetMessage(&msg, NULL, 0, 0)) {
		/* Intercept TAB and handle it ourselves */
		if (msg.message == WM_KEYDOWN && msg.wParam == VK_TAB) {
			hwndFocus = GetFocus();
			bShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
			hwndNext = GetNextDlgTabItem(hwnd, hwndFocus, bShift);
			if (hwndNext) {
				SetFocus(hwndNext);
				continue;
			}
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}
