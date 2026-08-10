#include "scene/SelectServerScene.h"

#include "ui/SControl.h"
#include "ui/SControls.h"
#include "ui/SControlContainer.h"

namespace tmx {

using namespace SelectServerID;

bool SelectServerScene::Init(SControlContainer* container) {
    if (!container)
        return false;
    m_container = container;

    m_pServerPanel     = (SPanel*)container->FindControl(P_SERVER_SEL);
    m_pServerGroupList = (SListBox*)container->FindControl(L_SELECT_SERVERG);
    m_pServerList      = (SListBox*)container->FindControl(L_SELECT_SERVER);
    m_pLoginPanel      = (SPanel*)container->FindControl(P_LOGIN_BOX);
    m_pEditID          = (SEditableText*)container->FindControl(E_LOGIN_ID);
    m_pEditPW          = (SEditableText*)container->FindControl(E_LOGIN_PASSWORD);
    m_pLoginBtns[0]    = (SButton*)container->FindControl(B_LOGIN_OK);
    m_pLoginBtns[1]    = (SButton*)container->FindControl(B_QUIT);
    m_pLoginBtns[2]    = (SButton*)container->FindControl(B_CREATE_ID);
    m_pLogo[0]         = (SPanel*)container->FindControl(TMP_LOGO_PANEL1);
    m_pLogo[1]         = (SPanel*)container->FindControl(TMP_LOGO_PANEL2);
    m_pCopyright       = (SText*)container->FindControl(TMT_SCENE_TEXT);

    // One-time nudges (TMSelectServerScene.cpp:138-152). Kept out of Layout
    // so re-layouts on resize don't accumulate offsets.
    if (SText* t = (SText*)container->FindControl(TMT_SEL_SERVER_TEXT))
        t->m_nPosY += 5.0f;
    if (SText* t = (SText*)container->FindControl(T_LOGIN_BOX_TEXT))
        t->m_nPosY += 5.0f;

    return m_pServerPanel && m_pLoginPanel;
}

void SelectServerScene::Layout(int screenW, int screenH) {
    const float w = (float)screenW;
    const float h = (float)screenH;
    const float hRatio = h / 600.0f; // RenderDevice.cpp:65

    // Copyright → bottom (TMSelectServerScene.cpp:155); x centered like the
    // original SetCenterPos (SControl.cpp:228-236) but with the real width.
    if (m_pCopyright) {
        m_pCopyright->m_nPosX = (w - m_pCopyright->m_nWidth) * 0.5f;
        m_pCopyright->m_nPosY = h - 15.0f;
    }

    // Login panel → screen center (TMSelectServerScene.cpp:157-158).
    if (m_pLoginPanel) {
        m_pLoginPanel->m_nPosX = (w - m_pLoginPanel->m_nWidth) * 0.5f;
        m_pLoginPanel->m_nPosY = (h - m_pLoginPanel->m_nHeight) * 0.5f;
    }

    // Logo pair → centered at top as a 512-wide unit (TMSelectServerScene.cpp:171-181).
    if (m_pLogo[0] && m_pLogo[1]) {
        const int addHeight = (screenW == 1024) ? 20 : 0;
        const float y = 10.0f * hRatio + (float)addHeight;
        m_pLogo[0]->m_nPosX = w * 0.5f - m_pLogo[0]->m_nWidth;
        m_pLogo[0]->m_nPosY = y;
        m_pLogo[1]->m_nPosX = w * 0.5f;
        m_pLogo[1]->m_nPosY = y;
    }

    // Server select panel → screen center + 75 down (InitializeUI, TMSelectServerScene.cpp:1353-1357).
    if (m_pServerPanel) {
        m_pServerPanel->m_nPosX = (w - m_pServerPanel->m_nWidth) * 0.5f;
        m_pServerPanel->m_nPosY = (h - m_pServerPanel->m_nHeight) * 0.5f + 75.0f;
    }
}

void SelectServerScene::ApplyBootState() {
    // TMSelectServerScene.cpp:222-232 — login hidden, buttons hidden & untinted.
    m_cLogin = 0;
    for (int i = 0; i < 3; ++i) {
        if (m_pLoginBtns[i]) {
            m_pLoginBtns[i]->SetVisible(0);
            m_pLoginBtns[i]->m_GCPanel.dwColor = 0;
        }
    }
    if (m_pLoginPanel) {
        m_pLoginPanel->SetVisible(0);
        m_pLoginPanel->m_GCPanel.dwColor = 0;
    }

    // InitializeUI (TMSelectServerScene.cpp:1344-1352): server panel visible,
    // channel list hidden until a group is picked.
    if (m_pServerPanel)
        m_pServerPanel->SetVisible(1);
    if (m_pServerList)
        m_pServerList->SetVisible(0);

    if (m_pLogo[0]) m_pLogo[0]->SetVisible(1);
    if (m_pLogo[1]) m_pLogo[1]->SetVisible(1);
}

void SelectServerScene::ShowLogin() {
    // TMSelectServerScene.cpp:607-616. NB: buttons keep their transparent
    // dwColor (0) — their rect is invisible; only the panel gets its alpha
    // restored (SetAlphaLogin, TMSelectServerScene.cpp:1302-1335).
    if (m_pServerPanel)
        m_pServerPanel->SetVisible(0);
    for (int i = 0; i < 3; ++i) {
        if (m_pLoginBtns[i])
            m_pLoginBtns[i]->SetVisible(1);
    }
    if (m_pLoginPanel) {
        m_pLoginPanel->SetVisible(1);
        m_pLoginPanel->m_GCPanel.dwColor = 0xFFFFFFFF;
    }
    if (m_container && m_pEditID)
        m_container->SetFocusedControl(m_pEditID);
    m_cLogin = 1;
}

void SelectServerScene::ShowServerSelect() {
    // TMSelectServerScene.cpp:625-633.
    if (m_pServerPanel)
        m_pServerPanel->SetVisible(1);
    for (int i = 0; i < 3; ++i) {
        if (m_pLoginBtns[i]) {
            m_pLoginBtns[i]->SetVisible(0);
            m_pLoginBtns[i]->m_GCPanel.dwColor = 0;
        }
    }
    if (m_pLoginPanel) {
        m_pLoginPanel->SetVisible(0);
        m_pLoginPanel->m_GCPanel.dwColor = 0;
    }
    if (m_container)
        m_container->SetFocusedControl(nullptr);
    m_cLogin = 0;
}

int SelectServerScene::OnKeyDown(int key) {
    if (!m_container)
        return 0;

    switch (key) {
    case 9: // VK_TAB (TMSelectServerScene.cpp:789-795)
        if (m_pEditID && m_pEditID->IsFocused()) {
            m_container->SetFocusedControl(m_pEditPW);
            return 1;
        }
        if (m_pEditPW && m_pEditPW->IsFocused()) {
            m_container->SetFocusedControl(m_pEditID);
            return 1;
        }
        return 0;

    case 13: // VK_RETURN (TMSelectServerScene.cpp:796-799)
        if (m_pEditPW && m_pEditPW->IsFocused())
            return OnControlEvent(B_LOGIN_OK, 0);
        return 0;

    default:
        return 0;
    }
}

int SelectServerScene::OnControlEvent(uint32_t controlID, uint32_t event) {
    if (event != 0) // TMC_BUTTON_CLICK only
        return 0;

    switch (controlID) {
    case B_SERVER_SEL_OK:
        // Original validates a server pick first (TMSelectServerScene.cpp:584-617);
        // the server list needs serverlist.bin decode (phase 8c). Until then,
        // Conectar goes straight to the login box.
        ShowLogin();
        return 1;

    case B_QUIT:
        // "Voltar" — back to server select (TMSelectServerScene.cpp:625-633).
        if (m_cLogin == 1) {
            ShowServerSelect();
            return 1;
        }
        return 0;

    case B_SERVER_SEL_EXIT:
        // "Fechar" — original shows a confirm MessageBox then WM_CLOSE.
        m_quitRequested = true;
        return 1;

    case B_LOGIN_OK:
        // Network login is phase 8c+; button is inert for now.
        return 0;

    case B_CREATE_ID:
        // Original ShellExecutes the registration URL — out of scope.
        return 0;

    default:
        return 0;
    }
}

} // namespace tmx
