/* It doesn't exist yet */

#include "spitemail.h"


LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
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
		0,
		CLASS_NAME,
		L"Spite Mail",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
		NULL, NULL, hInstance, NULL
	);

	if (hwnd == NULL) {
		return 0;
	}

	/* Force explicit icon assignment */
	HICON hIconLarge = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(2), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
	HICON hIconSmall = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(2), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconLarge);
	SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

	ShowWindow(hwnd, nShowCmd);
	UpdateWindow(hwnd);

	MSG msg = {0};
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}
