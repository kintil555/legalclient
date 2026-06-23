#include "EntityList.h"

namespace Client::SDK {

// STUB IMPLEMENTATION - see LocalPlayer.cpp for the rationale.
// Returns an empty list so PlayerESP simply renders nothing until the
// real entity-list walk (game's entity array / linked list) is wired up.
std::vector<Entity> EntityList::getPlayers() {
    return {};
}

} // namespace Client::SDK
