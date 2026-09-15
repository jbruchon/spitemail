/* Spite Mail; license: GPLv2; (C) 2026 Jody Bruchon <jody@jodybruchon.com> */

#include "version.h"
#include "spitemail.h"
#include <commctrl.h>

static HWND hwndToolbar = NULL;
static HWND hwndFolderTree = NULL;
static HWND hwndMessageList = NULL;
static HWND hwndReadingPane = NULL;

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
