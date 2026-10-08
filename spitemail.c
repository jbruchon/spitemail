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


/* UTF-16 String Pool Ingestion (C89 Declarations) */
uint32_t pool_add_utf8(StringPool *pool, const char *utf8_str)
{
	int req_chars;
	uint32_t offset;
	uint32_t new_cap;
	wchar_t *new_data;

	if (!utf8_str || *utf8_str == '\0') {
		return 0;
	}

	req_chars = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
	if (req_chars <= 0) {
		return 0;
	}

	if (pool->used + (uint32_t)req_chars > pool->capacity) {
		new_cap = pool->capacity ? pool->capacity * 2 : 4096;
		while (pool->used + (uint32_t)req_chars > new_cap) {
			new_cap *= 2;
		}

		new_data = (wchar_t *)realloc(pool->data, new_cap * sizeof(wchar_t));
		if (!new_data) {
			return 0;
		}

		pool->data = new_data;
		pool->capacity = new_cap;
	}

	offset = pool->used;
	MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, pool->data + offset, req_chars);
	pool->used += (uint32_t)req_chars;

	return offset;
}


wchar_t *pool_get_wstr(const StringPool *pool, uint32_t offset)
{
	return pool->data + offset;
}



void mail_context_init(MailContext *ctx)
{
	uint32_t acc_name_off;
	uint32_t inbox_name_off, sent_name_off;

	memset(ctx, 0, sizeof(MailContext));

	/* 1. Allocate initial capacities for SoA tables */
	ctx->accounts.capacity = 2;
	ctx->accounts.name_offsets = (uint32_t *)malloc(2 * sizeof(uint32_t));
	ctx->accounts.root_folder_idx = (int32_t *)malloc(2 * sizeof(int32_t));
	ctx->accounts.store_flags = (uint32_t *)malloc(2 * sizeof(uint32_t));

	ctx->folders.capacity = 4;
	ctx->folders.name_offsets = (uint32_t *)malloc(4 * sizeof(uint32_t));
	ctx->folders.path_offsets = (uint32_t *)malloc(4 * sizeof(uint32_t));
	ctx->folders.unread_counts = (uint32_t *)malloc(4 * sizeof(uint32_t));
	ctx->folders.flags = (uint32_t *)malloc(4 * sizeof(uint32_t));
	ctx->folders.parent_idx = (int32_t *)malloc(4 * sizeof(int32_t));
	ctx->folders.first_child_idx = (int32_t *)malloc(4 * sizeof(int32_t));
	ctx->folders.next_sibling_idx = (int32_t *)malloc(4 * sizeof(int32_t));
	ctx->folders.htree_items = (HTREEITEM *)malloc(4 * sizeof(HTREEITEM));

	/* 2. Add sample strings to UTF-16 pool */
	acc_name_off = pool_add_utf8(&ctx->pool, "root@spitemail.com");
	inbox_name_off = pool_add_utf8(&ctx->pool, "Inbox");
	sent_name_off = pool_add_utf8(&ctx->pool, "Sent");

	/* 3. Populate account 0 */
	ctx->accounts.name_offsets[0] = acc_name_off;
	ctx->accounts.root_folder_idx[0] = 0; /* Index of Inbox folder */
	ctx->accounts.store_flags[0] = 1;     /* RAM store flag */
	ctx->accounts.count = 1;

	/* 4. Populate folder 0: Inbox */
	ctx->folders.name_offsets[0] = inbox_name_off;
	ctx->folders.path_offsets[0] = inbox_name_off;
	ctx->folders.unread_counts[0] = 3;
	ctx->folders.flags[0] = SM_FOLDER_INBOX;
	ctx->folders.parent_idx[0] = INVALID_INDEX;
	ctx->folders.first_child_idx[0] = INVALID_INDEX;
	ctx->folders.next_sibling_idx[0] = 1; /* Points to Sent */
	ctx->folders.count++;

	/* 5. Populate folder 1: Sent */
	ctx->folders.name_offsets[1] = sent_name_off;
	ctx->folders.path_offsets[1] = sent_name_off;
	ctx->folders.unread_counts[1] = 0;
	ctx->folders.flags[1] = SM_FOLDER_SENT;
	ctx->folders.parent_idx[1] = INVALID_INDEX;
	ctx->folders.first_child_idx[1] = INVALID_INDEX;
	ctx->folders.next_sibling_idx[1] = INVALID_INDEX;
	ctx->folders.count++;
}


void mail_context_free(MailContext *ctx)
{
	if (!ctx) {
		return;
	}
	free(ctx->pool.data);
	free(ctx->accounts.name_offsets);
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

	if (folder_idx == INVALID_INDEX) {
		return NULL;
	}

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
			cmp = _wcsicmp(item1->sender, item2->sender);
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

	switch (uMsg) {
		case WM_GETMINMAXINFO:
			minMax = (MINMAXINFO *)lParam;
			minMax->ptMinTrackSize.x = SM_MIN_WINDOW_WIDTH;
			minMax->ptMinTrackSize.y = SM_MIN_WINDOW_HEIGHT;
			return 0;

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

			/* Initialize data backing store */
			store_init(&g_mail_ctx);
			tree_view_refresh(hwndFolderTree, &g_mail_ctx);

			/* Select the Inbox (index 0) by default on launch */
			TreeView_SelectItem(hwndFolderTree, g_mail_ctx.folders.htree_items[0]);

			return 0;

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

		case WM_SIZE:
			GetClientRect(hwnd, &rc);
			cx = rc.right - rc.left;
			cy = rc.bottom - rc.top;

			/* Position the top toolbar, 30px */
			tbHeight = 30;
			MoveWindow(hwndToolbar, 0, 0, cx, tbHeight, TRUE);
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
			MoveWindow(hwndFolderTree, 0, contentTop, col1Width, contentHeight, TRUE);
			MoveWindow(hwndMessageList, col1Width, contentTop, col2Width, contentHeight, TRUE);
			MoveWindow(hwndReadingPane, col1Width + col2Width, contentTop, col3Width, contentHeight, TRUE);
			EndDeferWindowPos(hdwp);
			return 0;

		case WM_NOTIFY:
			{
				NMHDR *pNmhdr;
				NMTREEVIEWW *pnmtv;
				NMLISTVIEW *pnmv;
				int32_t folder_idx;
				uint32_t folder_flags;
				LVITEMW lvi;
				LVCOLUMNW lvcCol;
				StoreMsgHeader *headers = NULL;
				uint32_t msg_count = 0;
				uint32_t i;
				int row;

				pNmhdr = (NMHDR *)lParam;

				/* Free MessageListItem allocations when items are deleted */
				if (pNmhdr->hwndFrom == hwndMessageList && pNmhdr->code == LVN_DELETEITEM) {
					pnmv = (NMLISTVIEW *)lParam;
					if (pnmv->lParam) {
						free((void *)pnmv->lParam);
					}
					return 0;
				}

				/* Handle message selection change to update reading pane */
				if (pNmhdr->hwndFrom == hwndMessageList && pNmhdr->code == LVN_ITEMCHANGED) {
					pnmv = (NMLISTVIEW *)lParam;

					if ((pnmv->uChanged & LVIF_STATE) && (pnmv->uNewState & LVIS_SELECTED) && !(pnmv->uOldState & LVIS_SELECTED)) {
						MessageListItem *item = (MessageListItem *)pnmv->lParam;
						if (item) {
							wchar_t *body_text = NULL;
							if (store_fetch_message_body(item->id, &body_text) == 0 && body_text) {
								SetWindowTextW(hwndReadingPane, body_text);
								store_free_message_body(body_text);
							}
						}
					} else if ((pnmv->uChanged & LVIF_STATE) && !(pnmv->uNewState & LVIS_SELECTED) && (pnmv->uOldState & LVIS_SELECTED)) {
						/* Clear pane if item is deselected */
						SetWindowTextW(hwndReadingPane, L"");
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

					if (folder_idx == INVALID_INDEX) {
						return 0;
					}

					folder_flags = g_mail_ctx.folders.flags[folder_idx];

					/* Clear list view items and reading pane text */
					ListView_DeleteAllItems(hwndMessageList);
					SetWindowTextW(hwndReadingPane, L"");

					/* Dynamic Column Labeling */
					memset(&lvcCol, 0, sizeof(lvcCol));
					lvcCol.mask = LVCF_TEXT;
					lvcCol.pszText = ISFLAG(folder_flags, SM_FOLDER_SENT) ? (LPWSTR)(uintptr_t)L"To" : (LPWSTR)(uintptr_t)L"From";
					ListView_SetColumn(hwndMessageList, 1, &lvcCol);

					/* Query Data Store for Messages in Folder */
					if (store_fetch_folder_messages(folder_idx, &headers, &msg_count) == 0 && headers) {
						for (i = 0; i < msg_count; i++) {
							MessageListItem *mItem = (MessageListItem *)malloc(sizeof(MessageListItem));
							if (!mItem) continue;

							mItem->id = headers[i].id;
							wcscpy_s(mItem->subject, 128, headers[i].subject);
							wcscpy_s(mItem->sender, 128, headers[i].sender);
							wcscpy_s(mItem->date_str, 32, headers[i].date_str);
							mItem->timestamp = headers[i].timestamp;

							memset(&lvi, 0, sizeof(lvi));
							lvi.mask = LVIF_TEXT | LVIF_PARAM;
							lvi.iItem = (int)i;
							lvi.iSubItem = 0;
							lvi.pszText = mItem->subject;
							lvi.lParam = (LPARAM)mItem;

							row = ListView_InsertItem(hwndMessageList, &lvi);
							ListView_SetItemText(hwndMessageList, row, 1, mItem->sender);
							ListView_SetItemText(hwndMessageList, row, 2, mItem->date_str);
						}

						/* Release Store Query Memory */
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
			}
			return 0;

		case WM_DESTROY:
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

	if (!RegisterClassEx(&wc)) {
		return 0;
	}

	HWND hwnd = CreateWindowEx(
		0, CLASS_NAME, L"Spite Mail", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, SM_WINDOW_WIDTH, SM_WINDOW_HEIGHT,
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
