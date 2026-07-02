#include "pch.h"
#include "framework.h"
#include "DashboardToolbar.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace Theme;

IMPLEMENT_DYNAMIC(CDashboardToolbar, CDockablePane)

BEGIN_MESSAGE_MAP(CDashboardToolbar, CDockablePane)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_MESSAGE(Theme::WM_MODEL_LOADED, &CDashboardToolbar::OnModelLoaded)
    ON_CONTROL_RANGE(BN_CLICKED, ID_CHAN_ORIGIN, ID_CHAN_BLUE, &CDashboardToolbar::OnChannelClicked)
    ON_BN_CLICKED(ID_DASH_APPLY,  &CDashboardToolbar::OnApplyClicked)
END_MESSAGE_MAP()

namespace
{
    constexpr int kMargin   = 8;
    constexpr int kRadioW   = 60;
    constexpr int kBtnW     = 88;
    constexpr int kLabelW   = 28;
    constexpr int kEditW    = 70;
    constexpr int kApplyW   = 56;
    constexpr int kModelW   = 160;
    constexpr int kCtrlH    = 22;
}

int CDashboardToolbar::OnCreate(LPCREATESTRUCT lpcs)
{
    if (CDockablePane::OnCreate(lpcs) == -1) return -1;

    m_font.CreateFont(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

    DWORD radioStyle = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON;
    m_radioOrigin.Create(_T("Origin"), radioStyle | WS_GROUP, CRect(0, 0, 0, 0), this, ID_CHAN_ORIGIN);
    m_radioRed.Create   (_T("Red"),    radioStyle,            CRect(0, 0, 0, 0), this, ID_CHAN_RED);
    m_radioGreen.Create (_T("Green"),  radioStyle,            CRect(0, 0, 0, 0), this, ID_CHAN_GREEN);
    m_radioBlue.Create  (_T("Blue"),   radioStyle,            CRect(0, 0, 0, 0), this, ID_CHAN_BLUE);
    m_radioOrigin.SetCheck(BST_CHECKED);

    DWORD btnStyle = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
    m_btnSave.Create       (_T("Save"),        btnStyle, CRect(0, 0, 0, 0), this, ID_DASH_SAVE);
    m_btnDisplayInit.Create(_T("Display Init"),btnStyle, CRect(0, 0, 0, 0), this, ID_DASH_DISPINIT);
    m_btnApply.Create      (_T("Apply"),       btnStyle, CRect(0, 0, 0, 0), this, ID_DASH_APPLY);

    m_lblLot.Create(_T("LOT"), WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, CRect(0, 0, 0, 0), this);
    m_lblId.Create (_T("ID"),  WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, CRect(0, 0, 0, 0), this);
    m_lblModel.Create(_T("model"), WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, CRect(0, 0, 0, 0), this);

    m_editLot.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(0, 0, 0, 0), this, ID_DASH_LOT_EDIT);
    m_editId.Create (WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(0, 0, 0, 0), this, ID_DASH_ID_EDIT);

    m_staticModel.Create(_T(""), WS_CHILD | WS_VISIBLE | WS_BORDER | SS_CENTERIMAGE | SS_LEFTNOWORDWRAP,
        CRect(0, 0, 0, 0), this);

    for (CWnd* w : { (CWnd*)&m_radioOrigin, (CWnd*)&m_radioRed, (CWnd*)&m_radioGreen, (CWnd*)&m_radioBlue,
                     (CWnd*)&m_btnSave, (CWnd*)&m_btnDisplayInit, (CWnd*)&m_btnApply,
                     (CWnd*)&m_lblLot, (CWnd*)&m_lblId, (CWnd*)&m_lblModel,
                     (CWnd*)&m_editLot, (CWnd*)&m_editId, (CWnd*)&m_staticModel })
    {
        w->SetFont(&m_font);
    }

    return 0;
}

void CDashboardToolbar::OnSize(UINT nType, int cx, int cy)
{
    CDockablePane::OnSize(nType, cx, cy);
    if (!m_radioOrigin.m_hWnd) return;

    int y = (cy - kCtrlH) / 2;
    int x = kMargin;

    auto place = [&](CWnd& w, int width)
    {
        w.MoveWindow(x, y, width, kCtrlH);
        x += width + 6;
    };

    place(m_radioOrigin, kRadioW);
    place(m_radioRed,    kRadioW);
    place(m_radioGreen,  kRadioW);
    place(m_radioBlue,   kRadioW);

    x += 10; // separator gap
    place(m_btnSave,        kBtnW);
    place(m_btnDisplayInit, kBtnW + 20);

    x += 16; // separator gap
    place(m_lblLot, kLabelW);
    place(m_editLot, kEditW);
    place(m_lblId, kLabelW);
    place(m_editId, kEditW);
    place(m_btnApply, kApplyW);

    // Model box anchored to the right edge
    CRect rcModel(cx - kMargin - kModelW, y, cx - kMargin, y + kCtrlH);
    m_staticModel.MoveWindow(&rcModel);
    CRect rcModelLbl(rcModel.left - kLabelW - 6, y, rcModel.left - 6, y + kCtrlH);
    m_lblModel.MoveWindow(&rcModelLbl);

    PostMessage(WM_NCPAINT, 1, 0);
}

BOOL CDashboardToolbar::OnEraseBkgnd(CDC* pDC)
{
    CRect rc; GetClientRect(&rc);
    pDC->FillSolidRect(&rc, TOP_BG);
    pDC->FillSolidRect(0, rc.bottom - 1, rc.Width(), 1, SEPARATOR);
    return TRUE;
}

void CDashboardToolbar::OnChannelClicked(UINT nID)
{
    switch (nID)
    {
    case ID_CHAN_ORIGIN: m_activeChannel = LightChannel::White; break;
    case ID_CHAN_RED:    m_activeChannel = LightChannel::Red;   break;
    case ID_CHAN_GREEN:  m_activeChannel = LightChannel::Green; break;
    case ID_CHAN_BLUE:   m_activeChannel = LightChannel::Blue;  break;
    }
}

void CDashboardToolbar::OnApplyClicked()
{
    // Placeholder — no L3 pipeline exists yet to apply LOT/ID against.
    CString lot, id;
    m_editLot.GetWindowText(lot);
    m_editId.GetWindowText(id);
}

LRESULT CDashboardToolbar::OnModelLoaded(WPARAM /*wParam*/, LPARAM lParam)
{
    LPCTSTR name = reinterpret_cast<LPCTSTR>(lParam);
    m_staticModel.SetWindowText(name ? name : _T(""));
    return 0;
}
