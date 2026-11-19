#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <vector>
#include <string>
#include <cmath>
using namespace geode::prelude;

static bool g_orbActivated = false;
static bool g_promptShown = false;
static bool g_reverseModeActive = false;
static bool g_reverseCompleted = false;
static bool g_reverseOutroActive = false;
static bool g_playerReallyDied = false;
static float g_savedEndX = 26280.0f;
static float g_introTimer = 0.0f;
static bool g_musicStarted = false;

static bool g_dashCrashArmed = false;
static bool g_dashInWave = false;
static bool g_dashCamFixed = false;
static int g_dashLoopCount = 0;
static CCPoint g_dashWaveStartPos = { 0.0f, 0.0f };
static CCPoint g_dashFixedCamPos = { 0.0f, 0.0f };
static unsigned int g_dashWaveMusicMS = 0;

static bool g_xstepPromptOpen = false;
static bool g_xstepWarpActive = false;
static float g_xstepIntroTimer = 0.0f;
static float g_xstepTargetAngle = 0.0f;
static float g_xstepCurrentAngle = 0.0f;
static bool g_xstepLastFlipped = false;
static bool g_xstepWasOnGround = true;
static bool g_xstepSkipNextJump = false;

static EndLevelLayer* g_endLayer = nullptr;
static Ref<CCNode> g_levelCompleteNode = nullptr;

static int getSecretLevelIndex(GJGameLevel* lvl) {
    if (!lvl) return 0;
    static const std::vector<std::string> secretNames = {
        "Secret Madness", "Secret on Track", "Secret Polargiest",
        "Secret Out", "Secret after Secret", "Secret Let Go",
        "Secret Jumper", "Secret Machine", "Secret Cycles",
        "Secretstep", "Secret Clutterfunk", "Secret of Everything",
        "Secrets Adventures", "Secret Clubstep", "Secretdynamics",
        "Secret Force", "Secret Processing", "Secret of Everything 2",
        "Secret Dominator", "Secretlocked", "Secret Fingerdash",
        "Secret Dash"
    };
    std::string name(lvl->m_levelName);
    for (size_t i = 0; i < secretNames.size(); ++i) {
        if (name == secretNames[i]) return static_cast<int>(i + 1);
    }
    return 0;
}

static bool isSecretMadnessLevel(GJGameLevel* lvl) {
    return lvl && (getSecretLevelIndex(lvl) == 1 || (lvl->m_levelID == 0 && lvl->m_songID == 0));
}

static bool isSecretStepLevel(GJGameLevel* lvl) {
    return lvl && (getSecretLevelIndex(lvl) == 10 || std::string(lvl->m_levelName).find("step") != std::string::npos || lvl->m_songID == 9);
}

static bool isSecretDashLevel(GJGameLevel* lvl) {
    return lvl && (getSecretLevelIndex(lvl) == 22 || std::string(lvl->m_levelName).find("Dash") != std::string::npos || lvl->m_songID == 22);
}

static void applyDashSpeedAndPitch(PlayLayer* pl, float speed) {
    if (!pl) return;
    pl->m_gameState.m_timeWarp = speed;
    auto* fmod = FMODAudioEngine::sharedEngine();
    if (fmod && fmod->m_backgroundMusicChannel) {
        fmod->m_backgroundMusicChannel->setPitch(speed);
    }
}

static void showXstepRulesBanner(PlayLayer* pl) {
    if (!pl) return;
    auto win = CCDirector::sharedDirector()->getWinSize();
    auto container = CCNode::create();
    container->setPosition({ win.width * 0.5f, win.height * 0.52f });
    container->setScale(0.0f);
    container->setZOrder(250);

    const char* sprites[4] = {
        "portal_01_front_001.png",
        "bump_03_001.png",
        "ring_01_001.png",
        nullptr
    };
    const char* values[4] = { "-25°", "+30°", "+15°", "-5°" };
    float offsetsX[4] = { -150.0f, -50.0f, 50.0f, 150.0f };

    for (int i = 0; i < 4; ++i) {
        if (sprites[i]) {
            auto spr = CCSprite::createWithSpriteFrameName(sprites[i]);
            if (spr) {
                spr->setPosition({ offsetsX[i], 18.0f });
                spr->setScale(i == 0 ? 0.65f : 0.85f);
                container->addChild(spr);
            }
        } else {
            auto titleLbl = CCLabelBMFont::create("Every jump", "goldFont.fnt");
            titleLbl->setPosition({ offsetsX[i], 18.0f });
            titleLbl->setScale(0.55f);
            container->addChild(titleLbl);
        }
        auto valLbl = CCLabelBMFont::create(values[i], "bigFont.fnt");
        valLbl->setPosition({ offsetsX[i], -24.0f });
        valLbl->setScale(0.62f);
        container->addChild(valLbl);
    }

    pl->addChild(container);
    container->runAction(CCSequence::create(
        CCEaseBackOut::create(CCScaleTo::create(0.35f, 1.0f)),
        CCDelayTime::create(3.0f),
        CCEaseIn::create(CCMoveBy::create(0.45f, { 0.0f, -260.0f }), 2.0f),
        CCRemoveSelf::create(),
        nullptr
    ));
}
class XstepPromptPopup : public FLAlertLayerProtocol {
public:
    void FLAlert_Clicked(FLAlertLayer* layer, bool btn2) override {
        if (layer) {
            layer->setVisible(false);
            layer->removeFromParentAndCleanup(true);
        }
        g_xstepPromptOpen = false;
        auto* pl = PlayLayer::get();
        if (!pl) return;

        if (!btn2) {
            g_xstepWarpActive = false;
            pl->resetLevel();
        } else {
            g_xstepWarpActive = true;
            g_xstepTargetAngle = 0.0f;
            g_xstepCurrentAngle = 0.0f;
            g_xstepIntroTimer = 3.35f;
            pl->resetLevel();
            FMODAudioEngine::sharedEngine()->pauseAllMusic(true);
            if (pl->m_player1) {
                pl->m_player1->m_isLocked = true;
                g_xstepLastFlipped = pl->m_player1->m_isUpsideDown;
            }
            showXstepRulesBanner(pl);
        }
    }
};

static XstepPromptPopup g_xstepPopupHandler;

class ReversePromptPopup : public FLAlertLayerProtocol {
public:
    void FLAlert_Clicked(FLAlertLayer* layer, bool btn2) override {
        if (layer) {
            layer->setVisible(false);
            layer->removeFromParentAndCleanup(true);
        }
        auto* pl = PlayLayer::get();
        if (!pl) return;

        if (!btn2) {
            g_orbActivated = false;
            if (g_levelCompleteNode) {
                g_levelCompleteNode->resumeSchedulerAndActions();
                g_levelCompleteNode = nullptr;
            }
            if (g_endLayer) {
                g_endLayer->setVisible(true);
                g_endLayer->setPositionY(0.0f);
            }
        } else {
            g_orbActivated = false;
            g_reverseModeActive = true;
            g_reverseCompleted = false;
            g_reverseOutroActive = false;
            g_playerReallyDied = false;
            g_introTimer = 1.25f;
            g_musicStarted = false;

            if (g_endLayer) {
                g_endLayer->removeFromParentAndCleanup(true);
                g_endLayer = nullptr;
            }
            pl->stopAllActions();
            pl->m_hasCompletedLevel = false;
            pl->spawnGroup(2, false, 0.0, {}, -1, -1);

            if (g_levelCompleteNode) {
                g_levelCompleteNode->resumeSchedulerAndActions();
                g_levelCompleteNode->stopAllActions();
                g_levelCompleteNode->runAction(CCSequence::create(
                    CCScaleTo::create(0.12f, 1.25f),
                    CCScaleTo::create(0.28f, 0.0f),
                    CCRemoveSelf::create(),
                    nullptr
                ));
                g_levelCompleteNode = nullptr;
            }

            if (pl->m_endPortal) {
                pl->m_endPortal->stopAllActions();
                pl->m_endPortal->setVisible(true);
                pl->m_endPortal->setPosition(pl->m_endPosition);
                pl->m_endPortal->updateEndPos(true);
                pl->m_endPortal->runAction(CCSequence::create(
                    CCDelayTime::create(0.05f),
                    CCMoveBy::create(0.85f, { 280.0f, 0.0f }),
                    CCHide::create(),
                    nullptr
                ));
            }

            auto* p1 = pl->m_player1;
            if (p1) {
                p1->m_isLocked = true;
                p1->m_isDead = false;
                p1->setVisible(false);
                p1->setOpacity(255);
                p1->setScale(1.0f);
                p1->setRotation(-180.0f);
                p1->setPosition({ g_savedEndX + 340.0f, 195.0f });
                p1->reversePlayer(nullptr);
            }

            FMODAudioEngine::sharedEngine()->stopAllMusic(true);
            FMODAudioEngine::sharedEngine()->playEffect("endStart_reverse.ogg"_spr);
        }
    }
};

static ReversePromptPopup g_reversePopupHandler;

class $modify(HiddenSecretsMod, PlayLayer) {
    struct Fields {
        bool m_waitingPrompt = false;
        float m_endTimer = 0.0f;
        float m_camStopTimer = 0.0f;
    };

    void pauseGame(bool p0) {
        if (g_reverseModeActive) this->m_hasCompletedLevel = false;
        PlayLayer::pauseGame(p0);
    }

    void onQuit() {
        g_orbActivated = false;
        g_promptShown = false;
        g_reverseModeActive = false;
        g_reverseCompleted = false;
        g_reverseOutroActive = false;
        g_playerReallyDied = false;
        g_introTimer = 0.0f;
        g_musicStarted = false;
        g_dashCrashArmed = false;
        g_dashInWave = false;
        g_dashCamFixed = false;
        g_dashLoopCount = 0;
        g_xstepPromptOpen = false;
        g_xstepWarpActive = false;
        g_xstepIntroTimer = 0.0f;
        g_xstepTargetAngle = 0.0f;
        g_xstepCurrentAngle = 0.0f;
        applyDashSpeedAndPitch(this, 1.0f);
        g_endLayer = nullptr;
        g_levelCompleteNode = nullptr;
        PlayLayer::onQuit();
    }

    void resetLevelFromStart() {
        if (isSecretMadnessLevel(this->m_level) && (g_orbActivated || m_fields->m_waitingPrompt || (g_reverseModeActive && !g_playerReallyDied))) {
            return;
        }
        g_orbActivated = false;
        g_promptShown = false;
        g_reverseModeActive = false;
        g_reverseCompleted = false;
        g_reverseOutroActive = false;
        g_playerReallyDied = false;
        g_introTimer = 0.0f;
        g_musicStarted = false;
        g_dashCrashArmed = false;
        g_dashInWave = false;
        g_dashCamFixed = false;
        g_dashLoopCount = 0;
        g_xstepPromptOpen = false;
        g_xstepWarpActive = false;
        g_xstepIntroTimer = 0.0f;
        g_xstepTargetAngle = 0.0f;
        g_xstepCurrentAngle = 0.0f;
        applyDashSpeedAndPitch(this, 1.0f);
        g_endLayer = nullptr;
        g_levelCompleteNode = nullptr;
        m_fields->m_waitingPrompt = false;
        m_fields->m_endTimer = 0.0f;
        m_fields->m_camStopTimer = 0.0f;
        PlayLayer::resetLevelFromStart();
    }
    void resetLevel() {
        if (isSecretStepLevel(this->m_level)) {
            if (g_xstepPromptOpen) return;
            g_xstepTargetAngle = 0.0f;
            g_xstepCurrentAngle = 0.0f;
            if (this->m_objectLayer) this->m_objectLayer->setRotation(0.0f);
            this->m_gameState.m_cameraAngle = 0.0f;
            PlayLayer::resetLevel();
            if (this->m_player1) g_xstepLastFlipped = this->m_player1->m_isUpsideDown;
            return;
        }
        if (isSecretDashLevel(this->m_level)) {
            g_dashCrashArmed = false;
            g_dashInWave = false;
            g_dashCamFixed = false;
            g_dashLoopCount = 0;
            m_fields->m_camStopTimer = 0.0f;
            applyDashSpeedAndPitch(this, 1.0f);
            PlayLayer::resetLevel();
            return;
        }
        if (!isSecretMadnessLevel(this->m_level)) {
            PlayLayer::resetLevel();
            return;
        }
        if (g_orbActivated || m_fields->m_waitingPrompt || (g_reverseModeActive && !g_playerReallyDied)) {
            return;
        }
        g_orbActivated = false;
        g_promptShown = false;
        g_reverseModeActive = false;
        g_reverseCompleted = false;
        g_reverseOutroActive = false;
        g_playerReallyDied = false;
        g_introTimer = 0.0f;
        g_musicStarted = false;
        g_endLayer = nullptr;
        g_levelCompleteNode = nullptr;
        m_fields->m_waitingPrompt = false;
        m_fields->m_endTimer = 0.0f;
        PlayLayer::resetLevel();
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        if (isSecretStepLevel(this->m_level) && obj && !g_xstepPromptOpen) {
            float pct = this->getCurrentPercent();
            if (pct >= 4.65f && pct <= 5.25f) {
                g_xstepPromptOpen = true;
                this->m_player1->m_isLocked = true;
                FMODAudioEngine::sharedEngine()->pauseAllMusic(true);
                auto alert = FLAlertLayer::create(
                    &g_xstepPopupHandler,
                    "Mysterious Secret",
                    "Do you want to <cp>warp reality</c>?",
                    "No",
                    "Yes"
                );
                alert->show();
                return;
            }
        }
        if (isSecretMadnessLevel(this->m_level) && g_reverseModeActive && (g_introTimer > 0.0f || g_reverseOutroActive || (obj && (!obj->isVisible() || obj->m_isHide || obj->getOpacity() == 0)))) {
            return;
        }
        PlayLayer::destroyPlayer(player, obj);
        if (g_reverseModeActive) g_playerReallyDied = true;
    }

    void bumpPlayer(PlayerObject* p0, EffectGameObject* p1) {
        PlayLayer::bumpPlayer(p0, p1);
        if (isSecretStepLevel(this->m_level) && g_xstepWarpActive && g_xstepIntroTimer <= 0.0f) {
            g_xstepTargetAngle += 30.0f;
            g_xstepSkipNextJump = true;
        }
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!this->m_player1) return;

        if (isSecretStepLevel(this->m_level)) {
            if (g_xstepPromptOpen) {
                this->m_player1->m_isLocked = true;
                return;
            }
            if (g_xstepWarpActive) {
                if (g_xstepIntroTimer > 0.0f) {
                    g_xstepIntroTimer -= dt;
                    this->m_player1->m_isLocked = true;
                    this->m_player1->setPositionX(0.0f);
                    if (g_xstepIntroTimer <= 0.0f) {
                        this->m_player1->m_isLocked = false;
                        FMODAudioEngine::sharedEngine()->resumeAllMusic();
                    }
                    return;
                }

                bool onGround = this->m_player1->m_isOnGround;
                if (g_xstepWasOnGround && !onGround && !this->m_player1->m_isShip && !this->m_player1->m_isDart && !this->m_player1->m_isSwing && !this->m_player1->m_isBall && !this->m_player1->m_isBird) {
                    if (g_xstepSkipNextJump) g_xstepSkipNextJump = false;
                    else g_xstepTargetAngle -= 5.0f;
                }
                g_xstepWasOnGround = onGround;
                if (this->m_player1->m_isUpsideDown != g_xstepLastFlipped) {
                    g_xstepLastFlipped = this->m_player1->m_isUpsideDown;
                    g_xstepTargetAngle -= 25.0f;
                }

                g_xstepCurrentAngle += (g_xstepTargetAngle - g_xstepCurrentAngle) * std::min(1.0f, dt * 8.0f);
                this->m_gameState.m_cameraAngle = g_xstepCurrentAngle;
            }
            return;
        }

        if (isSecretDashLevel(this->m_level)) {
            float px = this->m_player1->getPositionX();
            float py = this->m_player1->getPositionY();

            if (!g_dashCrashArmed && px > 21000.0f && py > 195.0f) {
                g_dashCrashArmed = true;
                g_dashInWave = false;
                g_dashCamFixed = false;
                g_dashLoopCount = 0;
                m_fields->m_camStopTimer = 0.0f;
            }

            if (g_dashCrashArmed && !this->m_player1->m_isDead) {
                if (g_dashWaveStartPos.x < 1000.0f && !this->m_player1->m_isDart && px > 20000.0f) {
                    g_dashWaveStartPos = this->m_player1->getPosition();
                    auto* fmod = FMODAudioEngine::sharedEngine();
                    if (fmod) g_dashWaveMusicMS = fmod->getMusicTimeMS(0);
                }

                if (!g_dashInWave && this->m_player1->m_isDart) {
                    g_dashInWave = true;
                }

                if (g_dashInWave && !g_dashCamFixed && this->m_objectLayer) {
                    m_fields->m_camStopTimer += dt;
                    if (m_fields->m_camStopTimer >= 0.45f) {
                        g_dashCamFixed = true;
                        g_dashFixedCamPos = this->m_objectLayer->getPosition();
                    }
                }

                if (g_dashCamFixed && this->m_objectLayer) {
                    this->m_objectLayer->setPosition(g_dashFixedCamPos);
                }

                if (g_dashInWave && g_dashWaveStartPos.x > 1000.0f && (!this->m_player1->m_isDart || px >= g_dashWaveStartPos.x + 840.0f)) {
                    g_dashInWave = false;
                    g_dashLoopCount++;
                    if (g_dashLoopCount >= 25) {
                        std::abort();
                    }
                    float nextSpeed = 1.0f + (g_dashLoopCount * 0.04f);
                    applyDashSpeedAndPitch(this, nextSpeed);
                    this->m_player1->setPosition({ g_dashWaveStartPos.x - 40.0f, g_dashWaveStartPos.y });
                    this->m_player1->m_yVelocity = 0.0;
                    auto* fmod = FMODAudioEngine::sharedEngine();
                    if (fmod) fmod->setMusicTimeMS(g_dashWaveMusicMS, true, 0);
                }
            }
            return;
        }
        if (!isSecretMadnessLevel(this->m_level)) return;

        if (!g_orbActivated && !g_reverseModeActive && this->m_objects) {
            for (auto* obj : CCArrayExt<GameObject*>(this->m_objects)) {
                if (obj && obj->m_objectID == 1594) {
                    if (fabsf(obj->getPositionX() - this->m_player1->getPositionX()) < 100.0f && fabsf(obj->getPositionY() - this->m_player1->getPositionY()) < 100.0f) {
                        g_orbActivated = true;
                        g_savedEndX = this->m_player1->getPositionX();
                        break;
                    }
                }
            }
        }

        if (m_fields->m_waitingPrompt && !g_promptShown) {
            m_fields->m_endTimer += dt;
            if (m_fields->m_endTimer >= 0.04f) {
                m_fields->m_waitingPrompt = false;
                g_promptShown = true;

                auto win = CCDirector::sharedDirector()->getWinSize();
                auto* children = this->getChildren();
                if (children) {
                    for (int i = static_cast<int>(children->count()) - 1; i >= 0; --i) {
                        auto* child = static_cast<CCNode*>(children->objectAtIndex(i));
                        if (!child || child == this->m_objectLayer || !child->isVisible()) continue;
                        if (fabsf(child->getPositionX() - win.width * 0.5f) < 80.0f && fabsf(child->getPositionY() - win.height * 0.5f) < 80.0f && !g_levelCompleteNode) {
                            g_levelCompleteNode = child;
                            g_levelCompleteNode->pauseSchedulerAndActions();
                            break;
                        }
                    }
                }

                auto alert = FLAlertLayer::create(
                    &g_reversePopupHandler,
                    "Reverse Secret",
                    "Do you want to activate the <cp>Reverse Mode</c>?",
                    "No",
                    "Yes"
                );
                alert->show();
            }
        }

        if (g_reverseModeActive && !g_reverseCompleted) {
            if (!this->m_player1->m_isDead) g_playerReallyDied = false;

            if (g_introTimer > 0.0f) {
                g_introTimer -= dt;
                this->m_player1->m_isLocked = true;
                this->m_player1->m_yVelocity = 0.0;

                if (g_introTimer > 0.85f) {
                    this->m_player1->setVisible(false);
                    this->m_player1->setScale(1.0f);
                    this->m_player1->setRotation(-180.0f);
                    this->m_player1->setPosition({ g_savedEndX + 340.0f, 195.0f });
                } else {
                    if (!g_musicStarted) {
                        g_musicStarted = true;
                        FMODAudioEngine::sharedEngine()->playMusic("StereoMadness_reverse.mp3"_spr, true, 0.0f, 0);
                    }
                    float tOut = 1.0f - (g_introTimer / 0.85f);
                    float curX = (g_savedEndX + 340.0f) - (340.0f * tOut);
                    this->m_player1->setVisible(true);
                    this->m_player1->setScale(1.0f);
                    this->m_player1->setRotation(-180.0f * (1.0f - tOut));
                    this->m_player1->setPosition({ curX, 195.0f });
                }

                if (g_introTimer <= 0.0f) {
                    this->m_player1->m_isLocked = false;
                    this->m_player1->setRotation(0.0f);
                    if (this->m_endPortal) {
                        this->m_endPortal->setVisible(false);
                        this->m_endPortal->setPosition({ -5000.0f, -5000.0f });
                    }
                }
                return;
            }

            float px = this->m_player1->getPositionX();
            if (this->m_player1->m_isShip && this->m_player1->getPositionY() > 282.0f) {
                this->m_player1->setPositionY(282.0f);
                if (this->m_player1->m_yVelocity > 0.0) this->m_player1->m_yVelocity = 0.0;
            }
            float pct = (px / g_savedEndX) * 100.0f;
            if (pct < 0.0f) pct = 0.0f;
            if (pct > 100.0f) pct = 100.0f;

            if (this->m_percentageLabel) {
                this->m_percentageLabel->setString(fmt::format("{:.2f}%", pct).c_str());
            }
            if (this->m_progressFill && this->m_progressBar) {
                float fullW = this->m_progressBar->getContentSize().width - 4.0f;
                this->m_progressFill->setTextureRect({ 0.0f, 0.0f, fullW * (pct / 100.0f), this->m_progressFill->getContentSize().height });
            }

            if (px <= 140.0f && !g_reverseOutroActive) {
                g_reverseOutroActive = true;
                this->m_player1->m_isLocked = true;
            }

            if (g_reverseOutroActive) {
                this->m_player1->setPositionX(px - (310.0f * dt));
                this->m_player1->m_yVelocity = 0.0;
                if (this->m_objectLayer && this->m_objectLayer->getPositionX() < 0.0f) {
                    float nextCamX = this->m_objectLayer->getPositionX() + (120.0f * dt);
                    if (nextCamX > 0.0f) nextCamX = 0.0f;
                    this->m_objectLayer->setPositionX(nextCamX);
                }
                if (this->m_player1->getPositionX() <= -75.0f) {
                    g_reverseCompleted = true;
                    g_reverseModeActive = false;
                    g_reverseOutroActive = false;
                    FMODAudioEngine::sharedEngine()->stopAllMusic(true);
                    FMODAudioEngine::sharedEngine()->playEffect("playSound_reverse.ogg"_spr);
                    this->onQuit();
                }
            }
        }
    }
};

class $modify(ReverseEndLayerHook, EndLevelLayer) {
    void showLayer(bool instant) {
        if (g_orbActivated && !g_reverseCompleted) {
            g_endLayer = this;
            auto* pl = PlayLayer::get();
            if (pl) {
                static_cast<HiddenSecretsMod*>(pl)->m_fields->m_waitingPrompt = true;
                static_cast<HiddenSecretsMod*>(pl)->m_fields->m_endTimer = 0.0f;
            }
            this->setVisible(false);
            this->setPositionY(2000.0f);
            return;
        }
        EndLevelLayer::showLayer(instant);
    }
};

class $modify(SecretOrbHook, PlayerObject) {
    void ringJump(RingObject* ring, bool p1) {
        PlayerObject::ringJump(ring, p1);
        auto* pl = PlayLayer::get();
        if (!ring || !pl) return;
        if (isSecretStepLevel(pl->m_level) && g_xstepWarpActive && g_xstepIntroTimer <= 0.0f) {
            g_xstepTargetAngle += 15.0f;
            g_xstepSkipNextJump = true;
        }
        if (ring->m_objectID == 1594 && isSecretMadnessLevel(pl->m_level)) {
            g_orbActivated = true;
            g_savedEndX = this->getPositionX();
        }
    }
};

class $modify(ReverseKeyHook, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat, double ts) {
        auto* pl = PlayLayer::get();
        if (isKeyDown && !isKeyRepeat && g_reverseModeActive && pl && key == enumKeyCodes::KEY_Escape) {
            pl->m_hasCompletedLevel = false;
            pl->pauseGame(false);
            return true;
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat, ts);
    }
};
