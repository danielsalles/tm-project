#pragma once

#include "ui/SControl.h"

#include <cstdint>

namespace tmx {

class SControlContainer;
class SPanel;
class SButton;
class SText;
class SListBox;
class SEditableText;

// UI logic of the initial screen (server select + login box).
// Port of TMSelectServerScene::InitializeScene / InitializeUI layout and the
// button-driven state flow (Projects/TMProject/TMSelectServerScene.cpp).
//
// Boot state: server panel centered+75 (visible), login panel hidden,
// logo pair centered top, copyright bottom. "Conectar" swaps to the login
// box; "Voltar" goes back; "Fechar" quits.
class SelectServerScene : public IEventListener {
public:
    // FindControl wiring + one-time text nudges. Call after ReadRCBin.
    bool Init(SControlContainer* container);

    // Resolution-dependent layout (idempotent). Call on load and on resize.
    // Ports TMSelectServerScene.cpp:153-186 + InitializeUI:1353-1357.
    void Layout(int screenW, int screenH);

    // Initial visibility (TMSelectServerScene.cpp:222-232, 1344-1352).
    void ApplyBootState();

    // IEventListener — button routing. Main loop reads QuitRequested().
    int OnControlEvent(uint32_t controlID, uint32_t event) override;

    // Key routing (TMSelectServerScene.cpp:785-806): TAB toggles ID/PW focus,
    // RETURN on the PW field fires Login OK. Returns 1 when consumed.
    int OnKeyDown(int key);

    bool QuitRequested() const { return m_quitRequested; }
    bool IsLoginVisible() const { return m_cLogin == 1; }

private:
    void ShowLogin();
    void ShowServerSelect();

    SControlContainer* m_container = nullptr;

    SPanel*        m_pServerPanel = nullptr;     // P_SERVER_SEL 65537
    SListBox*      m_pServerGroupList = nullptr; // L_SELECT_SERVERG 65542
    SListBox*      m_pServerList = nullptr;      // L_SELECT_SERVER 65543
    SPanel*        m_pLoginPanel = nullptr;      // P_LOGIN_BOX 65870
    SEditableText* m_pEditID = nullptr;          // E_LOGIN_ID 65871
    SEditableText* m_pEditPW = nullptr;          // E_LOGIN_PASSWORD 65872
    SButton*       m_pLoginBtns[3] = {};         // 65873 OK / 65874 Voltar / 65875 Novo ID
    SPanel*        m_pLogo[2] = {};              // TMP_LOGO_PANEL1/2 311/312
    SText*         m_pCopyright = nullptr;       // TMT_SCENE_TEXT 769

    int  m_cLogin = 0; // 0 = server select, 1 = login box (original m_cLogin)
    bool m_quitRequested = false;
};

// Control IDs (Projects/TMProject/ResourceControl.h).
namespace SelectServerID {
    constexpr uint32_t TMP_LOGO_PANEL1     = 311;
    constexpr uint32_t TMP_LOGO_PANEL2     = 312;
    constexpr uint32_t TMT_SCENE_TEXT      = 769;
    constexpr uint32_t TMT_SEL_SERVER_TEXT = 5635;
    constexpr uint32_t P_SERVER_SEL        = 65537;
    constexpr uint32_t B_SERVER_SEL_OK     = 65538;
    constexpr uint32_t B_SERVER_SEL_EXIT   = 65539;
    constexpr uint32_t L_SELECT_SERVERG    = 65542;
    constexpr uint32_t L_SELECT_SERVER     = 65543;
    constexpr uint32_t P_LOGIN_BOX         = 65870;
    constexpr uint32_t E_LOGIN_ID          = 65871;
    constexpr uint32_t E_LOGIN_PASSWORD    = 65872;
    constexpr uint32_t B_LOGIN_OK          = 65873;
    constexpr uint32_t B_QUIT              = 65874;
    constexpr uint32_t B_CREATE_ID         = 65875;
    constexpr uint32_t T_LOGIN_BOX_TEXT    = 65876;
}

} // namespace tmx
