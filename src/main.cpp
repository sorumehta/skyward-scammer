#include "RPG_Game.h"

RPG_Dynamic * RPG_Game::findObjectByName (std::vector<RPG_Dynamic *> vectorDyns, const char *name) {
    auto it = std::find_if(vectorDyns.begin(), vectorDyns.end(), [&](RPG_Dynamic *obj){
        return obj->sName == name;
    });
    if(it != vectorDyns.end()){
        return *it;
    }
    return nullptr;
}

void RPG_Game::handleInputEvent(int eventType, int keyCode, float fElapsedTime) {
    if (eventType == SDL_KEYDOWN) {
        if (keyCode == SDLK_SPACE) {
            if (!mScript.bUserControlEnabled && !bGameOver) {
                if(bShowDialog){
                    bShowDialog = false;
                    mScript.completeCommand();
                }

            }
        }
        if (keyCode == SDLK_s) {
            gameStarted = true;
            mScript.addCommand(
                    new Command_ShowDialog({"Welcome.", "You are here to meet Tuco", "You meet Gonzo, his associate"},
                                           {0xFF, 0, 0}));
            RPG_Dynamic *gonzo = findObjectByName(mVecDynamics, "Enemy");

            mScript.addCommand(new Command_MoveTo(gonzo, 14, 3, 2));
            mScript.addCommand(new Command_ShowDialog({"You:", "I want to see Tuco"}));
            mScript.addCommand(new Command_ShowDialog({"Gonzo:", "Appointments only!", "And you don't have one"}));
            mScript.addCommand(new Command_ShowDialog({"You:", "I am gonna meet him.", "Stop me if you can"}));
            mScript.addCommand(
                    new Command_ShowDialog({"Run to the main door", "Don't let Gonzo catch you"}, {0xFF, 0, 0}));
        }
    }
}

void RPG_Game::handleInputState(const unsigned char *state, int mouseX, int mouseY, float secPerFrame) {
    if (!mScript.bUserControlEnabled) {
        return;
    }
    if (state[SDL_SCANCODE_UP]) {
        player->vy -= 10.0f * secPerFrame;
    }
    if (state[SDL_SCANCODE_DOWN]) {
        player->vy += 10.0f * secPerFrame;
    }
    if (state[SDL_SCANCODE_LEFT]) {
        player->vx -= 10.0f * secPerFrame;
    }
    if (state[SDL_SCANCODE_RIGHT]) {
        player->vx += 10.0f * secPerFrame;
    }
}

bool RPG_Game::onInit() {
    gameStarted = false;
    playerOnRun = false;
    bGameOver = false;
    totalTimeElapsed = 0.0f;
    // becuase this is static, this would enable the subclasses to access the game object as well
    RPG_Commands::engine = this;
    cMap::g_scriptProcessor = &mScript;
    ASSETS.loadSprites();
    ASSETS.loadMaps();
    player = new DynamicCreature("Player", ASSETS.getSprite(PLAYER_SPR_IDX), PLAYER_SPR_W, PLAYER_SPR_H, 3);


//        DynamicCreature *dynObj1 = new DynamicCreature("Gonzo", ASSETS.getSprite(1), 32, 32, 4);
//        dynObj1->px = 5;
//        dynObj1->py = 5;

    // player is always the first object in the vector
//        mVecDynamics.emplace_back(player);
//        mVecDynamics.emplace_back(dynObj1);

    nTileWidth = 24;
    nTileHeight = 24;
//        pCurrentMap = ASSETS.getMap("village");
    changeMap("village", 12, 3);

    std::unordered_map<std::string, std::string> soundWavFiles;
    soundWavFiles["trumpet"] = "../res/sound/trumpet.wav";
    soundWavFiles["orchestra"] = "../res/sound/orchestra.wav";
    loadSoundEffects(soundWavFiles);
    return true;
}

void RPG_Game::showDialog(std::vector<std::string> vecLines){
    vecDialogToShow = vecLines;
    bShowDialog = true;
}

void RPG_Game::displayDialog(std::vector<std::string> vecLines, int dialogBoxPosX, int dialogBoxPosY){
    int nLines = vecLines.size(); // h
    int maxLineLength = 0; // w
    for (auto l : vecLines){
        if (l.size() > maxLineLength){
            maxLineLength = l.size();
        }
    }
    GameEngine::fillRect(dialogBoxPosX, dialogBoxPosY, maxLineLength * 20, nLines * 20, {0, 0, 0xFF});
    for (int l = 0; l < vecLines.size(); l++){
        drawText(vecLines[l], dialogBoxPosX, dialogBoxPosY + l*18);
    }
    drawText("press SPACE to continue", dialogBoxPosX, dialogBoxPosY + vecLines.size()*18 + 1);

}

// change map when player is teleported
void RPG_Game::changeMap(std::string mapName, float x, float y) {
    mVecDynamics.clear();
    mVecDynamics.emplace_back(player); // player is the first object in the vector
    pCurrentMap = ASSETS.getMap(mapName);
    player->px = x;
    player->py = y;
    // append the map dynamics (teleports etc) to the dynamic objects vector
    pCurrentMap->PopulateDynamics(mVecDynamics);
}

bool RPG_Game::onFrameUpdate(float fElapsedTime) {
    // utility functions
    auto doObjectsOverlap = [](float px1, float py1, float px2, float py2) {
        float distance = (px1 - px2) * (px1 - px2) + (py1 - py2) * (py1 - py2);
        return distance <= 1.0f;
    };

    float maxVelocity = 10.0f;
    for (auto &object: mVecDynamics) {
        // clamp velocities
        if (object->vy > maxVelocity) {
            object->vy = maxVelocity;
        }
        if (object->vy < -maxVelocity) {
            object->vy = -maxVelocity;
        }
        if (object->vx > maxVelocity) {
            object->vx = maxVelocity;
        }
        if (object->vx < -maxVelocity) {
            object->vx = -maxVelocity;
        }

        float fNewObjectPosX = object->px + object->vx * fElapsedTime;
        float fNewObjectPosY = object->py + object->vy * fElapsedTime;
        // resolve collision along X axis, if any
        float border = 0.1f; // the border on the tiles to shrink the opaqueness of tiles and make collisions a bit more relaxed
        if (object->vx < 0) {
            if (pCurrentMap->GetSolid(static_cast<int>(fNewObjectPosX + border), static_cast<int>(object->py + border)) ||
                pCurrentMap->GetSolid(static_cast<int>(fNewObjectPosX + border), static_cast<int>(object->py + 1 - border))) {
                // cast the new position to an integer and shift by 1 so that the player is on the boundary
                // of the colliding tile, instead of leaving some space
                fNewObjectPosX = static_cast<int>(fNewObjectPosX) + 1;
                object->vx = 0;
            }
        } else if (object->vx > 0) {
            if (pCurrentMap->GetSolid(static_cast<int>(fNewObjectPosX + 1 - border), static_cast<int>(object->py + border)) ||
                pCurrentMap->GetSolid(static_cast<int>(fNewObjectPosX + 1 - border), static_cast<int>(object->py + 1 - border))) {
                fNewObjectPosX = static_cast<int>(fNewObjectPosX);
                object->vx = 0;
            }
        }
        // check collision along y
        if (object->vy < 0) {
            if (pCurrentMap->GetSolid(static_cast<int>(fNewObjectPosX + border), static_cast<int>(fNewObjectPosY + border)) ||
                pCurrentMap->GetSolid(static_cast<int>(fNewObjectPosX + 1 - border), static_cast<int>(fNewObjectPosY + border))) {
                fNewObjectPosY = static_cast<int>(fNewObjectPosY) + 1;
                object->vy = 0;
            }
        } else if (object->vy > 0) {
            if (pCurrentMap->GetSolid(static_cast<int>(fNewObjectPosX + border), static_cast<int>(fNewObjectPosY + 1 - border)) ||
                pCurrentMap->GetSolid(static_cast<int>(fNewObjectPosX + 1 - border),
                                      static_cast<int>(fNewObjectPosY + 1 - border))) {
                fNewObjectPosY = static_cast<int>(fNewObjectPosY);
                object->vy = 0;
            }
        }

        float fDynamicObjPosX = fNewObjectPosX;
        float fDynamicObjPosY = fNewObjectPosY;
        for (auto &dyn: mVecDynamics) {
            if (dyn != object) {
                // player must not overlap if solidVDyn is true
                if (dyn->bSolidVsDyn && object->bSolidVsDyn) {
                } else { // object can interact with things
                    // object is player
                    if (object->sName == player->sName) {
                        if (fDynamicObjPosX < (dyn->px + 1.0f) && (fDynamicObjPosX + 1.0f) > dyn->px &&
                            object->py < (dyn->py + 1.0f) && (object->py + 1.0f) > dyn->py)
                        {
                            pCurrentMap->onInteraction(mVecDynamics, dyn, cMap::WALK);
                        }
                    }
                }
            }
        }

        object->px = fNewObjectPosX;
        object->py = fNewObjectPosY;

        // apply friction
        object->vx += -4.0f * object->vx * fElapsedTime;
        if (std::abs(object->vx) < 0.01f) {
            object->vx = 0.0f;
        }
        object->vy += -4.0f * object->vy * fElapsedTime;
        if (std::abs(object->vy) < 0.01f) {
            object->vy = 0.0f;
        }
        object->update(fElapsedTime, player, pCurrentMap);
    }
    fCameraPosX = player->px;
    fCameraPosY = player->py;
    // Draw Level
    int nVisibleTilesX = mWindowWidth / nTileWidth;
    int nVisibleTilesY = mWindowHeight / nTileHeight;
    // get the top left corner (in tiles space) to be shown on screen
    // camera posX and posY are also in tile space
    float fOffsetX = fCameraPosX - static_cast<float>(nVisibleTilesX) / 2.0f;
    float fOffsetY = fCameraPosY - static_cast<float>(nVisibleTilesY) / 2.0f;
    // clamp
    if (fOffsetX < 0) fOffsetX = 0;
    if (fOffsetY < 0) fOffsetY = 0;
    if (fOffsetX > static_cast<float>(pCurrentMap->nWidth - nVisibleTilesX))
        fOffsetX = static_cast<float>(pCurrentMap->nWidth - nVisibleTilesX);
    if (fOffsetY > static_cast<float>(pCurrentMap->nHeight - nVisibleTilesY))
        fOffsetY = static_cast<float>(pCurrentMap->nHeight - nVisibleTilesY);

    float fTileOffsetX = fOffsetX - static_cast<int>(fOffsetX);
    float fTileOffsetY = fOffsetY - static_cast<int>(fOffsetY);
    // draw each tile
    // we overdraw on the corners to avoid distortion (hacky)
    for (int x = -1; x < nVisibleTilesX + 1; x++) {
        for (int y = -1; y < nVisibleTilesY + 1; y++) {
            int spriteIdx = pCurrentMap->GetIndex(x + static_cast<int>(fOffsetX),
                                                  y + static_cast<int>(fOffsetY));
            LTexture *texture = ASSETS.getSprite(spriteIdx);
            texture->drawTexture(static_cast<int>((x - fTileOffsetX) * nTileWidth),
                                 static_cast<int>((y - fTileOffsetY) * nTileHeight), nTileWidth, nTileHeight);
        }
    }
    for (auto &object: mVecDynamics) {
        // draw object
        object->drawSelf(this, fOffsetX, fOffsetY, nTileWidth, nTileHeight);
    }
    if(bShowDialog){
        displayDialog(vecDialogToShow, 20, 20);
    }
    mScript.processCommand(fElapsedTime);
    if (!gameStarted) {
        drawText("Press S to start the game", 200, 200);
    }
    return true;
}


int main() {
    RPG_Game echoes;
    echoes.init(30 * 24, 16 * 24, "Echoes Of Deception");
    echoes.startGameLoop();
    return 0;
}
