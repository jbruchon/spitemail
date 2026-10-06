/* spitemail main program header
 * See spitemail.c for license information */

#ifndef SPITEMAIL_H
#define SPITEMAIL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Spite Mail is a Win32 application only */
#if defined _WIN32 || defined __MINGW32__ || defined __MINGW64__
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <io.h>
 #include <wchar.h>
 #include <commctrl.h>
#else
#error This is a Win32 application.
#endif /* Win32 */

#include <stdint.h>


/*** Helpful general-purpose definitions ***/

#define ISFLAG(a,b) ((a & b) == b)
#define SETFLAG(a,b) (a |= b)
#define CLEARFLAG(a,b) (a &= (~b))

#define INVALID_INDEX (-1)

/* Chunk sizing */
#ifndef CHUNK_SIZE
 #define CHUNK_SIZE 65536
#endif

/* Aggressive verbosity for deep debugging */
#ifdef LOUD_DEBUG
 #ifndef DEBUG
  #define DEBUG
 #endif
 #define LOUD(...) if ISFLAG(flags, F_LOUD) __VA_ARGS__
#else
 #define LOUD(a)
#endif

/* Compile out debugging stat counters unless requested */
#ifdef DEBUG
 #define DBG(a) a
#else
 #define DBG(a)
#endif


/*** Data context definitions - we favor SoA (struct-of-arrays) layouts ***/

/* Single large arena for account/folder/path string storage */
typedef struct StringPool {
	wchar_t *data;
	uint32_t used;
	uint32_t capacity;
} StringPool;

typedef struct FolderTable {
	uint32_t  *name_offsets;
	uint32_t  *path_offsets;
	uint32_t  *unread_counts;
	int32_t   *parent_idx;
	int32_t   *first_child_idx;
	int32_t   *next_sibling_idx;
	HTREEITEM *htree_items;
	uint32_t   count;
	uint32_t   capacity;
} FolderTable;

typedef struct AccountTable {
	uint32_t *name_offsets;
	int32_t  *root_folder_idx;
	uint32_t *store_flags;
	uint32_t  count;
	uint32_t  capacity;
} AccountTable;

typedef struct MailContext {
	StringPool   pool;
	FolderTable  folders;
	AccountTable accounts;
} MailContext;


/*** Resource definitions for icons, menus, etc. ***/

#define WINRES_MANIFEST 1
#define ICON_SPITEMAIL 2
#define IDM_MENU 3

#define IDM_FILE_EXIT  9001
#define IDM_HELP_ABOUT 9002


/*** Constants ***/
#define SM_WINDOW_WIDTH 800
#define SM_WINDOW_HEIGHT 600
#define SM_MIN_WINDOW_WIDTH 600
#define SM_MIN_WINDOW_HEIGHT 300
/* The next 3 numbers should add up to SM_WINDOW_WIDTH */
#define SM_FOLDER_PANE_WIDTH 200
#define SM_MESSAGE_PANE_WIDTH 240
#define SM_READING_PANE_WIDTH 360
/* The next 3 numbers should add up to SM_MIN_WINDOW_WIDTH */
#define SM_MIN_FOLDER_PANE_WIDTH 120
#define SM_MIN_MESSAGE_PANE_WIDTH 140
#define SM_MIN_READING_PANE_WIDTH 340


/*** Function prototypes ***/
uint32_t pool_add_utf8(StringPool *pool, const char *utf8_str);
wchar_t *pool_get_wstr(const StringPool *pool, uint32_t offset);
void mail_context_init(MailContext *ctx);
void mail_context_free(MailContext *ctx);
void tree_view_refresh(HWND hwndTree, MailContext *ctx);


#ifdef __cplusplus
}
#endif

#endif /* SPITEMAIL_H */
