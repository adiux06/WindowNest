/*
 * WindowHider.cpp
 * Compile (MinGW 32 or 64-bit, any version):
 *   g++ WindowHider.cpp -o WindowHider.exe -mwindows -lcomctl32 -lgdi32 -luser32
 *
 * Compatible: Windows XP SP3 through Windows 11, MinGW / MSVC.
 */

#ifndef WINVER
#  define WINVER 0x0501
#endif
#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0501
#endif
#ifndef _WIN32_IE
#  define _WIN32_IE 0x0600
#endif

#include <windows.h>
#include <commctrl.h>
#include <string.h>
#include <stdio.h>

#ifndef TRACKBAR_CLASS
#  define TRACKBAR_CLASS "msctls_trackbar32"
#endif
#ifndef TBS_HORZ
#  define TBS_HORZ    0x0000
#endif
#ifndef TBS_NOTICKS
#  define TBS_NOTICKS 0x0010
#endif
#ifndef TBM_SETRANGE
#  define TBM_SETRANGE (WM_USER+6)
#endif
#ifndef TBM_SETPOS
#  define TBM_SETPOS   (WM_USER+5)
#endif
#ifndef TBM_GETPOS
#  define TBM_GETPOS   (WM_USER+0)
#endif
#ifndef WDA_EXCLUDEFROMCAPTURE
#  define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#ifndef FW_SEMIBOLD
#  define FW_SEMIBOLD 600
#endif

typedef BOOL (WINAPI *FnSetAffinity)(HWND, DWORD);
typedef BOOL (WINAPI *FnDPIAware)(void);

static FnSetAffinity g_pfnAffinity = NULL;

#define CLR_BG       RGB(15,  17,  23)
#define CLR_PANEL    RGB(24,  27,  36)
#define CLR_BORDER   RGB(45,  50,  65)
#define CLR_ACCENT   RGB(88, 166, 255)
#define CLR_DANGER   RGB(255,  85,  85)
#define CLR_TEXT     RGB(220, 225, 240)
#define CLR_MUTED    RGB(110, 120, 145)
#define CLR_BTN_NORM RGB(35,  40,  55)
#define CLR_BTN_HOV  RGB(55,  62,  80)

#define TOOLBAR_H      48
#define BTN_W          118
#define BTN_H          32
#define HK_BYPASS      1
#define HK_HIDE        2
#define ID_BTN_PICK    1
#define ID_BTN_RELEASE 2
#define ID_LBL_OPACITY 3
#define ID_SLIDER      4
#define ID_BTN_BYPASS  5
#define ID_BTN_HELP    6
#define ID_LBL_STATUS  7

static HWND  g_hwndMain      = NULL;
static HWND  g_hwndTarget    = NULL;
static HWND  g_hwndSlider    = NULL;
static HWND  g_hwndOpacity   = NULL;
static HWND  g_hwndStatus    = NULL;
static HWND  g_hwndBtnBypass = NULL;

static RECT  g_rcOrig        = {0,0,0,0};
static LONG  g_styleOrig     = 0;
static LONG  g_exStyleOrig   = 0;
static int   g_bypassMode    = 0;
static int   g_hidden        = 0;

static HFONT  g_fontUI    = NULL;
static HFONT  g_fontTitle = NULL;
static HFONT  g_fontMono  = NULL;
static HBRUSH g_brPanel   = NULL;
static HBRUSH g_brBg      = NULL;
static HBRUSH g_brStatic  = NULL;

static int g_btnHover[4] = {0,0,0,0};

LRESULT CALLBACK WndProc    (HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK HelpWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK BtnSubProc (HWND, UINT, WPARAM, LPARAM);
static WNDPROC g_origBtnProc = NULL;

/* ── Fonts ──────────────────────────────────────────────────────────────── */
static HFONT MakeFont(int h, int w, int pitch, const char* face) {
    return CreateFontA(h,0,0,0,w,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, (BYTE)(pitch|FF_DONTCARE), face);
}

static void CreateFonts(void) {
    g_fontUI    = MakeFont(-13, FW_NORMAL,   DEFAULT_PITCH, "Segoe UI");
    if (!g_fontUI)
        g_fontUI = MakeFont(-13, FW_NORMAL,  DEFAULT_PITCH, "Tahoma");

    g_fontTitle = MakeFont(-14, FW_SEMIBOLD, DEFAULT_PITCH, "Segoe UI");
    if (!g_fontTitle)
        g_fontTitle = MakeFont(-14, FW_BOLD, DEFAULT_PITCH, "Tahoma");

    g_fontMono  = MakeFont(-12, FW_NORMAL, FIXED_PITCH, "Cascadia Mono");
    if (!g_fontMono)
        g_fontMono = MakeFont(-12, FW_NORMAL, FIXED_PITCH, "Consolas");
    if (!g_fontMono)
        g_fontMono = MakeFont(-12, FW_NORMAL, FIXED_PITCH, "Courier New");
}

/* ── Draw button ─────────────────────────────────────────────────────────── */
static void DrawButton(HDC hdc, RECT rc, const char* text, int hover, int active, int danger) {
    COLORREF face, border, txtClr;
    HBRUSH br; HPEN pn;

    if      (active && danger) { face = CLR_DANGER;   border = CLR_DANGER; }
    else if (active)           { face = CLR_ACCENT;   border = CLR_ACCENT; }
    else if (hover)            { face = CLR_BTN_HOV;  border = CLR_BORDER; }
    else                       { face = CLR_BTN_NORM; border = CLR_BORDER; }
    txtClr = (active && !danger) ? RGB(10,10,20) : CLR_TEXT;

    br = CreateSolidBrush(face);
    pn = CreatePen(PS_SOLID, 1, border);
    SelectObject(hdc, br);
    SelectObject(hdc, pn);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
    DeleteObject(br); DeleteObject(pn);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, txtClr);
    if (g_fontUI) SelectObject(hdc, g_fontUI);
    DrawTextA(hdc, text, -1, &rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
}

/* ── Draw toolbar ────────────────────────────────────────────────────────── */
static void DrawToolbar(HWND hwnd, HDC hdc) {
    RECT rc = {0,0,0,0}, titleRc;
    HBRUSH br; HPEN pn; HBRUSH acBr;

    GetClientRect(hwnd, &rc); rc.bottom = TOOLBAR_H;
    br = CreateSolidBrush(CLR_PANEL); FillRect(hdc,&rc,br); DeleteObject(br);
    pn = CreatePen(PS_SOLID,1,CLR_BORDER); SelectObject(hdc,pn);
    MoveToEx(hdc,0,TOOLBAR_H-1,NULL); LineTo(hdc,rc.right,TOOLBAR_H-1);
    DeleteObject(pn);
    acBr = CreateSolidBrush(CLR_ACCENT);
    SelectObject(hdc,acBr); SelectObject(hdc,GetStockObject(NULL_PEN));
    Ellipse(hdc,12,16,22,26); DeleteObject(acBr);
    SetBkMode(hdc,TRANSPARENT); SetTextColor(hdc,CLR_TEXT);
    if (g_fontTitle) SelectObject(hdc,g_fontTitle);
    titleRc.left=28; titleRc.top=0; titleRc.right=200; titleRc.bottom=TOOLBAR_H;
    DrawTextA(hdc,"Window Hider",-1,&titleRc,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    if (g_bypassMode) {
        RECT badge = {0,0,5,TOOLBAR_H};
        HBRUSH db = CreateSolidBrush(CLR_DANGER);
        FillRect(hdc,&badge,db); DeleteObject(db);
    }
}

/* ── Opacity ─────────────────────────────────────────────────────────────── */
static void SetMainOpacity(int pct) {
    char buf[32];
    SetLayeredWindowAttributes(g_hwndMain, 0, (BYTE)(pct*255/100), LWA_ALPHA);
    wsprintfA(buf,"Opacity %d%%",pct);
    SetWindowTextA(g_hwndOpacity,buf);
}

static void FitTarget(void) {
    RECT rc = {0,0,0,0};
    if (!g_hwndTarget) return;
    GetClientRect(g_hwndMain,&rc);
    SetWindowPos(g_hwndTarget,HWND_TOP,0,TOOLBAR_H,rc.right,rc.bottom-TOOLBAR_H,
        SWP_NOACTIVATE|SWP_SHOWWINDOW|SWP_FRAMECHANGED);
    SendMessage(g_hwndTarget,WM_SIZE,SIZE_RESTORED,
        MAKELPARAM(rc.right,rc.bottom-TOOLBAR_H));
}

/* ── Bypass ──────────────────────────────────────────────────────────────── */
static void EnableBypass(void) {
    g_bypassMode=1;
    SetWindowPos(g_hwndMain,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
    SetLayeredWindowAttributes(g_hwndMain,0,200,LWA_ALPHA);
    if (g_hwndSlider)  SendMessage(g_hwndSlider,TBM_SETPOS,TRUE,78);
    if (g_hwndOpacity) SetWindowTextA(g_hwndOpacity,"Opacity 78%");
    if (g_hwndStatus)  SetWindowTextA(g_hwndStatus,"[BYPASS ON]  Ctrl+Shift+B = off");
    InvalidateRect(g_hwndMain,NULL,FALSE);
    InvalidateRect(g_hwndBtnBypass,NULL,FALSE);
    if (g_hwndTarget) { SetForegroundWindow(g_hwndMain); SetFocus(g_hwndTarget); }
}

static void DisableBypass(void) {
    g_bypassMode=0;
    SetWindowPos(g_hwndMain,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
    SetLayeredWindowAttributes(g_hwndMain,0,255,LWA_ALPHA);
    if (g_hwndSlider)  SendMessage(g_hwndSlider,TBM_SETPOS,TRUE,100);
    if (g_hwndOpacity) SetWindowTextA(g_hwndOpacity,"Opacity 100%");
    if (g_hwndStatus)  SetWindowTextA(g_hwndStatus,"BYPASS OFF  Ctrl+Shift+B = on");
    InvalidateRect(g_hwndMain,NULL,FALSE);
    InvalidateRect(g_hwndBtnBypass,NULL,FALSE);
}

/* ── Capture / Release ───────────────────────────────────────────────────── */
static void CaptureWindow(HWND target) {
    char cls[64], title[128], status[220];
    LONG style, ex;
    if (!target || target==g_hwndMain) return;
    GetClassNameA(target,cls,sizeof(cls));
    if (strcmp(cls,"HiddenAppClass")==0) return;

    g_hwndTarget  = target;
    GetWindowRect(target,&g_rcOrig);
    g_styleOrig   = GetWindowLong(target,GWL_STYLE);
    g_exStyleOrig = GetWindowLong(target,GWL_EXSTYLE);

    style  = g_styleOrig;
    style &= ~(WS_CAPTION|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_SYSMENU|WS_POPUP);
    style |= WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS;
    SetWindowLong(target,GWL_STYLE,style);

    ex  = g_exStyleOrig;
    ex &= ~WS_EX_APPWINDOW;
    ex |= WS_EX_TOOLWINDOW;
    SetWindowLong(target,GWL_EXSTYLE,ex);
    SetWindowPos(target,NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
    SetParent(target,g_hwndMain);
    FitTarget();
    if (g_pfnAffinity) g_pfnAffinity(target,WDA_EXCLUDEFROMCAPTURE);
    SetForegroundWindow(g_hwndMain);
    SetFocus(g_hwndTarget);
    GetWindowTextA(target,title,sizeof(title));
    if (!title[0]) lstrcpyA(title,"(untitled)");
    wsprintfA(status,"Captured: %.110s",title);
    SetWindowTextA(g_hwndStatus,status);
}

static void ReleaseWindow(void) {
    if (!g_hwndTarget) return;
    SetParent(g_hwndTarget,NULL);
    SetWindowLong(g_hwndTarget,GWL_STYLE,g_styleOrig);
    SetWindowLong(g_hwndTarget,GWL_EXSTYLE,g_exStyleOrig);
    if (g_pfnAffinity) g_pfnAffinity(g_hwndTarget,0);
    SetWindowPos(g_hwndTarget,HWND_TOP,
        g_rcOrig.left,g_rcOrig.top,
        g_rcOrig.right-g_rcOrig.left,g_rcOrig.bottom-g_rcOrig.top,
        SWP_SHOWWINDOW|SWP_FRAMECHANGED);
    g_hwndTarget=NULL;
    SetWindowTextA(g_hwndStatus,"No window captured");
    InvalidateRect(g_hwndMain,NULL,TRUE);
}

/* ── Button subclass ─────────────────────────────────────────────────────── */
LRESULT CALLBACK BtnSubProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    int id  = GetDlgCtrlID(hwnd);
    int idx = (id==ID_BTN_PICK)?0:(id==ID_BTN_RELEASE)?1:(id==ID_BTN_BYPASS)?2:3;

    switch (msg) {
    case WM_MOUSEMOVE:
        if (!g_btnHover[idx]) {
            TRACKMOUSEEVENT tme;
            g_btnHover[idx]=1;
            InvalidateRect(hwnd,NULL,FALSE);
            tme.cbSize=sizeof(tme); tme.dwFlags=TME_LEAVE;
            tme.hwndTrack=hwnd; tme.dwHoverTime=0;
            TrackMouseEvent(&tme);
        }
        break;
    case WM_MOUSELEAVE:
        g_btnHover[idx]=0; InvalidateRect(hwnd,NULL,FALSE); break;
    case WM_PAINT: {
        PAINTSTRUCT ps; RECT rc; char text[64];
        int active,danger;
        HDC hdc=BeginPaint(hwnd,&ps);
        GetClientRect(hwnd,&rc);
        GetWindowTextA(hwnd,text,sizeof(text));
        active=(idx==2&&g_bypassMode)?1:0; danger=active;
        DrawButton(hdc,rc,text,g_btnHover[idx],active,danger);
        EndPaint(hwnd,&ps); return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_KEYDOWN:
        if (wp==VK_SPACE||wp==VK_RETURN) {
            if (g_hwndTarget) SetFocus(g_hwndTarget); return 0;
        }
        break;
    }
    return CallWindowProc(g_origBtnProc,hwnd,msg,wp,lp);
}

/* ── Help window ─────────────────────────────────────────────────────────── */
static const char* HELP_TEXT =
    "WINDOW HIDER  v1.0\r\n"
    "=========================================\r\n"
    "\r\n"
    "ABOUT\r\n"
    "  Embeds any external window inside this frame,\r\n"
    "  hides it from screen-capture / proctoring tools,\r\n"
    "  and lets you overlay it semi-transparently.\r\n"
    "\r\n"
    "FEATURES\r\n"
    "  - Capture any running window into this frame\r\n"
    "  - WDA_EXCLUDEFROMCAPTURE (hidden from OBS,\r\n"
    "    screenshots, proctoring software)\r\n"
    "  - Bypass Mode: always-on-top + semi-transparent\r\n"
    "  - Opacity slider 10%% to 100%%\r\n"
    "  - Hotkeys to hide/restore and toggle bypass\r\n"
    "\r\n"
    "KEYBOARD SHORTCUTS\r\n"
    "  Ctrl+Shift+B   Toggle Bypass Mode on / off\r\n"
    "  Ctrl+Shift+H   Hide / show this window\r\n"
    "\r\n"
    "HOW TO USE\r\n"
    "  1. Click [Pick Window].\r\n"
    "  2. Click OK on the prompt, hover your cursor\r\n"
    "     over the window you want to capture.\r\n"
    "  3. After 3 seconds it is embedded here.\r\n"
    "  4. Use Bypass Mode to float on top of\r\n"
    "     fullscreen apps at reduced opacity.\r\n"
    "  5. Click [Release] to restore the target.\r\n"
    "\r\n"
    "NOTES\r\n"
    "  - Some UWP/sandboxed apps refuse re-parenting.\r\n"
    "  - Capture exclusion requires Windows 10 2004+.\r\n"
    "  - DPI-awareness is applied automatically.\r\n"
    "=========================================\r\n";

LRESULT CALLBACK HelpWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HBRUSH brBg = NULL;
    static HWND hEdit  = NULL;

    switch (msg) {
    case WM_CREATE: {
        HINSTANCE hInst = (HINSTANCE)GetWindowLong(hwnd,GWL_HINSTANCE);
        brBg = CreateSolidBrush(CLR_BG);
        hEdit = CreateWindowExA(0,"EDIT","",
            WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,
            10,10,520,430, hwnd,(HMENU)100,hInst,NULL);
        if (g_fontMono) SendMessage(hEdit,WM_SETFONT,(WPARAM)g_fontMono,TRUE);
        SetWindowTextA(hEdit,HELP_TEXT);
        {
            HWND hBtn = CreateWindowExA(0,"BUTTON","Close",
                WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
                225,455,100,30, hwnd,(HMENU)IDOK,hInst,NULL);
            if (g_fontUI) SendMessage(hBtn,WM_SETFONT,(WPARAM)g_fontUI,TRUE);
        }
        break;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
        SetBkColor((HDC)wp,CLR_PANEL);
        SetTextColor((HDC)wp,CLR_TEXT);
        return (LRESULT)(brBg ? brBg : (HBRUSH)GetStockObject(BLACK_BRUSH));
    case WM_ERASEBKGND: {
        RECT rc={0,0,0,0}; GetClientRect(hwnd,&rc);
        if (brBg) FillRect((HDC)wp,&rc,brBg);
        return 1;
    }
    case WM_PAINT: { PAINTSTRUCT ps; BeginPaint(hwnd,&ps); EndPaint(hwnd,&ps); return 0; }
    case WM_COMMAND:
        if (LOWORD(wp)==IDOK) DestroyWindow(hwnd); break;
    case WM_DESTROY:
        if (brBg) { DeleteObject(brBg); brBg=NULL; } hEdit=NULL; break;
    }
    return DefWindowProc(hwnd,msg,wp,lp);
}

static void OpenHelp(HWND parent) {
    WNDCLASSA wch;
    HWND ex, helpWnd;
    RECT pr={0,0,0,0}, hr={0,0,0,0};
    int cx,cy;
    HINSTANCE hInst=(HINSTANCE)GetWindowLong(parent,GWL_HINSTANCE);

    ex = FindWindowA("HiddenAppHelp",NULL);
    if (ex) { SetForegroundWindow(ex); return; }

    ZeroMemory(&wch,sizeof(wch));
    wch.lpfnWndProc=HelpWndProc; wch.hInstance=hInst;
    wch.lpszClassName="HiddenAppHelp";
    wch.hbrBackground=CreateSolidBrush(CLR_BG);
    wch.hCursor=LoadCursor(NULL,IDC_ARROW);
    RegisterClassA(&wch);

    helpWnd=CreateWindowExA(WS_EX_DLGMODALFRAME,"HiddenAppHelp",
        "Window Hider - Help & Info",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,
        0,0,560,510, parent,NULL,hInst,NULL);
    GetWindowRect(parent,&pr); GetWindowRect(helpWnd,&hr);
    cx=pr.left+(pr.right-pr.left)/2-(hr.right-hr.left)/2;
    cy=pr.top +(pr.bottom-pr.top)/2-(hr.bottom-hr.top)/2;
    SetWindowPos(helpWnd,HWND_TOP,cx,cy,0,0,SWP_NOSIZE);
}

/* ── Main WndProc ────────────────────────────────────────────────────────── */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_CREATE: {
        int bx=170, by=(TOOLBAR_H-BTN_H)/2;
        HWND b;
        HINSTANCE hInst=(HINSTANCE)GetWindowLong(hwnd,GWL_HINSTANCE);

        b=CreateWindowExA(0,"BUTTON","Pick Window",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            bx,by,BTN_W,BTN_H,hwnd,(HMENU)ID_BTN_PICK,hInst,NULL);
        g_origBtnProc=(WNDPROC)(LONG_PTR)SetWindowLong(b,GWL_WNDPROC,(LONG)(LONG_PTR)BtnSubProc);

        bx+=BTN_W+8;
        b=CreateWindowExA(0,"BUTTON","Release",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            bx,by,BTN_W,BTN_H,hwnd,(HMENU)ID_BTN_RELEASE,hInst,NULL);
        SetWindowLong(b,GWL_WNDPROC,(LONG)(LONG_PTR)BtnSubProc);

        bx+=BTN_W+8;
        g_hwndBtnBypass=CreateWindowExA(0,"BUTTON","Bypass Mode",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            bx,by,BTN_W,BTN_H,hwnd,(HMENU)ID_BTN_BYPASS,hInst,NULL);
        SetWindowLong(g_hwndBtnBypass,GWL_WNDPROC,(LONG)(LONG_PTR)BtnSubProc);

        bx+=BTN_W+8;
        b=CreateWindowExA(0,"BUTTON","? Help",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            bx,by,72,BTN_H,hwnd,(HMENU)ID_BTN_HELP,hInst,NULL);
        SetWindowLong(b,GWL_WNDPROC,(LONG)(LONG_PTR)BtnSubProc);

        bx+=82;
        g_hwndOpacity=CreateWindowExA(0,"STATIC","Opacity 100%",WS_CHILD|WS_VISIBLE|SS_LEFT,
            bx,by+8,90,18,hwnd,(HMENU)ID_LBL_OPACITY,hInst,NULL);
        SendMessage(g_hwndOpacity,WM_SETFONT,(WPARAM)g_fontUI,TRUE);

        bx+=96;
        g_hwndSlider=CreateWindowExA(0,TRACKBAR_CLASS,NULL,
            WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_NOTICKS,
            bx,by+2,160,28,hwnd,(HMENU)ID_SLIDER,hInst,NULL);
        SendMessage(g_hwndSlider,TBM_SETRANGE,TRUE,MAKELPARAM(10,100));
        SendMessage(g_hwndSlider,TBM_SETPOS,TRUE,100);

        bx+=168;
        g_hwndStatus=CreateWindowExA(0,"STATIC","No window captured",WS_CHILD|WS_VISIBLE|SS_LEFT,
            bx,by+8,300,18,hwnd,(HMENU)ID_LBL_STATUS,hInst,NULL);
        SendMessage(g_hwndStatus,WM_SETFONT,(WPARAM)g_fontUI,TRUE);

        RegisterHotKey(hwnd,HK_BYPASS,MOD_CONTROL|MOD_SHIFT,'B');
        RegisterHotKey(hwnd,HK_HIDE,  MOD_CONTROL|MOD_SHIFT,'H');
        break;
    }

    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)wp,TRANSPARENT);
        SetTextColor((HDC)wp, g_bypassMode ? CLR_DANGER : CLR_MUTED);
        if (!g_brStatic) g_brStatic=CreateSolidBrush(CLR_PANEL);
        return (LRESULT)g_brStatic;

    case WM_ERASEBKGND: {
        RECT rc={0,0,0,0}, strip, content;
        HBRUSH bgbr;
        GetClientRect(hwnd,&rc);
        strip=rc;   strip.bottom=TOOLBAR_H;
        content=rc; content.top=TOOLBAR_H;
        if (g_brPanel) FillRect((HDC)wp,&strip,g_brPanel);
        bgbr=CreateSolidBrush(CLR_BG);
        FillRect((HDC)wp,&content,bgbr); DeleteObject(bgbr);
        return 1;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc=BeginPaint(hwnd,&ps);
        DrawToolbar(hwnd,hdc);
        if (!g_hwndTarget) {
            RECT rc={0,0,0,0}, msgRc;
            GetClientRect(hwnd,&rc);
            msgRc.left=0; msgRc.top=TOOLBAR_H; msgRc.right=rc.right; msgRc.bottom=rc.bottom;
            SetBkMode(hdc,TRANSPARENT); SetTextColor(hdc,CLR_MUTED);
            if (g_fontUI) SelectObject(hdc,g_fontUI);
            DrawTextA(hdc,"No window captured.\n\nClick  [Pick Window]  to embed a window.",-1,
                &msgRc,DT_CENTER|DT_VCENTER|DT_WORDBREAK);
        }
        EndPaint(hwnd,&ps); return 0;
    }

    case WM_HOTKEY:
        if ((int)wp==HK_BYPASS) { if(g_bypassMode) DisableBypass(); else EnableBypass(); }
        if ((int)wp==HK_HIDE) {
            if (!g_hidden) { ShowWindow(hwnd,SW_HIDE); g_hidden=1; }
            else { ShowWindow(hwnd,SW_SHOW); SetForegroundWindow(hwnd);
                   if (g_hwndTarget) SetFocus(g_hwndTarget); g_hidden=0; }
        }
        break;

    case WM_COMMAND: {
        int id=LOWORD(wp);
        if (id==ID_BTN_PICK) {
            POINT pt={0,0}; HWND target;
            MessageBoxA(hwnd,
                "Click OK, then hover your cursor over the target window.\n"
                "It will be captured after 3 seconds.",
                "Pick Window",MB_OK|MB_ICONINFORMATION);
            Sleep(3000);
            GetCursorPos(&pt);
            target=WindowFromPoint(pt);
            while (GetParent(target)) target=GetParent(target);
            CaptureWindow(target);
        }
        if (id==ID_BTN_RELEASE) ReleaseWindow();
        if (id==ID_BTN_BYPASS) {
            if (g_bypassMode) DisableBypass(); else EnableBypass();
            if (g_hwndTarget) SetFocus(g_hwndTarget);
        }
        if (id==ID_BTN_HELP) OpenHelp(hwnd);
        break;
    }

    case WM_HSCROLL:
        if ((HWND)lp==g_hwndSlider) {
            int pos=(int)SendMessage(g_hwndSlider,TBM_GETPOS,0,0);
            SetMainOpacity(pos);
            if (g_hwndTarget) SetFocus(g_hwndTarget);
        }
        break;

    case WM_LBUTTONDOWN: if (g_hwndTarget) SetFocus(g_hwndTarget); break;
    case WM_SIZE: FitTarget(); InvalidateRect(hwnd,NULL,TRUE); break;
    case WM_KEYDOWN:
    case WM_CHAR:
        if (g_hwndTarget) { SetFocus(g_hwndTarget); PostMessage(g_hwndTarget,msg,wp,lp); }
        break;
    case WM_SETFOCUS: if (g_hwndTarget) SetFocus(g_hwndTarget); break;

    case WM_DESTROY:
        UnregisterHotKey(hwnd,HK_BYPASS);
        UnregisterHotKey(hwnd,HK_HIDE);
        ReleaseWindow();
        if (g_fontUI)    { DeleteObject(g_fontUI);    g_fontUI=NULL; }
        if (g_fontTitle) { DeleteObject(g_fontTitle); g_fontTitle=NULL; }
        if (g_fontMono)  { DeleteObject(g_fontMono);  g_fontMono=NULL; }
        if (g_brPanel)   { DeleteObject(g_brPanel);   g_brPanel=NULL; }
        if (g_brBg)      { DeleteObject(g_brBg);      g_brBg=NULL; }
        if (g_brStatic)  { DeleteObject(g_brStatic);  g_brStatic=NULL; }
        PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd,msg,wp,lp);
}

/* ── WinMain ─────────────────────────────────────────────────────────────── */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    WNDCLASSA wc;
    HMODULE   hUser;
    FnDPIAware pDPI;
    MSG msg;

    (void)hPrev; (void)lpCmd; (void)nShow;

    InitCommonControls();  /* no struct — works on all Windows / MinGW versions */

    hUser = GetModuleHandleA("user32.dll");
    pDPI  = (FnDPIAware)GetProcAddress(hUser,"SetProcessDPIAware");
    if (pDPI) pDPI();
    g_pfnAffinity = (FnSetAffinity)GetProcAddress(hUser,"SetWindowDisplayAffinity");

    CreateFonts();
    g_brPanel = CreateSolidBrush(CLR_PANEL);
    g_brBg    = CreateSolidBrush(CLR_BG);

    ZeroMemory(&wc,sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = "HiddenAppClass";
    wc.hCursor       = LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground = g_brPanel;
    wc.style         = CS_HREDRAW|CS_VREDRAW;
    RegisterClassA(&wc);

    g_hwndMain = CreateWindowExA(WS_EX_LAYERED,
        "HiddenAppClass","Window Hider",
        WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_VISIBLE,
        80,60,1400,880, NULL,NULL,hInstance,NULL);

    SetLayeredWindowAttributes(g_hwndMain,0,255,LWA_ALPHA);
    if (g_pfnAffinity) g_pfnAffinity(g_hwndMain,WDA_EXCLUDEFROMCAPTURE);

    while (GetMessage(&msg,NULL,0,0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
