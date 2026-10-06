/* Spite Mail; license: GPLv2; (C) 2026 Jody Bruchon <jody@jodybruchon.com> */

#include "version.h"
#include "spitemail.h"


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
	ctx->folders.parent_idx = (int32_t *)malloc(4 * sizeof(int32_t));
	ctx->folders.first_child_idx = (int32_t *)malloc(4 * sizeof(int32_t));
	ctx->folders.next_sibling_idx = (int32_t *)malloc(4 * sizeof(int32_t));
	ctx->folders.htree_items = (HTREEITEM *)malloc(4 * sizeof(HTREEITEM));

	/* 2. Add sample strings to UTF-16 pool */
	acc_name_off = pool_add_utf8(&ctx->pool, "Spite Mail (RAM Store)");
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
	ctx->folders.parent_idx[0] = INVALID_INDEX;
	ctx->folders.first_child_idx[0] = INVALID_INDEX;
	ctx->folders.next_sibling_idx[0] = 1; /* Points to Sent */
	ctx->folders.count++;

	/* 5. Populate folder 1: Sent */
	ctx->folders.name_offsets[1] = sent_name_off;
	ctx->folders.path_offsets[1] = sent_name_off;
	ctx->folders.unread_counts[1] = 0;
	ctx->folders.parent_idx[1] = INVALID_INDEX;
	ctx->folders.first_child_idx[1] = INVALID_INDEX;
	ctx->folders.next_sibling_idx[1] = INVALID_INDEX;
	ctx->folders.count++;
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
	int col1Width;
	int col2Width;
	int col3Width;
	HDWP hdwp;
	MINMAXINFO *minMax;

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
				WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | WS_BORDER,
				0, 0, 0, 0, hwnd, (HMENU)2, hInstance, NULL
			);
			hwndMessageList = CreateWindowEx(
				0, WC_LISTVIEW, L"Messages",
				WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_BORDER,
				0, 0, 0, 0, hwnd, (HMENU)3, hInstance, NULL
			);
			hwndReadingPane = CreateWindowEx(
				0, L"EDIT", L"Reading Pane",
				WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_BORDER,
				0, 0, 0, 0, hwnd, (HMENU)4, hInstance, NULL
			);

			/* Init fake data for testing */
			mail_context_init(&g_mail_ctx);
			tree_view_refresh(hwndFolderTree, &g_mail_ctx);

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
				col1Width = ((long)cx * SM_FOLDER_PANE_WIDTH) / SM_WINDOW_WIDTH;
				col2Width = ((long)cx * SM_MESSAGE_PANE_WIDTH) / SM_WINDOW_WIDTH;
				if (col1Width < SM_MIN_FOLDER_PANE_WIDTH) col1Width = SM_MIN_FOLDER_PANE_WIDTH;
				if (col2Width < SM_MIN_MESSAGE_PANE_WIDTH) col2Width = SM_MIN_MESSAGE_PANE_WIDTH;
			}

			col3Width = cx - col1Width - col2Width;
			if (col3Width < SM_MIN_READING_PANE_WIDTH) col3Width = SM_MIN_READING_PANE_WIDTH;

			hdwp = BeginDeferWindowPos(3);
			MoveWindow(hwndFolderTree, 0, contentTop, col1Width, contentHeight, TRUE);
			MoveWindow(hwndMessageList, col1Width, contentTop, col2Width, contentHeight, TRUE);
			MoveWindow(hwndReadingPane, col1Width + col2Width, contentTop, col3Width, contentHeight, TRUE);
			EndDeferWindowPos(hdwp);
			return 0;

		case WM_DESTROY:
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

	MSG msg = { 0 };
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}
