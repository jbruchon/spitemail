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
 #include <uxtheme.h>
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
	char *data;
	uint32_t used;
	uint32_t capacity;
} StringPool;

/* Folder role flags */
#define SM_FOLDER_INBOX   (1 << 0)
#define SM_FOLDER_SENT    (1 << 1)
#define SM_FOLDER_DRAFTS  (1 << 2)
#define SM_FOLDER_TRASH   (1 << 3)
#define SM_FOLDER_CUSTOM  (1 << 4)

typedef struct FolderTable {
	uint32_t  *name_offsets;
	uint32_t  *path_offsets;
	uint32_t  *unread_counts;
	uint32_t  *flags;
	int32_t   *parent_idx;
	int32_t   *first_child_idx;
	int32_t   *next_sibling_idx;
	uint32_t   count;
	uint32_t   capacity;
} FolderTable;

typedef struct AccountTable {
	uint32_t *name_offsets;
	uint32_t *server_offsets;
	uint32_t *username_offsets;
	uint32_t *ports;
	int32_t  *root_folder_idx;
	uint32_t *store_flags;
	uint32_t  count;
	uint32_t  capacity;
} AccountTable;

typedef struct ColorRGB {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} ColorRGB;

typedef struct ThemeConfig {
	ColorRGB text;
	ColorRGB text_bg;
	ColorRGB error_bg;
	ColorRGB error_hi;
	ColorRGB error_text;
	ColorRGB inactive_sel;
	ColorRGB nomsg_bg;
	ColorRGB zebra_odd;
	ColorRGB zebra_even;
} ThemeConfig;

typedef struct MailContext {
	StringPool   pool;
	FolderTable  folders;
	AccountTable accounts;
	ThemeConfig  theme;
} MailContext;

typedef struct {
    char     subject[128];
    char     sender[128];
    uint32_t timestamp;
} MailItem;

extern MailContext g_mail_ctx;


/*** Function prototypes (Defined AFTER MailContext) ***/
uint32_t pool_add_str(StringPool *pool, const char *str);
char *pool_get_str(const StringPool *pool, uint32_t offset);
void load_app_configuration(MailContext *ctx);
void mail_context_init(MailContext *ctx);
void mail_context_free(MailContext *ctx);
void tree_view_refresh(HWND hwndTree, MailContext *ctx);


#ifdef __cplusplus
}
#endif

#endif /* SPITEMAIL_H */
