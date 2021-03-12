/*
 * RichEdit - ITextHost implementation for windowed richedit controls
 *
 * Copyright 2009 by Dylan Smith
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS

#include "editor.h"
#include "ole2.h"
#include "richole.h"
#include "imm.h"
#include "textserv.h"
#include "wine/debug.h"
#include "editstr.h"
#include "rtf.h"
#include "res.h"

WINE_DEFAULT_DEBUG_CHANNEL(richedit);

struct host
{
    ITextHost ITextHost_iface;
    LONG ref;
    ITextServices *text_srv;
    ME_TextEditor *editor; /* to be removed */
    HWND window;
    BOOL emulate_10;
    PARAFORMAT2 para_fmt;
};

static const ITextHostVtbl textHostVtbl;

static BOOL listbox_registered;
static BOOL combobox_registered;

struct host *host_create( HWND hwnd, CREATESTRUCTW *cs, BOOL emulate_10 )
{
    struct host *texthost;

    texthost = CoTaskMemAlloc(sizeof(*texthost));
    if (!texthost) return NULL;

    texthost->ITextHost_iface.lpVtbl = &textHostVtbl;
    texthost->ref = 1;
    texthost->window = hwnd;
    texthost->emulate_10 = emulate_10;
    memset( &texthost->para_fmt, 0, sizeof(texthost->para_fmt) );
    texthost->para_fmt.cbSize = sizeof(texthost->para_fmt);
    texthost->para_fmt.dwMask = PFM_ALIGNMENT;
    texthost->para_fmt.wAlignment = PFA_LEFT;
    if (cs->style & ES_RIGHT)
        texthost->para_fmt.wAlignment = PFA_RIGHT;
    if (cs->style & ES_CENTER)
        texthost->para_fmt.wAlignment = PFA_CENTER;
    texthost->editor = NULL;

    return texthost;
}

static inline struct host *impl_from_ITextHost( ITextHost *iface )
{
    return CONTAINING_RECORD( iface, struct host, ITextHost_iface );
}

static HRESULT WINAPI ITextHostImpl_QueryInterface( ITextHost *iface, REFIID riid, void **obj )
{
    struct host *host = impl_from_ITextHost( iface );

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ITextHost))
    {
        *obj = &host->ITextHost_iface;
        ITextHost_AddRef( (ITextHost *)*obj );
        return S_OK;
    }

    FIXME("Unknown interface: %s\n", debugstr_guid(riid));
    return E_NOINTERFACE;
}

static ULONG WINAPI ITextHostImpl_AddRef(ITextHost *iface)
{
    struct host *host = impl_from_ITextHost( iface );
    ULONG ref = InterlockedIncrement( &host->ref );
    return ref;
}

static ULONG WINAPI ITextHostImpl_Release(ITextHost *iface)
{
    struct host *host = impl_from_ITextHost( iface );
    ULONG ref = InterlockedDecrement( &host->ref );

    if (!ref)
    {
        SetWindowLongPtrW( host->window, 0, 0 );
        ITextServices_Release( host->text_srv );
        CoTaskMemFree( host );
    }
    return ref;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxGetDC,4)
DECLSPEC_HIDDEN HDC __thiscall ITextHostImpl_TxGetDC(ITextHost *iface)
{
    struct host *host = impl_from_ITextHost( iface );
    return GetDC( host->window );
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxReleaseDC,8)
DECLSPEC_HIDDEN INT __thiscall ITextHostImpl_TxReleaseDC(ITextHost *iface, HDC hdc)
{
    struct host *host = impl_from_ITextHost( iface );
    return ReleaseDC( host->window, hdc );
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxShowScrollBar,12)
DECLSPEC_HIDDEN BOOL __thiscall ITextHostImpl_TxShowScrollBar( ITextHost *iface, INT bar, BOOL show )
{
    struct host *host = impl_from_ITextHost( iface );
    return ShowScrollBar( host->window, bar, show );
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxEnableScrollBar,12)
DECLSPEC_HIDDEN BOOL __thiscall ITextHostImpl_TxEnableScrollBar( ITextHost *iface, INT bar, INT arrows )
{
    struct host *host = impl_from_ITextHost( iface );
    return EnableScrollBar( host->window, bar, arrows );
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxSetScrollRange,20)
DECLSPEC_HIDDEN BOOL __thiscall ITextHostImpl_TxSetScrollRange( ITextHost *iface, INT bar, LONG min_pos, INT max_pos, BOOL redraw )
{
    struct host *host = impl_from_ITextHost( iface );
    return SetScrollRange( host->window, bar, min_pos, max_pos, redraw );
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxSetScrollPos,16)
DECLSPEC_HIDDEN BOOL __thiscall ITextHostImpl_TxSetScrollPos( ITextHost *iface, INT bar, INT pos, BOOL redraw )
{
    struct host *host = impl_from_ITextHost( iface );
    return SetScrollPos( host->window, bar, pos, redraw ) != 0;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxInvalidateRect,12)
DECLSPEC_HIDDEN void __thiscall ITextHostImpl_TxInvalidateRect( ITextHost *iface, const RECT *rect, BOOL mode )
{
    struct host *host = impl_from_ITextHost( iface );
    InvalidateRect( host->window, rect, mode );
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxViewChange,8)
DECLSPEC_HIDDEN void __thiscall ITextHostImpl_TxViewChange( ITextHost *iface, BOOL update )
{
    struct host *host = impl_from_ITextHost( iface );
    if (update) UpdateWindow( host->window );
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxCreateCaret,16)
DECLSPEC_HIDDEN BOOL __thiscall ITextHostImpl_TxCreateCaret( ITextHost *iface, HBITMAP bitmap, INT width, INT height )
{
    struct host *host = impl_from_ITextHost( iface );
    return CreateCaret( host->window, bitmap, width, height );
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxShowCaret,8)
DECLSPEC_HIDDEN BOOL __thiscall ITextHostImpl_TxShowCaret( ITextHost *iface, BOOL show )
{
    struct host *host = impl_from_ITextHost( iface );
    if (show) return ShowCaret( host->window );
    else return HideCaret( host->window );
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxSetCaretPos,12)
DECLSPEC_HIDDEN BOOL __thiscall ITextHostImpl_TxSetCaretPos( ITextHost *iface, INT x, INT y )
{
    return SetCaretPos(x, y);
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxSetTimer,12)
DECLSPEC_HIDDEN BOOL __thiscall ITextHostImpl_TxSetTimer( ITextHost *iface, UINT id, UINT timeout )
{
    struct host *host = impl_from_ITextHost( iface );
    return SetTimer( host->window, id, timeout, NULL ) != 0;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxKillTimer,8)
DECLSPEC_HIDDEN void __thiscall ITextHostImpl_TxKillTimer( ITextHost *iface, UINT id )
{
    struct host *host = impl_from_ITextHost( iface );
    KillTimer( host->window, id );
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxScrollWindowEx,32)
DECLSPEC_HIDDEN void __thiscall ITextHostImpl_TxScrollWindowEx( ITextHost *iface, INT dx, INT dy, const RECT *scroll,
                                                                const RECT *clip, HRGN update_rgn, RECT *update_rect,
                                                                UINT flags )
{
    struct host *host = impl_from_ITextHost( iface );
    ScrollWindowEx( host->window, dx, dy, scroll, clip, update_rgn, update_rect, flags );
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxSetCapture,8)
DECLSPEC_HIDDEN void __thiscall ITextHostImpl_TxSetCapture( ITextHost *iface, BOOL capture )
{
    struct host *host = impl_from_ITextHost( iface );
    if (capture) SetCapture( host->window );
    else ReleaseCapture();
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxSetFocus,4)
DECLSPEC_HIDDEN void __thiscall ITextHostImpl_TxSetFocus(ITextHost *iface)
{
    struct host *host = impl_from_ITextHost( iface );
    SetFocus( host->window );
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxSetCursor,12)
DECLSPEC_HIDDEN void __thiscall ITextHostImpl_TxSetCursor( ITextHost *iface, HCURSOR cursor, BOOL text )
{
    SetCursor( cursor );
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxScreenToClient,8)
DECLSPEC_HIDDEN BOOL __thiscall ITextHostImpl_TxScreenToClient( ITextHost *iface, POINT *pt )
{
    struct host *host = impl_from_ITextHost( iface );
    return ScreenToClient( host->window, pt );
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxClientToScreen,8)
DECLSPEC_HIDDEN BOOL __thiscall ITextHostImpl_TxClientToScreen( ITextHost *iface, POINT *pt )
{
    struct host *host = impl_from_ITextHost( iface );
    return ClientToScreen( host->window, pt );
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxActivate,8)
DECLSPEC_HIDDEN HRESULT __thiscall ITextHostImpl_TxActivate( ITextHost *iface, LONG *old_state )
{
    struct host *host = impl_from_ITextHost( iface );
    *old_state = HandleToLong( SetActiveWindow( host->window ) );
    return *old_state ? S_OK : E_FAIL;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxDeactivate,8)
DECLSPEC_HIDDEN HRESULT __thiscall ITextHostImpl_TxDeactivate( ITextHost *iface, LONG new_state )
{
    HWND ret = SetActiveWindow( LongToHandle( new_state ) );
    return ret ? S_OK : E_FAIL;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxGetClientRect,8)
DECLSPEC_HIDDEN HRESULT __thiscall ITextHostImpl_TxGetClientRect( ITextHost *iface, RECT *rect )
{
    struct host *host = impl_from_ITextHost( iface );
    int ret = GetClientRect( host->window, rect );
    return ret ? S_OK : E_FAIL;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxGetViewInset,8)
DECLSPEC_HIDDEN HRESULT __thiscall ITextHostImpl_TxGetViewInset( ITextHost *iface, RECT *rect )
{
    SetRectEmpty( rect );
    return S_OK;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxGetCharFormat,8)
DECLSPEC_HIDDEN HRESULT __thiscall ITextHostImpl_TxGetCharFormat(ITextHost *iface, const CHARFORMATW **ppCF)
{
    return E_NOTIMPL;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxGetParaFormat,8)
DECLSPEC_HIDDEN HRESULT __thiscall ITextHostImpl_TxGetParaFormat( ITextHost *iface, const PARAFORMAT **fmt )
{
    struct host *host = impl_from_ITextHost( iface );
    *fmt = (const PARAFORMAT *)&host->para_fmt;
    return S_OK;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxGetSysColor,8)
DECLSPEC_HIDDEN COLORREF __thiscall ITextHostImpl_TxGetSysColor( ITextHost *iface, int index )
{
    return GetSysColor( index );
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxGetBackStyle,8)
DECLSPEC_HIDDEN HRESULT __thiscall ITextHostImpl_TxGetBackStyle( ITextHost *iface, TXTBACKSTYLE *style )
{
    *style = TXTBACK_OPAQUE;
    return S_OK;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxGetMaxLength,8)
DECLSPEC_HIDDEN HRESULT __thiscall ITextHostImpl_TxGetMaxLength( ITextHost *iface, DWORD *length )
{
    *length = INFINITE;
    return S_OK;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxGetScrollBars,8)
DECLSPEC_HIDDEN HRESULT __thiscall ITextHostImpl_TxGetScrollBars( ITextHost *iface, DWORD *scrollbar )
{
    struct host *host = impl_from_ITextHost( iface );
    const DWORD mask = WS_VSCROLL|
                       WS_HSCROLL|
                       ES_AUTOVSCROLL|
                       ES_AUTOHSCROLL|
                       ES_DISABLENOSCROLL;
    if (host->editor)
    {
        *scrollbar = host->editor->styleFlags & mask;
    }
    else
    {
        DWORD style = GetWindowLongW( host->window, GWL_STYLE );
        if (style & WS_VSCROLL)
            style |= ES_AUTOVSCROLL;
        if (!host->emulate_10 && (style & WS_HSCROLL))
            style |= ES_AUTOHSCROLL;
        *scrollbar = style & mask;
    }
    return S_OK;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxGetPasswordChar,8)
DECLSPEC_HIDDEN HRESULT __thiscall ITextHostImpl_TxGetPasswordChar( ITextHost *iface, WCHAR *c )
{
    *c = '*';
    return S_OK;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxGetAcceleratorPos,8)
DECLSPEC_HIDDEN HRESULT __thiscall ITextHostImpl_TxGetAcceleratorPos( ITextHost *iface, LONG *pos )
{
    *pos = -1;
    return S_OK;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxGetExtent,8)
DECLSPEC_HIDDEN HRESULT __thiscall ITextHostImpl_TxGetExtent( ITextHost *iface, SIZEL *extent )
{
    return E_NOTIMPL;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_OnTxCharFormatChange,8)
DECLSPEC_HIDDEN HRESULT __thiscall ITextHostImpl_OnTxCharFormatChange(ITextHost *iface, const CHARFORMATW *pcf)
{
    return S_OK;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_OnTxParaFormatChange,8)
DECLSPEC_HIDDEN HRESULT __thiscall ITextHostImpl_OnTxParaFormatChange(ITextHost *iface, const PARAFORMAT *ppf)
{
    return S_OK;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxGetPropertyBits,12)
DECLSPEC_HIDDEN HRESULT __thiscall ITextHostImpl_TxGetPropertyBits( ITextHost *iface, DWORD mask, DWORD *bits )
{
    struct host *host = impl_from_ITextHost( iface );
    DWORD style;
    DWORD dwBits = 0;

    if (host->editor)
    {
        style = host->editor->styleFlags;
        if (host->editor->mode & TM_RICHTEXT)
            dwBits |= TXTBIT_RICHTEXT;
        if (host->editor->bWordWrap)
            dwBits |= TXTBIT_WORDWRAP;
        if (style & ECO_AUTOWORDSELECTION)
            dwBits |= TXTBIT_AUTOWORDSEL;
    }
    else
    {
        DWORD dwScrollBar;

        style = GetWindowLongW( host->window, GWL_STYLE );
        ITextHostImpl_TxGetScrollBars(iface, &dwScrollBar);

        dwBits |= TXTBIT_RICHTEXT|TXTBIT_AUTOWORDSEL;
        if (!(dwScrollBar & ES_AUTOHSCROLL))
            dwBits |= TXTBIT_WORDWRAP;
    }

    /* Bits that correspond to window styles. */
    if (style & ES_MULTILINE)
        dwBits |= TXTBIT_MULTILINE;
    if (style & ES_READONLY)
        dwBits |= TXTBIT_READONLY;
    if (style & ES_PASSWORD)
        dwBits |= TXTBIT_USEPASSWORD;
    if (!(style & ES_NOHIDESEL))
        dwBits |= TXTBIT_HIDESELECTION;
    if (style & ES_SAVESEL)
        dwBits |= TXTBIT_SAVESELECTION;
    if (style & ES_VERTICAL)
        dwBits |= TXTBIT_VERTICAL;
    if (style & ES_NOOLEDRAGDROP)
        dwBits |= TXTBIT_DISABLEDRAG;

    dwBits |= TXTBIT_ALLOWBEEP;

    /* The following bits are always FALSE because they are probably only
     * needed for ITextServices_OnTxPropertyBitsChange:
     *   TXTBIT_VIEWINSETCHANGE
     *   TXTBIT_BACKSTYLECHANGE
     *   TXTBIT_MAXLENGTHCHANGE
     *   TXTBIT_CHARFORMATCHANGE
     *   TXTBIT_PARAFORMATCHANGE
     *   TXTBIT_SHOWACCELERATOR
     *   TXTBIT_EXTENTCHANGE
     *   TXTBIT_SELBARCHANGE
     *   TXTBIT_SCROLLBARCHANGE
     *   TXTBIT_CLIENTRECTCHANGE
     *
     * Documented by MSDN as not supported:
     *   TXTBIT_USECURRENTBKG
     */

    *bits = dwBits & mask;
    return S_OK;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxNotify,12)
DECLSPEC_HIDDEN HRESULT __thiscall ITextHostImpl_TxNotify(ITextHost *iface, DWORD iNotify, void *pv)
{
    struct host *host = impl_from_ITextHost( iface );
    HWND hwnd = host->window;
    UINT id;

    if (!host->editor || !host->editor->hwndParent) return S_OK;

    id = GetWindowLongW(hwnd, GWLP_ID);

    switch (iNotify)
    {
        case EN_DROPFILES:
        case EN_LINK:
        case EN_OLEOPFAILED:
        case EN_PROTECTED:
        case EN_REQUESTRESIZE:
        case EN_SAVECLIPBOARD:
        case EN_SELCHANGE:
        case EN_STOPNOUNDO:
        {
            /* FIXME: Verify this assumption that pv starts with NMHDR. */
            NMHDR *info = pv;
            if (!info)
                return E_FAIL;

            info->hwndFrom = hwnd;
            info->idFrom = id;
            info->code = iNotify;
            SendMessageW( host->editor->hwndParent, WM_NOTIFY, id, (LPARAM)info );
            break;
        }

        case EN_UPDATE:
            /* Only sent when the window is visible. */
            if (!IsWindowVisible(hwnd))
                break;
            /* Fall through */
        case EN_CHANGE:
        case EN_ERRSPACE:
        case EN_HSCROLL:
        case EN_KILLFOCUS:
        case EN_MAXTEXT:
        case EN_SETFOCUS:
        case EN_VSCROLL:
            SendMessageW( host->editor->hwndParent, WM_COMMAND, MAKEWPARAM( id, iNotify ), (LPARAM)hwnd );
            break;

        case EN_MSGFILTER:
            FIXME("EN_MSGFILTER is documented as not being sent to TxNotify\n");
            /* fall through */
        default:
            return E_FAIL;
    }
    return S_OK;
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxImmGetContext,4)
DECLSPEC_HIDDEN HIMC __thiscall ITextHostImpl_TxImmGetContext(ITextHost *iface)
{
    struct host *host = impl_from_ITextHost( iface );
    return ImmGetContext( host->window );
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxImmReleaseContext,8)
DECLSPEC_HIDDEN void __thiscall ITextHostImpl_TxImmReleaseContext( ITextHost *iface, HIMC context )
{
    struct host *host = impl_from_ITextHost( iface );
    ImmReleaseContext( host->window, context );
}

DEFINE_THISCALL_WRAPPER(ITextHostImpl_TxGetSelectionBarWidth,8)
DECLSPEC_HIDDEN HRESULT __thiscall ITextHostImpl_TxGetSelectionBarWidth( ITextHost *iface, LONG *width )
{
    struct host *host = impl_from_ITextHost( iface );

    DWORD style = host->editor ? host->editor->styleFlags : GetWindowLongW( host->window, GWL_STYLE );
    *width = (style & ES_SELECTIONBAR) ? 225 : 0; /* in HIMETRIC */
    return S_OK;
}


#ifdef __ASM_USE_THISCALL_WRAPPER

#define STDCALL(func) (void *) __stdcall_ ## func
#ifdef _MSC_VER
#define DEFINE_STDCALL_WRAPPER(num,func,args) \
    __declspec(naked) HRESULT __stdcall_##func(void) \
    { \
        __asm pop eax \
        __asm pop ecx \
        __asm push eax \
        __asm mov eax, [ecx] \
        __asm jmp dword ptr [eax + 4*num] \
    }
#else /* _MSC_VER */
#define DEFINE_STDCALL_WRAPPER(num,func,args) \
   extern HRESULT __stdcall_ ## func(void); \
   __ASM_GLOBAL_FUNC(__stdcall_ ## func, \
                   "popl %eax\n\t" \
                   "popl %ecx\n\t" \
                   "pushl %eax\n\t" \
                   "movl (%ecx), %eax\n\t" \
                   "jmp *(4*(" #num "))(%eax)" )
#endif /* _MSC_VER */

DEFINE_STDCALL_WRAPPER(3,ITextHostImpl_TxGetDC,4)
DEFINE_STDCALL_WRAPPER(4,ITextHostImpl_TxReleaseDC,8)
DEFINE_STDCALL_WRAPPER(5,ITextHostImpl_TxShowScrollBar,12)
DEFINE_STDCALL_WRAPPER(6,ITextHostImpl_TxEnableScrollBar,12)
DEFINE_STDCALL_WRAPPER(7,ITextHostImpl_TxSetScrollRange,20)
DEFINE_STDCALL_WRAPPER(8,ITextHostImpl_TxSetScrollPos,16)
DEFINE_STDCALL_WRAPPER(9,ITextHostImpl_TxInvalidateRect,12)
DEFINE_STDCALL_WRAPPER(10,ITextHostImpl_TxViewChange,8)
DEFINE_STDCALL_WRAPPER(11,ITextHostImpl_TxCreateCaret,16)
DEFINE_STDCALL_WRAPPER(12,ITextHostImpl_TxShowCaret,8)
DEFINE_STDCALL_WRAPPER(13,ITextHostImpl_TxSetCaretPos,12)
DEFINE_STDCALL_WRAPPER(14,ITextHostImpl_TxSetTimer,12)
DEFINE_STDCALL_WRAPPER(15,ITextHostImpl_TxKillTimer,8)
DEFINE_STDCALL_WRAPPER(16,ITextHostImpl_TxScrollWindowEx,32)
DEFINE_STDCALL_WRAPPER(17,ITextHostImpl_TxSetCapture,8)
DEFINE_STDCALL_WRAPPER(18,ITextHostImpl_TxSetFocus,4)
DEFINE_STDCALL_WRAPPER(19,ITextHostImpl_TxSetCursor,12)
DEFINE_STDCALL_WRAPPER(20,ITextHostImpl_TxScreenToClient,8)
DEFINE_STDCALL_WRAPPER(21,ITextHostImpl_TxClientToScreen,8)
DEFINE_STDCALL_WRAPPER(22,ITextHostImpl_TxActivate,8)
DEFINE_STDCALL_WRAPPER(23,ITextHostImpl_TxDeactivate,8)
DEFINE_STDCALL_WRAPPER(24,ITextHostImpl_TxGetClientRect,8)
DEFINE_STDCALL_WRAPPER(25,ITextHostImpl_TxGetViewInset,8)
DEFINE_STDCALL_WRAPPER(26,ITextHostImpl_TxGetCharFormat,8)
DEFINE_STDCALL_WRAPPER(27,ITextHostImpl_TxGetParaFormat,8)
DEFINE_STDCALL_WRAPPER(28,ITextHostImpl_TxGetSysColor,8)
DEFINE_STDCALL_WRAPPER(29,ITextHostImpl_TxGetBackStyle,8)
DEFINE_STDCALL_WRAPPER(30,ITextHostImpl_TxGetMaxLength,8)
DEFINE_STDCALL_WRAPPER(31,ITextHostImpl_TxGetScrollBars,8)
DEFINE_STDCALL_WRAPPER(32,ITextHostImpl_TxGetPasswordChar,8)
DEFINE_STDCALL_WRAPPER(33,ITextHostImpl_TxGetAcceleratorPos,8)
DEFINE_STDCALL_WRAPPER(34,ITextHostImpl_TxGetExtent,8)
DEFINE_STDCALL_WRAPPER(35,ITextHostImpl_OnTxCharFormatChange,8)
DEFINE_STDCALL_WRAPPER(36,ITextHostImpl_OnTxParaFormatChange,8)
DEFINE_STDCALL_WRAPPER(37,ITextHostImpl_TxGetPropertyBits,12)
DEFINE_STDCALL_WRAPPER(38,ITextHostImpl_TxNotify,12)
DEFINE_STDCALL_WRAPPER(39,ITextHostImpl_TxImmGetContext,4)
DEFINE_STDCALL_WRAPPER(40,ITextHostImpl_TxImmReleaseContext,8)
DEFINE_STDCALL_WRAPPER(41,ITextHostImpl_TxGetSelectionBarWidth,8)

const ITextHostVtbl text_host_stdcall_vtbl =
{
    NULL,
    NULL,
    NULL,
    STDCALL(ITextHostImpl_TxGetDC),
    STDCALL(ITextHostImpl_TxReleaseDC),
    STDCALL(ITextHostImpl_TxShowScrollBar),
    STDCALL(ITextHostImpl_TxEnableScrollBar),
    STDCALL(ITextHostImpl_TxSetScrollRange),
    STDCALL(ITextHostImpl_TxSetScrollPos),
    STDCALL(ITextHostImpl_TxInvalidateRect),
    STDCALL(ITextHostImpl_TxViewChange),
    STDCALL(ITextHostImpl_TxCreateCaret),
    STDCALL(ITextHostImpl_TxShowCaret),
    STDCALL(ITextHostImpl_TxSetCaretPos),
    STDCALL(ITextHostImpl_TxSetTimer),
    STDCALL(ITextHostImpl_TxKillTimer),
    STDCALL(ITextHostImpl_TxScrollWindowEx),
    STDCALL(ITextHostImpl_TxSetCapture),
    STDCALL(ITextHostImpl_TxSetFocus),
    STDCALL(ITextHostImpl_TxSetCursor),
    STDCALL(ITextHostImpl_TxScreenToClient),
    STDCALL(ITextHostImpl_TxClientToScreen),
    STDCALL(ITextHostImpl_TxActivate),
    STDCALL(ITextHostImpl_TxDeactivate),
    STDCALL(ITextHostImpl_TxGetClientRect),
    STDCALL(ITextHostImpl_TxGetViewInset),
    STDCALL(ITextHostImpl_TxGetCharFormat),
    STDCALL(ITextHostImpl_TxGetParaFormat),
    STDCALL(ITextHostImpl_TxGetSysColor),
    STDCALL(ITextHostImpl_TxGetBackStyle),
    STDCALL(ITextHostImpl_TxGetMaxLength),
    STDCALL(ITextHostImpl_TxGetScrollBars),
    STDCALL(ITextHostImpl_TxGetPasswordChar),
    STDCALL(ITextHostImpl_TxGetAcceleratorPos),
    STDCALL(ITextHostImpl_TxGetExtent),
    STDCALL(ITextHostImpl_OnTxCharFormatChange),
    STDCALL(ITextHostImpl_OnTxParaFormatChange),
    STDCALL(ITextHostImpl_TxGetPropertyBits),
    STDCALL(ITextHostImpl_TxNotify),
    STDCALL(ITextHostImpl_TxImmGetContext),
    STDCALL(ITextHostImpl_TxImmReleaseContext),
    STDCALL(ITextHostImpl_TxGetSelectionBarWidth),
};

#endif /* __ASM_USE_THISCALL_WRAPPER */

static const ITextHostVtbl textHostVtbl =
{
    ITextHostImpl_QueryInterface,
    ITextHostImpl_AddRef,
    ITextHostImpl_Release,
    THISCALL(ITextHostImpl_TxGetDC),
    THISCALL(ITextHostImpl_TxReleaseDC),
    THISCALL(ITextHostImpl_TxShowScrollBar),
    THISCALL(ITextHostImpl_TxEnableScrollBar),
    THISCALL(ITextHostImpl_TxSetScrollRange),
    THISCALL(ITextHostImpl_TxSetScrollPos),
    THISCALL(ITextHostImpl_TxInvalidateRect),
    THISCALL(ITextHostImpl_TxViewChange),
    THISCALL(ITextHostImpl_TxCreateCaret),
    THISCALL(ITextHostImpl_TxShowCaret),
    THISCALL(ITextHostImpl_TxSetCaretPos),
    THISCALL(ITextHostImpl_TxSetTimer),
    THISCALL(ITextHostImpl_TxKillTimer),
    THISCALL(ITextHostImpl_TxScrollWindowEx),
    THISCALL(ITextHostImpl_TxSetCapture),
    THISCALL(ITextHostImpl_TxSetFocus),
    THISCALL(ITextHostImpl_TxSetCursor),
    THISCALL(ITextHostImpl_TxScreenToClient),
    THISCALL(ITextHostImpl_TxClientToScreen),
    THISCALL(ITextHostImpl_TxActivate),
    THISCALL(ITextHostImpl_TxDeactivate),
    THISCALL(ITextHostImpl_TxGetClientRect),
    THISCALL(ITextHostImpl_TxGetViewInset),
    THISCALL(ITextHostImpl_TxGetCharFormat),
    THISCALL(ITextHostImpl_TxGetParaFormat),
    THISCALL(ITextHostImpl_TxGetSysColor),
    THISCALL(ITextHostImpl_TxGetBackStyle),
    THISCALL(ITextHostImpl_TxGetMaxLength),
    THISCALL(ITextHostImpl_TxGetScrollBars),
    THISCALL(ITextHostImpl_TxGetPasswordChar),
    THISCALL(ITextHostImpl_TxGetAcceleratorPos),
    THISCALL(ITextHostImpl_TxGetExtent),
    THISCALL(ITextHostImpl_OnTxCharFormatChange),
    THISCALL(ITextHostImpl_OnTxParaFormatChange),
    THISCALL(ITextHostImpl_TxGetPropertyBits),
    THISCALL(ITextHostImpl_TxNotify),
    THISCALL(ITextHostImpl_TxImmGetContext),
    THISCALL(ITextHostImpl_TxImmReleaseContext),
    THISCALL(ITextHostImpl_TxGetSelectionBarWidth),
};

static const char * const edit_messages[] =
{
    "EM_GETSEL",           "EM_SETSEL",           "EM_GETRECT",             "EM_SETRECT",
    "EM_SETRECTNP",        "EM_SCROLL",           "EM_LINESCROLL",          "EM_SCROLLCARET",
    "EM_GETMODIFY",        "EM_SETMODIFY",        "EM_GETLINECOUNT",        "EM_LINEINDEX",
    "EM_SETHANDLE",        "EM_GETHANDLE",        "EM_GETTHUMB",            "EM_UNKNOWN_BF",
    "EM_UNKNOWN_C0",       "EM_LINELENGTH",       "EM_REPLACESEL",          "EM_UNKNOWN_C3",
    "EM_GETLINE",          "EM_LIMITTEXT",        "EM_CANUNDO",             "EM_UNDO",
    "EM_FMTLINES",         "EM_LINEFROMCHAR",     "EM_UNKNOWN_CA",          "EM_SETTABSTOPS",
    "EM_SETPASSWORDCHAR",  "EM_EMPTYUNDOBUFFER",  "EM_GETFIRSTVISIBLELINE", "EM_SETREADONLY",
    "EM_SETWORDBREAKPROC", "EM_GETWORDBREAKPROC", "EM_GETPASSWORDCHAR",     "EM_SETMARGINS",
    "EM_GETMARGINS",       "EM_GETLIMITTEXT",     "EM_POSFROMCHAR",         "EM_CHARFROMPOS",
    "EM_SETIMESTATUS",     "EM_GETIMESTATUS"
};

static const char * const richedit_messages[] =
{
    "EM_CANPASTE",         "EM_DISPLAYBAND",      "EM_EXGETSEL",            "EM_EXLIMITTEXT",
    "EM_EXLINEFROMCHAR",   "EM_EXSETSEL",         "EM_FINDTEXT",            "EM_FORMATRANGE",
    "EM_GETCHARFORMAT",    "EM_GETEVENTMASK",     "EM_GETOLEINTERFACE",     "EM_GETPARAFORMAT",
    "EM_GETSELTEXT",       "EM_HIDESELECTION",    "EM_PASTESPECIAL",        "EM_REQUESTRESIZE",
    "EM_SELECTIONTYPE",    "EM_SETBKGNDCOLOR",    "EM_SETCHARFORMAT",       "EM_SETEVENTMASK",
    "EM_SETOLECALLBACK",   "EM_SETPARAFORMAT",    "EM_SETTARGETDEVICE",     "EM_STREAMIN",
    "EM_STREAMOUT",        "EM_GETTEXTRANGE",     "EM_FINDWORDBREAK",       "EM_SETOPTIONS",
    "EM_GETOPTIONS",       "EM_FINDTEXTEX",       "EM_GETWORDBREAKPROCEX",  "EM_SETWORDBREAKPROCEX",
    "EM_SETUNDOLIMIT",     "EM_UNKNOWN_USER_83",  "EM_REDO",                "EM_CANREDO",
    "EM_GETUNDONAME",      "EM_GETREDONAME",      "EM_STOPGROUPTYPING",     "EM_SETTEXTMODE",
    "EM_GETTEXTMODE",      "EM_AUTOURLDETECT",    "EM_GETAUTOURLDETECT",    "EM_SETPALETTE",
    "EM_GETTEXTEX",        "EM_GETTEXTLENGTHEX",  "EM_SHOWSCROLLBAR",       "EM_SETTEXTEX",
    "EM_UNKNOWN_USER_98",  "EM_UNKNOWN_USER_99",  "EM_SETPUNCTUATION",      "EM_GETPUNCTUATION",
    "EM_SETWORDWRAPMODE",  "EM_GETWORDWRAPMODE",  "EM_SETIMECOLOR",         "EM_GETIMECOLOR",
    "EM_SETIMEOPTIONS",    "EM_GETIMEOPTIONS",    "EM_CONVPOSITION",        "EM_UNKNOWN_USER_109",
    "EM_UNKNOWN_USER_110", "EM_UNKNOWN_USER_111", "EM_UNKNOWN_USER_112",    "EM_UNKNOWN_USER_113",
    "EM_UNKNOWN_USER_114", "EM_UNKNOWN_USER_115", "EM_UNKNOWN_USER_116",    "EM_UNKNOWN_USER_117",
    "EM_UNKNOWN_USER_118", "EM_UNKNOWN_USER_119", "EM_SETLANGOPTIONS",      "EM_GETLANGOPTIONS",
    "EM_GETIMECOMPMODE",   "EM_FINDTEXTW",        "EM_FINDTEXTEXW",         "EM_RECONVERSION",
    "EM_SETIMEMODEBIAS",   "EM_GETIMEMODEBIAS"
};

static const char *get_msg_name( UINT msg )
{
    if (msg >= EM_GETSEL && msg <= EM_CHARFROMPOS)
        return edit_messages[msg - EM_GETSEL];
    if (msg >= EM_CANPASTE && msg <= EM_GETIMEMODEBIAS)
        return richedit_messages[msg - EM_CANPASTE];
    return "";
}

static BOOL create_windowed_editor( HWND hwnd, CREATESTRUCTW *create, BOOL emulate_10 )
{
    struct host *host = host_create( hwnd, create, emulate_10 );
    IUnknown *unk;
    HRESULT hr;

    if (!host) return FALSE;

    hr = create_text_services( NULL, &host->ITextHost_iface, &unk, emulate_10, &host->editor );
    if (FAILED( hr ))
    {
        ITextHost_Release( &host->ITextHost_iface );
        return FALSE;
    }
    IUnknown_QueryInterface( unk, &IID_ITextServices, (void **)&host->text_srv );
    IUnknown_Release( unk );

    host->editor->exStyleFlags = GetWindowLongW( hwnd, GWL_EXSTYLE );
    host->editor->styleFlags |= GetWindowLongW( hwnd, GWL_STYLE ) & ES_WANTRETURN;
    host->editor->hWnd = hwnd; /* FIXME: Remove editor's dependence on hWnd */
    host->editor->hwndParent = create->hwndParent;

    SetWindowLongPtrW( hwnd, 0, (LONG_PTR)host );

    return TRUE;
}

static LRESULT RichEditWndProc_common( HWND hwnd, UINT msg, WPARAM wparam,
                                       LPARAM lparam, BOOL unicode )
{
    struct host *host;
    ME_TextEditor *editor;
    HRESULT hr;
    LRESULT res = 0;

    TRACE( "enter hwnd %p msg %04x (%s) %lx %lx, unicode %d\n",
           hwnd, msg, get_msg_name(msg), wparam, lparam, unicode );

    host = (struct host *)GetWindowLongPtrW( hwnd, 0 );
    if (!host)
    {
        if (msg == WM_NCCREATE)
        {
            CREATESTRUCTW *pcs = (CREATESTRUCTW *)lparam;

            TRACE( "WM_NCCREATE: hwnd %p style 0x%08x\n", hwnd, pcs->style );
            return create_windowed_editor( hwnd, pcs, FALSE );
        }
        else return DefWindowProcW( hwnd, msg, wparam, lparam );
    }

    editor = host->editor;
    switch (msg)
    {
    case WM_CHAR:
    {
        WCHAR wc = wparam;

        if (!unicode) MultiByteToWideChar( CP_ACP, 0, (char *)&wparam, 1, &wc, 1 );
        hr = ITextServices_TxSendMessage( host->text_srv, msg, wc, lparam, &res );
        break;
    }
    case WM_DESTROY:
        ITextHost_Release( &host->ITextHost_iface );
        return 0;

    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wparam;
        RECT rc;

        if (GetUpdateRect( editor->hWnd, &rc, TRUE ))
            FillRect( hdc, &rc, editor->hbrBackground );
        return 1;
    }
    case WM_GETTEXTLENGTH:
    {
        GETTEXTLENGTHEX params;

        params.flags = GTL_CLOSE | (host->emulate_10 ? 0 : GTL_USECRLF) | GTL_NUMCHARS;
        params.codepage = unicode ? CP_UNICODE : CP_ACP;
        hr = ITextServices_TxSendMessage( host->text_srv, EM_GETTEXTLENGTHEX, (WPARAM)&params, 0, &res );
        break;
    }
    case WM_PAINT:
    {
        HDC hdc;
        RECT rc;
        PAINTSTRUCT ps;
        HBRUSH old_brush;

        update_caret( editor );
        hdc = BeginPaint( editor->hWnd, &ps );
        if (!editor->bEmulateVersion10 || (editor->nEventMask & ENM_UPDATE))
            ME_SendOldNotify( editor, EN_UPDATE );
        old_brush = SelectObject( hdc, editor->hbrBackground );

        /* Erase area outside of the formatting rectangle */
        if (ps.rcPaint.top < editor->rcFormat.top)
        {
            rc = ps.rcPaint;
            rc.bottom = editor->rcFormat.top;
            PatBlt( hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, PATCOPY );
            ps.rcPaint.top = editor->rcFormat.top;
        }
        if (ps.rcPaint.bottom > editor->rcFormat.bottom)
        {
            rc = ps.rcPaint;
            rc.top = editor->rcFormat.bottom;
            PatBlt( hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, PATCOPY );
            ps.rcPaint.bottom = editor->rcFormat.bottom;
        }
        if (ps.rcPaint.left < editor->rcFormat.left)
        {
            rc = ps.rcPaint;
            rc.right = editor->rcFormat.left;
            PatBlt( hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, PATCOPY );
            ps.rcPaint.left = editor->rcFormat.left;
        }
        if (ps.rcPaint.right > editor->rcFormat.right)
        {
            rc = ps.rcPaint;
            rc.left = editor->rcFormat.right;
            PatBlt( hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, PATCOPY );
            ps.rcPaint.right = editor->rcFormat.right;
        }

        ME_PaintContent( editor, hdc, &ps.rcPaint );
        SelectObject( hdc, old_brush );
        EndPaint( editor->hWnd, &ps );
        return 0;
    }
    case EM_REPLACESEL:
    {
        int len;
        LONG codepage = unicode ? CP_UNICODE : CP_ACP;
        WCHAR *text = ME_ToUnicode( codepage, (void *)lparam, &len );

        hr = ITextServices_TxSendMessage( host->text_srv, msg, wparam, (LPARAM)text, &res );
        ME_EndToUnicode( codepage, text );
        res = len;
        break;
    }
    case EM_SETOPTIONS:
    {
        DWORD style;
        const DWORD mask = ECO_VERTICAL | ECO_AUTOHSCROLL | ECO_AUTOVSCROLL |
            ECO_NOHIDESEL | ECO_READONLY | ECO_WANTRETURN |
            ECO_SELECTIONBAR;

        res = ME_HandleMessage( editor, msg, wparam, lparam, unicode, &hr );
        style = GetWindowLongW( hwnd, GWL_STYLE );
        style = (style & ~mask) | (res & mask);
        SetWindowLongW( hwnd, GWL_STYLE, style );
        return res;
    }
    case EM_SETREADONLY:
    {
        DWORD style;

        res = ME_HandleMessage( editor, msg, wparam, lparam, unicode, &hr );
        style = GetWindowLongW( hwnd, GWL_STYLE );
        style &= ~ES_READONLY;
        if (wparam) style |= ES_READONLY;
        SetWindowLongW( hwnd, GWL_STYLE, style );
        return res;
    }
    case WM_SETTEXT:
    {
        char *textA = (char *)lparam;
        WCHAR *text = (WCHAR *)lparam;
        int len;

        if (!unicode && textA && strncmp( textA, "{\\rtf", 5 ) && strncmp( textA, "{\\urtf", 6 ))
            text = ME_ToUnicode( CP_ACP, textA, &len );
        hr = ITextServices_TxSendMessage( host->text_srv, msg, wparam, (LPARAM)text, &res );
        if (text != (WCHAR *)lparam) ME_EndToUnicode( CP_ACP, text );
        break;
    }
    default:
        res = ME_HandleMessage( editor, msg, wparam, lparam, unicode, &hr );
    }

    if (hr == S_FALSE)
        res = DefWindowProcW( hwnd, msg, wparam, lparam );

    TRACE( "exit hwnd %p msg %04x (%s) %lx %lx, unicode %d -> %lu\n",
           hwnd, msg, get_msg_name(msg), wparam, lparam, unicode, res );

    return res;
}

static LRESULT WINAPI RichEditWndProcW( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    BOOL unicode = TRUE;

    /* Under Win9x RichEdit20W returns ANSI strings, see the tests. */
    if (msg == WM_GETTEXT && (GetVersion() & 0x80000000))
        unicode = FALSE;

    return RichEditWndProc_common( hwnd, msg, wparam, lparam, unicode );
}

static LRESULT WINAPI RichEditWndProcA( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    return RichEditWndProc_common( hwnd, msg, wparam, lparam, FALSE );
}

/******************************************************************
 *        RichEditANSIWndProc (RICHED20.10)
 */
LRESULT WINAPI RichEditANSIWndProc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    return RichEditWndProcA( hwnd, msg, wparam, lparam );
}

/******************************************************************
 *        RichEdit10ANSIWndProc (RICHED20.9)
 */
LRESULT WINAPI RichEdit10ANSIWndProc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    if (msg == WM_NCCREATE && !GetWindowLongPtrW( hwnd, 0 ))
    {
        CREATESTRUCTW *pcs = (CREATESTRUCTW *)lparam;

        TRACE( "WM_NCCREATE: hwnd %p style 0x%08x\n", hwnd, pcs->style );
        return create_windowed_editor( hwnd, pcs, TRUE );
    }
    return RichEditANSIWndProc( hwnd, msg, wparam, lparam );
}

static LRESULT WINAPI REComboWndProc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    /* FIXME: Not implemented */
    TRACE( "hwnd %p msg %04x (%s) %08lx %08lx\n",
           hwnd, msg, get_msg_name( msg ), wparam, lparam );
    return DefWindowProcW( hwnd, msg, wparam, lparam );
}

static LRESULT WINAPI REListWndProc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    /* FIXME: Not implemented */
    TRACE( "hwnd %p msg %04x (%s) %08lx %08lx\n",
           hwnd, msg, get_msg_name( msg ), wparam, lparam );
    return DefWindowProcW( hwnd, msg, wparam, lparam );
}

/******************************************************************
 *        REExtendedRegisterClass (RICHED20.8)
 *
 * FIXME undocumented
 * Need to check for errors and implement controls and callbacks
 */
LRESULT WINAPI REExtendedRegisterClass( void )
{
    WNDCLASSW wc;
    UINT result;

    FIXME( "semi stub\n" );
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 4;
    wc.hInstance = NULL;
    wc.hIcon = NULL;
    wc.hCursor = NULL;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;

    if (!listbox_registered)
    {
        wc.style = CS_PARENTDC | CS_DBLCLKS | CS_GLOBALCLASS;
        wc.lpfnWndProc = REListWndProc;
        wc.lpszClassName = L"REListBox20W";
        if (RegisterClassW( &wc )) listbox_registered = TRUE;
    }

    if (!combobox_registered)
    {
        wc.style = CS_PARENTDC | CS_DBLCLKS | CS_GLOBALCLASS | CS_VREDRAW | CS_HREDRAW;
        wc.lpfnWndProc = REComboWndProc;
        wc.lpszClassName = L"REComboBox20W";
        if (RegisterClassW( &wc )) combobox_registered = TRUE;
    }

    result = 0;
    if (listbox_registered) result += 1;
    if (combobox_registered) result += 2;

    return result;
}

static BOOL register_classes( HINSTANCE instance )
{
    WNDCLASSW wcW;
    WNDCLASSA wcA;

    wcW.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
    wcW.lpfnWndProc = RichEditWndProcW;
    wcW.cbClsExtra = 0;
    wcW.cbWndExtra = sizeof(ME_TextEditor *);
    wcW.hInstance = NULL; /* hInstance would register DLL-local class */
    wcW.hIcon = NULL;
    wcW.hCursor = LoadCursorW( NULL, (LPWSTR)IDC_IBEAM );
    wcW.hbrBackground = GetStockObject( NULL_BRUSH );
    wcW.lpszMenuName = NULL;

    if (!(GetVersion() & 0x80000000))
    {
        wcW.lpszClassName = RICHEDIT_CLASS20W;
        if (!RegisterClassW( &wcW )) return FALSE;
        wcW.lpszClassName = MSFTEDIT_CLASS;
        if (!RegisterClassW( &wcW )) return FALSE;
    }
    else
    {
        /* WNDCLASSA/W have the same layout */
        wcW.lpszClassName = (LPCWSTR)"RichEdit20W";
        if (!RegisterClassA( (WNDCLASSA *)&wcW )) return FALSE;
        wcW.lpszClassName = (LPCWSTR)"RichEdit50W";
        if (!RegisterClassA( (WNDCLASSA *)&wcW )) return FALSE;
    }

    wcA.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
    wcA.lpfnWndProc = RichEditWndProcA;
    wcA.cbClsExtra = 0;
    wcA.cbWndExtra = sizeof(ME_TextEditor *);
    wcA.hInstance = NULL; /* hInstance would register DLL-local class */
    wcA.hIcon = NULL;
    wcA.hCursor = LoadCursorW( NULL, (LPWSTR)IDC_IBEAM );
    wcA.hbrBackground = GetStockObject(NULL_BRUSH);
    wcA.lpszMenuName = NULL;
    wcA.lpszClassName = RICHEDIT_CLASS20A;
    if (!RegisterClassA( &wcA )) return FALSE;
    wcA.lpszClassName = "RichEdit50A";
    if (!RegisterClassA( &wcA )) return FALSE;

    return TRUE;
}

BOOL WINAPI DllMain( HINSTANCE instance, DWORD reason, void *reserved )
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls( instance );
        me_heap = HeapCreate( 0, 0x10000, 0 );
        if (!register_classes( instance )) return FALSE;
        cursor_reverse = LoadCursorW( instance, MAKEINTRESOURCEW( OCR_REVERSE ) );
        LookupInit();
        break;

    case DLL_PROCESS_DETACH:
        if (reserved) break;
        UnregisterClassW( RICHEDIT_CLASS20W, 0 );
        UnregisterClassW( MSFTEDIT_CLASS, 0 );
        UnregisterClassA( RICHEDIT_CLASS20A, 0 );
        UnregisterClassA( "RichEdit50A", 0 );
        if (listbox_registered) UnregisterClassW( L"REListBox20W", 0 );
        if (combobox_registered) UnregisterClassW( L"REComboBox20W", 0 );
        LookupCleanup();
        HeapDestroy( me_heap );
        release_typelib();
        break;
    }
    return TRUE;
}
