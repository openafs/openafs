
#define NOSTRSAFE
#include "afscred.h"
#include "AFS_component_version_number.h"
#include <tchar.h>
#include <shellapi.h>
#include <strsafe.h>
#include <assert.h>

static ATOM message_window_class = 0;
static HWND notifier_window = NULL;
static volatile BOOL notification_icon_added = FALSE;

#define TOKEN_ICON_ID 1
#define TOKEN_MESSAGE_ID WM_USER

static LRESULT CALLBACK
notifier_wnd_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == TOKEN_MESSAGE_ID) {
        switch (lParam) {
        case NIN_SELECT:
        case NIN_KEYSELECT:

            {
                NOTIFYICONDATA idata;
                khm_int32 cmd = KHUI_ACTION_OPEN_APP;

                khc_read_int32(NULL, L"CredWindow\\NotificationAction", &cmd);

                khui_action_trigger(cmd, NULL);

                ZeroMemory(&idata, sizeof(idata));

                Shell_NotifyIcon(NIM_SETFOCUS, &idata);
            }
            return 0;

        default:
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static void
initialize_if_necessary(void)
{
    if (message_window_class == 0) {
        WNDCLASSEX c = {
            sizeof(WNDCLASSEX), /* cbSize */
            0,                  /* style */
            notifier_wnd_proc,  /* lpfnWndProc */
            0,                  /* cbClsExtra */
            0,                  /* cbWndExtra */
            hInstance,          /* hinstance */
            NULL,               /* hIcon */
            NULL,               /* hCursor */
            NULL,               /* hbrBackground */
            NULL,               /* lpszMenuName */
            L"OpenAFSTokenStateIconNotifier", /* lpszClassName */
            NULL,                             /* hIconSm */
        };

        message_window_class = RegisterClassEx(&c);
    }

    if (notifier_window == NULL && message_window_class != 0) {
        notifier_window = CreateWindow(MAKEINTATOM(message_window_class),
                                       L"OpenAFSTokenStateIconNotifierWindow",
                                       0, 0, 0, 0, 0,
                                       HWND_MESSAGE,
                                       NULL,
                                       hInstance,
                                       NULL);
    }

    assert(notifier_window != NULL);

    if (!notification_icon_added && notifier_window != NULL) {
        NOTIFYICONDATA idata;

        ZeroMemory(&idata, sizeof(idata));

        idata.cbSize = sizeof(idata);
        idata.hWnd = notifier_window;
        idata.uID = TOKEN_ICON_ID;
        idata.uFlags = NIF_ICON | NIF_MESSAGE;
        idata.uCallbackMessage = TOKEN_MESSAGE_ID;
        idata.hIcon = (HICON) LoadImage(hResModule, MAKEINTRESOURCE(IDI_CRED_NONE),
                                        IMAGE_ICON, 0, 0,
                                        LR_DEFAULTSIZE | LR_DEFAULTCOLOR | LR_SHARED);
        notification_icon_added = Shell_NotifyIcon(NIM_ADD, &idata);

        idata.cbSize = sizeof(idata);
        idata.uVersion = NOTIFYICON_VERSION;

        Shell_NotifyIcon(NIM_SETVERSION, &idata);

        assert(notification_icon_added);
    }
}

void
afs_remove_icon(void)
{
    NOTIFYICONDATA idata;
    wchar_t buf[ARRAYLENGTH(idata.szTip)];

    ZeroMemory(&idata, sizeof(idata));

    idata.cbSize = sizeof(idata);
    idata.hWnd = notifier_window;
    idata.uID = TOKEN_ICON_ID;
    Shell_NotifyIcon(NIM_DELETE, &idata);
    notification_icon_added = FALSE;
}

static void
set_tooltip_and_icon(UINT tooltip_text, const wchar_t * postfix, UINT icon_id)
{
    NOTIFYICONDATA idata;
    wchar_t buf[ARRAYLENGTH(idata.szTip)];

    ZeroMemory(&idata, sizeof(idata));

    idata.cbSize = sizeof(idata);
    idata.hWnd = notifier_window;
    idata.uID = TOKEN_ICON_ID;
    idata.uFlags = NIF_ICON | NIF_TIP;
    idata.hIcon = (HICON) LoadImage(hResModule, MAKEINTRESOURCE(icon_id),
                                    IMAGE_ICON, 0, 0,
                                    LR_DEFAULTCOLOR | LR_DEFAULTSIZE | LR_SHARED);
    if (tooltip_text != 0) {
        LoadString(hResModule, tooltip_text, buf, ARRAYLENGTH(buf));
    }
    StringCbPrintf(idata.szTip, sizeof(idata.szTip),
                   L"%s%s%s%s",
                   (tooltip_text != 0)? buf : L"",
                   (postfix != NULL)? postfix : L"",
                   (tooltip_text != 0 || postfix != NULL)? L"\n": L"",
                   _T(AFS_VERINFO_BUILD));

    Shell_NotifyIcon(NIM_MODIFY, &idata);
}

struct state_data {
    enum notification_icon_state state;
    khm_handle credset;
};

#define COLLECT_STR_LEN 256

static khm_int32 KHMAPI
collect_cell_names(khm_handle cred, void * rock)
{
    wchar_t *str = (wchar_t *) rock;
    wchar_t cell[KCDB_MAXCCH_NAME] = L"";
    FILETIME ft_now;
    FILETIME ft_expire;
    khm_size cb;

    cb = sizeof(ft_expire);
    if (KHM_FAILED(kcdb_cred_get_attr(cred, KCDB_ATTR_EXPIRE, NULL, &ft_expire, &cb)))
        return KHM_ERROR_SUCCESS;

    GetSystemTimeAsFileTime(&ft_now);
    if (CompareFileTime(&ft_now, &ft_expire) >= 0)
        return KHM_ERROR_SUCCESS;

    cb = sizeof(cell);

    if (KHM_SUCCEEDED(kcdb_cred_get_attr(cred, afs_attr_cell, NULL, cell, &cb)) &&
        cell[0]) {
        StringCchCat(str, COLLECT_STR_LEN, cell);
        StringCchCat(str, COLLECT_STR_LEN, L"\n");
    }

    return KHM_ERROR_SUCCESS;
}

static khm_int32 KHMAPI
set_state_from_ui_thread(HWND hwnd_main, void * stuff)
{
    struct state_data * d = (struct state_data *) stuff;

    initialize_if_necessary();

    switch (d->state) {
    case AFSICON_REPORT_TOKENS:
        {
            wchar_t cells[COLLECT_STR_LEN] = L"";

            kcdb_credset_apply(d->credset, collect_cell_names, cells);

            if (cells[0] == L'\0') {
                set_tooltip_and_icon(IDS_CRED_TT_NONE, NULL, IDI_CRED_NONE);
                break;
            }

            set_tooltip_and_icon(0, cells, IDI_CRED_OK);
        }
        break;

    case AFSICON_SERVICE_STOPPED:
        set_tooltip_and_icon(IDS_CRED_TT_NOS, NULL, IDI_CRED_SVCSTOP);
        break;

    case AFSICON_SERVICE_ERROR:
        set_tooltip_and_icon(IDS_CRED_TT_SERR, NULL, IDI_CRED_BROKEN);
        break;

    default:
        assert(FALSE);
    }

    return KHM_ERROR_SUCCESS;
}

void
afs_icon_set_state(enum notification_icon_state state,
                   khm_handle credset_with_tokens)
{
    struct state_data d;

#if KH_VERSION_API < 7
    if (pkhui_request_UI_callback == NULL)
        return;
#endif

    d.state = state;
    d.credset = credset_with_tokens;

    if (notification_icon_added) {
        set_state_from_ui_thread(NULL, &d);
    } else {
        khui_request_UI_callback(set_state_from_ui_thread, &d);
    }
}
