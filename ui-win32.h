/* Spite Mail Win32 API UI resources
 * license: GPLv2; (C) 2026 Jody Bruchon <jody@jodybruchon.com> */

#ifndef UI_WIN32_H
#define UI_WIN32_H

#ifdef __cplusplus
extern "C" {
#endif

/*** Resource definitions for icons, menus, etc. ***/
#define WINRES_MANIFEST 1
#define ICON_SPITEMAIL 2
#define IDM_MENU 3

/* Menu bar */
#define IDM_FILE_EXIT      9001
#define IDM_HELP_ABOUT     9002
/* Toolbar */
#define IDM_TOOLBAR_SYNC     9023
#define IDM_TOOLBAR_REPLY    9024
#define IDM_TOOLBAR_REPLYALL 9025
#define IDM_TOOLBAR_FORWARD  9026
#define IDM_TOOLBAR_DELETE   9027

/*** Constants ***/
#define SM_WINDOW_WIDTH 800
#define SM_WINDOW_HEIGHT 480
#define SM_MIN_WINDOW_WIDTH 640
#define SM_MIN_WINDOW_HEIGHT 300
#define SM_VV_FOLDER_PANE_WIDTH 120
#define SM_VV_MESSAGE_PANE_WIDTH 380
#define SM_VV_READING_PANE_WIDTH 300
#define SM_VV_MIN_FOLDER_PANE_WIDTH 120
#define SM_VV_MIN_MESSAGE_PANE_WIDTH 300
#define SM_VV_MIN_READING_PANE_WIDTH 240
#define SM_MP_SUBJECT_WIDTH 170
#define SM_MP_FROMTO_WIDTH 100
#define SM_MP_DATE_WIDTH 100

#ifdef __cplusplus
}
#endif

#endif /* UI_WIN32_H */
