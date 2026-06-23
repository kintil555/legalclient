#include "Core/ModuleManager.h"
#include "Modules/AutoSprint.h"
#include "Modules/KeystrokeHUD.h"
#include "Modules/AutoJumpOnDamage.h"
#include "Modules/PlayerESP.h"
#include "Modules/InjectionNotice.h"
#include "UI/MenuWindow.h"
#include "Settings/ConfigManager.h"
#include "Events/EventBus.h"
#include "Hooks/InputHook.h"   // <-- NEW: real WndProc hook that feeds EventBus

namespace Client::Core {

// Called once when the DLL is injected/attached.
void initializeClient() {
    auto& mgr = ModuleManager::get();

    mgr.registerModule<Modules::AutoSprint>();
    mgr.registerModule<Modules::KeystrokeHUD>();
    mgr.registerModule<Modules::AutoJumpOnDamage>();
    mgr.registerModule<Modules::PlayerESP>();
    mgr.registerModule<Modules::InjectionNotice>(); // confirms successful inject on-screen

    mgr.bindEvents();
    Settings::ConfigManager::get().load("default.json");

    // Wire the key channel: menu toggle (L key) + module raw-key callbacks.
    // This subscriber now actually receives events because InputHook below
    // dispatches real WM_KEY* messages into the bus.
    Events::EventBus::get().key.subscribe([](const Events::KeyEvent& evt) {
        // Forward to menu first (handles L-key toggle).
        UI::MenuWindow::get().onKeyEvent(evt.virtualKey, evt.isDown);
    });

    // Install the WndProc hook LAST, after all subscribers are set up,
    // so no event can arrive before the bus is fully ready.
    //
    // FIX: This was the missing piece.  AI previously wired up subscribers
    // but never installed any hook that forwarded real Windows input into
    // EventBus::key.  Without this call, onKeyEvent() was never invoked and
    // the menu was permanently invisible regardless of what key you pressed.
    Hooks::InputHook::get().install();
}

// Called every rendered frame, after ImGui::NewFrame().
void renderClient() {
    UI::MenuWindow::get().render();
    Events::EventBus::get().render.dispatch({ /*deltaTime*/ 0.0f });
}

// Called on DLL detach / game shutdown.
void shutdownClient() {
    // Remove the WndProc hook BEFORE saving config so we don't accidentally
    // dispatch stale events during teardown.
    Hooks::InputHook::get().uninstall();
    Settings::ConfigManager::get().save("default.json");
}

} // namespace Client::Core
