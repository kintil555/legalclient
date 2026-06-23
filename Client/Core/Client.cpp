#include "ModuleManager.h"
#include "../Modules/AutoSprint.h"
#include "../Modules/KeystrokeHUD.h"
#include "../Modules/AutoJumpOnDamage.h"
#include "../Modules/PlayerESP.h"
#include "../Modules/InjectionNotice.h"
#include "../UI/MenuWindow.h"
#include "../Settings/ConfigManager.h"
#include "../Events/EventBus.h"
#include "../Hooks/InputHook.h"

namespace Client::Core {

// Called once when the DLL thread starts (from DllMain worker thread).
void initializeClient() {
    auto& mgr = ModuleManager::get();

    mgr.registerModule<Modules::AutoSprint>();
    mgr.registerModule<Modules::KeystrokeHUD>();
    mgr.registerModule<Modules::AutoJumpOnDamage>();
    mgr.registerModule<Modules::PlayerESP>();
    mgr.registerModule<Modules::InjectionNotice>(); // shows inject confirmation on-screen

    mgr.bindEvents();
    Settings::ConfigManager::get().load("default.json");

    // Subscribe menu to key events (L key toggle).
    // Events arrive here because InputHook forwards real WM_KEY* messages.
    Events::EventBus::get().key.subscribe([](const Events::KeyEvent& evt) {
        UI::MenuWindow::get().onKeyEvent(evt.virtualKey, evt.isDown);
    });

    // Install WndProc hook AFTER all subscribers are registered.
    Hooks::InputHook::get().install();
}

// Called every frame by D3DHook (after ImGui::NewFrame, before ImGui::Render).
// This is what actually draws the menu and all HUD module overlays.
void renderClient() {
    UI::MenuWindow::get().render();
    Events::EventBus::get().render.dispatch({ 0.0f });
}

// Called on DLL detach.
void shutdownClient() {
    Hooks::InputHook::get().uninstall();
    Settings::ConfigManager::get().save("default.json");
}

} // namespace Client::Core
