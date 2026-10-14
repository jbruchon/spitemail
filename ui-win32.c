/* Spite Mail user interface for the Win32 API
 * license: GPLv2; (C) 2026 Jody Bruchon <jody@jodybruchon.com> */

#include "version.h"
#include "spitemail.h"
#include "datastore.h"
#include "ui-win32.h"


/*** Variables and type definitions ***/
static HWND hwndToolbar = NULL;
static HWND hwndFolderTree = NULL;
static HWND hwndMessageList = NULL;
static HWND hwndReadingPane = NULL;
static int g_reading_pane_state = 0;
static HBRUSH hbrTextBg = NULL;
static HBRUSH hbrErrorBg = NULL;
static HBRUSH hbrNoMsgBg = NULL;

static HTREEITEM *g_ui_htree_items = NULL;
static uint32_t g_ui_htree_capacity = 0;

static const wchar_t *rp_nomsg = L"\r\n  <no message selected>";

/* Toolbar button configuration */
typedef struct ToolbarButtonSpec {
	int id;
	const wchar_t *label;
	int default_width;
	int show_text;
	int show_icon;
	HWND hwnd;
} ToolbarButtonSpec;

static ToolbarButtonSpec g_tb_specs[] = {
	{ IDM_TOOLBAR_SYNC,     L"Sync",      45, 1, 0, NULL },
	{ IDM_TOOLBAR_REPLY,    L"Reply",     55, 1, 0, NULL },
	{ IDM_TOOLBAR_REPLYALL, L"Reply All", 75, 1, 0, NULL },
	{ IDM_TOOLBAR_FORWARD,  L"Forward",   65, 1, 0, NULL },
	{ IDM_TOOLBAR_DELETE,   L"Delete",    75, 1, 0, NULL }
};
static const int g_tb_spec_count = sizeof(g_tb_specs) / sizeof(g_tb_specs[0]);

/* For theme colors */
#define TO_COLORREF(c) RGB((c).r, (c).g, (c).b)
static COLORREF g_color_text;
static COLORREF g_color_text_bg;
static COLORREF g_color_error_bg;
static COLORREF g_color_error_hi;
static COLORREF g_color_error_text;
static COLORREF g_color_inactive_sel;
static COLORREF g_color_nomsg_bg;
static COLORREF g_color_zebra_odd;
static COLORREF g_color_zebra_even;

/* Convert a UTF-8 core string to a Win32 UTF-16 wide string */
static wchar_t *utf8_to_wide_dup(const char *utf8_str)
{
	int wlen;
	wchar_t *wbuf;

	if (!utf8_str) return NULL;

	wlen = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
	if (wlen <= 0) return NULL;

	wbuf = (wchar_t *)malloc(wlen * sizeof(wchar_t));
	if (!wbuf) return NULL;

	MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wbuf, wlen);
	return wbuf;
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
			cmp = _stricmp(item1->subject, item2->subject);
			break;
		case 1:
			cmp = _stricmp(item1->participant, item2->participant);
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


static void load_folder_messages(HWND hwndList, HWND hwndReading, MailContext *ctx, int32_t folder_idx, int select_row)
{
	uint32_t folder_flags;
	LVCOLUMNW lvcCol;
	StoreMsgHeader *headers = NULL;
	uint32_t msg_count = 0;
	uint32_t i;
	int row;
	LVITEMW lvi;
	int total_items;

	if (folder_idx == INVALID_INDEX) return;

	folder_flags = ctx->folders.flags[folder_idx];

	ListView_DeleteAllItems(hwndList);
	SetWindowTextW(hwndReading, rp_nomsg);
	g_reading_pane_state = 2;

	memset(&lvcCol, 0, sizeof(lvcCol));
	lvcCol.mask = LVCF_TEXT;
	lvcCol.pszText = ISFLAG(folder_flags, SM_FOLDER_SENT) ? (LPWSTR)(uintptr_t)L"To" : (LPWSTR)(uintptr_t)L"From";
	ListView_SetColumn(hwndList, 1, &lvcCol);

	if (store_fetch_folder_messages(folder_idx, &headers, &msg_count) == 0 && headers) {
		for (i = 0; i < msg_count; i++) {
			MessageListItem *mItem = (MessageListItem *)malloc(sizeof(MessageListItem));
			if (!mItem) continue;

			mItem->id = headers[i].id;
			strcpy_s(mItem->subject, sizeof(mItem->subject), headers[i].subject);
			strcpy_s(mItem->participant, sizeof(mItem->participant), headers[i].participant);
			strcpy_s(mItem->date_str, sizeof(mItem->date_str), headers[i].date_str);
			mItem->timestamp = headers[i].timestamp;

			wchar_t *w_subject = utf8_to_wide_dup(mItem->subject);
			wchar_t *w_participant = utf8_to_wide_dup(mItem->participant);
			wchar_t *w_date = utf8_to_wide_dup(mItem->date_str);

			memset(&lvi, 0, sizeof(lvi));
			lvi.mask = LVIF_TEXT | LVIF_PARAM;
			lvi.iItem = (int)i;
			lvi.iSubItem = 0;
			lvi.pszText = w_subject;
			lvi.lParam = (LPARAM)mItem;

			row = ListView_InsertItem(hwndList, &lvi);
			ListView_SetItemText(hwndList, row, 1, w_participant);
			ListView_SetItemText(hwndList, row, 2, w_date);

			free(w_subject);
			free(w_participant);
			free(w_date);
		}

		store_free_folder_messages(headers);
	}

	/* Default sort: Date column (index 2), Descending */
	{
		LPARAM defaultSortParam = (2 << 1) | 0;
		ListView_SortItems(hwndList, MessageListCompare, defaultSortParam);

		HWND hwndHeader = ListView_GetHeader(hwndList);
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

	total_items = ListView_GetItemCount(hwndList);
	if (total_items > 0) {
		if (select_row < 0) select_row = 0;
		if (select_row >= total_items) select_row = total_items - 1;
		ListView_SetItemState(hwndList, select_row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	}
}


static void delete_selected_message(HWND hwndTree, HWND hwndList, HWND hwndReading, MailContext *ctx)
{
	int sel_idx = ListView_GetNextItem(hwndList, -1, LVNI_SELECTED);
	HTREEITEM hSel;
	TVITEMW tvti;
	int32_t folder_idx;

	if (sel_idx == -1) return;

	LVITEMW lvi;
	memset(&lvi, 0, sizeof(lvi));
	lvi.iItem = sel_idx;
	lvi.mask = LVIF_PARAM;
	if (ListView_GetItem(hwndList, &lvi) && lvi.lParam) {
		MessageListItem *item = (MessageListItem *)lvi.lParam;
		uint64_t msg_id = item->id;

		if (store_delete_message(msg_id) == 0) {
			hSel = TreeView_GetSelection(hwndTree);
			if (hSel) {
				memset(&tvti, 0, sizeof(tvti));
				tvti.hItem = hSel;
				tvti.mask = TVIF_PARAM;
				if (TreeView_GetItem(hwndTree, &tvti)) {
					folder_idx = (int32_t)(intptr_t)tvti.lParam;
					int target_row = (sel_idx > 0) ? (sel_idx - 1) : 0;
					load_folder_messages(hwndList, hwndReading, ctx, folder_idx, target_row);
				}
			}
		}
	}
}


static HTREEITEM populate_folder_branch_w(HWND hwndTree, HTREEITEM hParent, 
                                          MailContext *ctx, int32_t folder_idx) 
{
	char *name_str;
	wchar_t *wname;
	TVINSERTSTRUCTW tvis;
	HTREEITEM hItem;
	int32_t child_idx;

	if (folder_idx == INVALID_INDEX) return NULL;

	name_str = pool_get_str(&ctx->pool, ctx->folders.name_offsets[folder_idx]);
	wname = utf8_to_wide_dup(name_str);

	memset(&tvis, 0, sizeof(tvis));
	tvis.hParent = hParent;
	tvis.hInsertAfter = TVI_LAST;
	tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
	tvis.item.pszText = (LPWSTR)wname;
	tvis.item.lParam = (LPARAM)(intptr_t)folder_idx;

	hItem = TreeView_InsertItem(hwndTree, &tvis);
	free(wname);

	if (g_ui_htree_items && folder_idx < (int32_t)g_ui_htree_capacity) {
		g_ui_htree_items[folder_idx] = hItem;
	}

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
	char *acc_name_str;
	wchar_t *w_acc_name;
	TVINSERTSTRUCTW tvis_acc;
	HTREEITEM hAccItem;
	int32_t curr_f_idx;

	TreeView_DeleteAllItems(hwndTree);

	if (ctx->folders.capacity > g_ui_htree_capacity) {
		HTREEITEM *new_items = (HTREEITEM *)realloc(g_ui_htree_items, ctx->folders.capacity * sizeof(HTREEITEM));
		if (new_items) {
			g_ui_htree_items = new_items;
			g_ui_htree_capacity = ctx->folders.capacity;
		}
	}

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
		acc_name_str = pool_get_str(&ctx->pool, ctx->accounts.name_offsets[i]);
		w_acc_name = utf8_to_wide_dup(acc_name_str);

		memset(&tvis_acc, 0, sizeof(tvis_acc));
		tvis_acc.hParent = TVI_ROOT;
		tvis_acc.hInsertAfter = TVI_LAST;
		tvis_acc.item.mask = TVIF_TEXT | TVIF_PARAM;
		tvis_acc.item.pszText = (LPWSTR)w_acc_name;
		tvis_acc.item.lParam = (LPARAM)(intptr_t)-1;

		hAccItem = TreeView_InsertItem(hwndTree, &tvis_acc);
		free(w_acc_name);

		curr_f_idx = ctx->accounts.root_folder_idx[i];
		while (curr_f_idx != INVALID_INDEX) {
			populate_folder_branch_w(hwndTree, hAccItem, ctx, curr_f_idx);
			curr_f_idx = ctx->folders.next_sibling_idx[curr_f_idx];
		}

		TreeView_Expand(hwndTree, hAccItem, TVE_EXPAND);
	}
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
	HFONT hUiFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

	switch (uMsg) {
		case WM_CREATE:
			pCreate = (CREATESTRUCT *)lParam;
			hInstance = pCreate->hInstance;

			hwndToolbar = CreateWindowEx(
				0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
				0, 0, 0, 0, hwnd, (HMENU)1, hInstance, NULL
			);

			{
				int x_offset = 6;
				int i;

				for (i = 0; i < g_tb_spec_count; i++) {
					const wchar_t *btn_text = g_tb_specs[i].show_text ? g_tb_specs[i].label : L"";
					DWORD btn_style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
					if (g_tb_specs[i].id == IDM_TOOLBAR_DELETE) btn_style |= BS_OWNERDRAW;
					else btn_style |= BS_PUSHBUTTON;

					g_tb_specs[i].hwnd = CreateWindowEx(
						0, L"BUTTON", btn_text, btn_style,
						x_offset, 3, g_tb_specs[i].default_width, 24,
						hwnd, (HMENU)(intptr_t)g_tb_specs[i].id, hInstance, NULL
					);
					/* The SendMessage(...WM_SETFONT...) calls set up the default font */
					SendMessage(g_tb_specs[i].hwnd, WM_SETFONT, (WPARAM)hUiFont, TRUE);
					x_offset += g_tb_specs[i].default_width + 4;
				}
			}

			hwndFolderTree = CreateWindowEx(
				0, WC_TREEVIEW, L"Folders",
				WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP,
				0, 0, 0, 0, hwnd, (HMENU)2, hInstance, NULL
			);
			SendMessage(hwndFolderTree, WM_SETFONT, (WPARAM)hUiFont, TRUE);

			hwndMessageList = CreateWindowEx(
				0, WC_LISTVIEW, L"Messages",
				WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP,
				0, 0, 0, 0, hwnd, (HMENU)3, hInstance, NULL
			);
			SendMessage(hwndMessageList, WM_SETFONT, (WPARAM)hUiFont, TRUE);
			ListView_SetExtendedListViewStyle(hwndMessageList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
 
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
			SendMessage(hwndReadingPane, WM_SETFONT, (WPARAM)hUiFont, TRUE);

			load_app_configuration(&g_mail_ctx);

			/* Assign theme colors to UI */
			g_color_text         = TO_COLORREF(g_mail_ctx.theme.text);
			g_color_text_bg      = TO_COLORREF(g_mail_ctx.theme.text_bg);
			g_color_error_bg     = TO_COLORREF(g_mail_ctx.theme.error_bg);
			g_color_error_hi     = TO_COLORREF(g_mail_ctx.theme.error_hi);
			g_color_error_text   = TO_COLORREF(g_mail_ctx.theme.error_text);
			g_color_inactive_sel = TO_COLORREF(g_mail_ctx.theme.inactive_sel);
			g_color_nomsg_bg     = TO_COLORREF(g_mail_ctx.theme.nomsg_bg);
			g_color_zebra_odd    = TO_COLORREF(g_mail_ctx.theme.zebra_odd);
			g_color_zebra_even   = TO_COLORREF(g_mail_ctx.theme.zebra_even);

			hbrTextBg  = CreateSolidBrush(g_color_text_bg);
			hbrErrorBg = CreateSolidBrush(g_color_error_bg);
			hbrNoMsgBg = CreateSolidBrush(g_color_nomsg_bg);

			store_init(&g_mail_ctx);
			tree_view_refresh(hwndFolderTree, &g_mail_ctx);

			if (g_mail_ctx.accounts.count > 0 && g_ui_htree_items) {
				int32_t root_f_idx = g_mail_ctx.accounts.root_folder_idx[0];
				if (root_f_idx != INVALID_INDEX) {
					TreeView_SelectItem(hwndFolderTree, g_ui_htree_items[root_f_idx]);
				}
			}

			return 0;
		/* End of WM_CREATE */

		case WM_CTLCOLORSTATIC:
			if ((HWND)lParam == hwndReadingPane) {
				switch (g_reading_pane_state) {
					default:
					case 0:
						SetBkColor((HDC)wParam, g_color_text_bg);
						return (INT_PTR)hbrTextBg;
					case 1:
						SetBkColor((HDC)wParam, g_color_error_bg);
						return (INT_PTR)hbrErrorBg;
					case 2:
						SetBkColor((HDC)wParam, g_color_nomsg_bg);
						return (INT_PTR)hbrNoMsgBg;
				}
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
					{
						wchar_t aboutText[512];
						swprintf(aboutText, sizeof(aboutText) / sizeof(wchar_t),
							L"Spite Mail: made with hate\nVersion %S (%S)\n(C) 2026 Jody Bruchon <jody@jodybruchon.com>\nPublished under GNU GPL v2",
							VER, VERDATE
						);
						MessageBox(hwnd, aboutText, L"About Spite Mail", MB_OK | MB_ICONINFORMATION);
					}
					break;

				case IDM_TOOLBAR_SYNC:
					break;

				case IDM_TOOLBAR_REPLY:
				case IDM_TOOLBAR_REPLYALL:
				case IDM_TOOLBAR_FORWARD:
					break;

				case IDM_TOOLBAR_DELETE:
					delete_selected_message(hwndFolderTree, hwndMessageList, hwndReadingPane, &g_mail_ctx);
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

			tbHeight = 30;
			contentTop = tbHeight;
			contentHeight = cy - contentTop;

			col1Width = SM_VV_FOLDER_PANE_WIDTH;
			col2Width = SM_VV_MESSAGE_PANE_WIDTH;
			if (cx < SM_WINDOW_WIDTH) {
				col1Width = (int)(((unsigned long)cx * SM_VV_FOLDER_PANE_WIDTH) / SM_WINDOW_WIDTH);
				col2Width = (int)(((unsigned long)cx * SM_VV_MESSAGE_PANE_WIDTH) / SM_WINDOW_WIDTH);
				if (col1Width < SM_VV_MIN_FOLDER_PANE_WIDTH) col1Width = SM_VV_MIN_FOLDER_PANE_WIDTH;
				if (col2Width < SM_VV_MIN_MESSAGE_PANE_WIDTH) col2Width = SM_VV_MIN_MESSAGE_PANE_WIDTH;
			}

			col3Width = cx - col1Width - col2Width;
			if (col3Width < SM_VV_MIN_READING_PANE_WIDTH) col3Width = SM_VV_MIN_READING_PANE_WIDTH;

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
				uint32_t i;

				pNmhdr = (NMHDR *)lParam;

				if (pNmhdr->hwndFrom == hwndMessageList && pNmhdr->code == LVN_DELETEITEM) {
					pnmv = (NMLISTVIEW *)lParam;
					if (pnmv->lParam) free((void *)pnmv->lParam);
					return 0;
				}

				if (pNmhdr->hwndFrom == hwndMessageList && pNmhdr->code == LVN_ITEMCHANGED) {
					pnmv = (NMLISTVIEW *)lParam;

					if ((pnmv->uChanged & LVIF_STATE) && (pnmv->uNewState & LVIS_SELECTED) && !(pnmv->uOldState & LVIS_SELECTED)) {
						MessageListItem *item = (MessageListItem *)pnmv->lParam;
						if (item) {
							char *eml_data = NULL;
							size_t eml_size = 0;
							if (store_fetch_message(item->id, &eml_data, &eml_size) == 0 && eml_data) {
								wchar_t *w_eml = utf8_to_wide_dup(eml_data);
								g_reading_pane_state = 0;
								SetWindowTextW(hwndReadingPane, w_eml ? w_eml : L"");
								free(w_eml);
								store_free_message(eml_data);
							} else {
								g_reading_pane_state = 1;
								SetWindowTextW(hwndReadingPane, L"\r\n  <error: can't load message content>\r\n");
							}
						}
					} else if ((pnmv->uChanged & LVIF_STATE) && !(pnmv->uNewState & LVIS_SELECTED) && (pnmv->uOldState & LVIS_SELECTED)) {
						g_reading_pane_state = 2;
						SetWindowTextW(hwndReadingPane, rp_nomsg);
						InvalidateRect(hwndReadingPane, NULL, TRUE);
					}
					return 0;
				}

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

				if (pNmhdr->hwndFrom == hwndFolderTree && pNmhdr->code == TVN_SELCHANGEDW) {
					pnmtv = (NMTREEVIEWW *)lParam;
					folder_idx = (int32_t)(intptr_t)pnmtv->itemNew.lParam;

					load_folder_messages(hwndMessageList, hwndReadingPane, &g_mail_ctx, folder_idx, 0);
					return 0;
				}

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

							if (is_selected && g_reading_pane_state) {
								pLVCD->nmcd.uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS);
								pLVCD->clrTextBk = g_color_error_hi;
								pLVCD->clrText = g_color_error_text;
								return CDRF_NEWFONT;
							}

							if (is_selected && is_inactive) {
								pLVCD->nmcd.uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS);
								pLVCD->clrTextBk = g_color_inactive_sel;
								pLVCD->clrText = g_color_text;
								return CDRF_NEWFONT;
							} else if (!is_selected) {
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

				if (pNmhdr->hwndFrom == hwndMessageList && pNmhdr->code == LVN_KEYDOWN) {
					NMLVKEYDOWN *pnkd = (NMLVKEYDOWN *)lParam;
					if (pnkd->wVKey == VK_DELETE) {
						delete_selected_message(hwndFolderTree, hwndMessageList, hwndReadingPane, &g_mail_ctx);
						return 0;
					}
				}
			}
			return 0;
		/* End of WM_NOTIFY */

		case WM_DRAWITEM:
		{
			DRAWITEMSTRUCT *pDI = (DRAWITEMSTRUCT *)lParam;
			if (pDI->CtlID == IDM_TOOLBAR_DELETE) {
				HDC hdc = pDI->hDC;
				int state = pDI->itemState;

				/* Draw standard button background/frame based on state */
				rc = pDI->rcItem;
				DrawFrameControl(hdc, &rc, DFC_BUTTON, (state & ODS_SELECTED) ? DFCS_BUTTONPUSH | DFCS_PUSHED : DFCS_BUTTONPUSH);

				/* Set up background transparency for text */
				SetBkMode(hdc, TRANSPARENT);

				/* Draw the red 'X' and text */
				{
					HFONT hFont = (HFONT)SendMessage(pDI->hwndItem, WM_GETFONT, 0, 0);
					HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
					COLORREF oldColor = SetTextColor(hdc, RGB(200, 0, 0)); /* Classic Outlook Red */

					TextOutW(hdc, rc.left + 8, rc.top + 4, L"X", 1);

					SetTextColor(hdc, RGB(0, 0, 0));
					TextOutW(hdc, rc.left + 22, rc.top + 4, L"Delete", 6);

					SelectObject(hdc, hOldFont);
					SetTextColor(hdc, oldColor);
				}

				/* Draw focus rect if focused */
				if (state & ODS_FOCUS) {
					RECT rcFocus = rc;
					InflateRect(&rcFocus, -3, -3);
					DrawFocusRect(hdc, &rcFocus);
				}

				return TRUE;
			}
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
		/* End of WM_DRAWITEM */

		case WM_GETMINMAXINFO:
			minMax = (MINMAXINFO *)lParam;
			minMax->ptMinTrackSize.x = SM_MIN_WINDOW_WIDTH;
			minMax->ptMinTrackSize.y = SM_MIN_WINDOW_HEIGHT;
			return 0;
		/* End of WM_GETMINMAXINFO */

		case WM_DESTROY:
			if (hbrTextBg) DeleteObject(hbrTextBg);
			if (hbrErrorBg) DeleteObject(hbrErrorBg);
			if (hbrNoMsgBg) DeleteObject(hbrNoMsgBg);
			if (g_ui_htree_items) {
				free(g_ui_htree_items);
				g_ui_htree_items = NULL;
				g_ui_htree_capacity = 0;
			}
			mail_context_free(&g_mail_ctx);
			store_cleanup();
			PostQuitMessage(0);
			return 0;
		/* End of WM_DESTROY */

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

	hMenu = LoadMenu(hInstance, MAKEINTRESOURCE(3));
	SetMenu(hwnd, hMenu);

	HICON hIconLarge = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(2), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
	HICON hIconSmall = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(2), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconLarge);
	SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

	ShowWindow(hwnd, nShowCmd);
	UpdateWindow(hwnd);

	memset(&msg, 0, sizeof(msg));
	while (GetMessage(&msg, NULL, 0, 0)) {
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
