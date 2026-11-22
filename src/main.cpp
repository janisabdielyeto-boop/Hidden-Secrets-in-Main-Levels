#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace geode::prelude;

struct SecretDef {
    const char* key;
    int levelNum;
    const char* levelName;
    const char* secretTitle;
    const char* whatItIs;
    const char* howToFind;
};

static const std::vector<SecretDef> g_implementedSecrets = {
    {
        "secret_level_1",
        1,
        "Stereo Madness",
        "Reverse Mode",
        "Plays the end-wall reverse curve and ring shockwave, then\nrewinds from 100.00% down to 0.00% in balanced Free Mode!",
        "Step 1: Open Level 1 (Stereo Madness or Secret Madness).\nStep 2: Fly to the top at the end of the final ship section.\nStep 3: Click or tap the hidden toggle orb and press YES!"
    },
    {
        "secret_level_2",
        2,
        "Back on Track",
        "Gravity Chaos",
        "Unleashes Gravity Chaos: your gravity flips automatically\nevery 2.5 seconds with a warning flash and sound until 100%!",
        "Step 1: Open Level 2 (Back on Track) in Normal Mode.\nStep 2: Right at the start, jump over the first spike\nand land directly on the second spike (1.70% to 2.00%).\nStep 3: Press YES on Gravity Chaos when your cube explodes!"
    },
    {
        "secret_level_10",
        10,
        "xStep",
        "Time Secret",
        "Warps game speed and music tempo in rhythm:\nPad: +20% Speed | Orb click: +10% Speed\nNormal jump: -5% Speed | Gravity portal: -30% Speed",
        "Step 1: Open Level 10 (xStep or Secretstep).\nStep 2: In the ball section, hit the two yellow pads\nand skip the yellow orb (between 13.25% and 13.55%).\nStep 3: Let your ball explode and press YES on Time Secret!"
    },
    {
        "secret_level_22",
        22,
        "Dash",
        "Ultimate Crash",
        "Smoothly stops the camera on the final section, then\nreplays it 10 times faster with a music fade-out before exiting!",
        "Step 1: Open Level 22 (Dash or Secret Dash).\nStep 2: Reach the very end of the level.\nStep 3: Press YES on the Ultimate Crash popup!"
    }
};

static bool isSecretUnlocked(const char* key) {
    return Mod::get()->getSavedValue<bool>(key, false);
}

static void unlockSecret(const char* key) {
    if (!Mod::get()->getSavedValue<bool>(key, false)) {
        Mod::get()->setSavedValue<bool>(key, true);
    }
}

static int getSecretLevelIndex(GJGameLevel* lvl) {
    if (!lvl) return 0;
    if (lvl->m_levelID == 1 || lvl->m_levelID == 2 || lvl->m_levelID == 10 || lvl->m_levelID == 22) {
        return lvl->m_levelID;
    }
    std::string name = lvl->m_levelName;
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    if (name.find("madness") != std::string::npos) return 1;
    if (name.find("track") != std::string::npos || lvl->m_songID == 1) return 2;
    if (name.find("step") != std::string::npos) return 10;
    if (name.find("dash") != std::string::npos) return 22;
    return 0;
}
static void applySpeedAndPitch(PlayLayer* pl, float mult) {
    if (!pl) return;
    pl->m_gameState.m_timeWarp = mult;
    if (auto fmod = FMODAudioEngine::sharedEngine()) {
        if (fmod->m_backgroundMusicChannel) {
            fmod->m_backgroundMusicChannel->setPitch(mult);
        }
    }
}

static void spawnXstepIntroBanner(PlayLayer* pl) {
    if (!pl) return;
    if (auto old = pl->getChildByTag(9910)) old->removeFromParent();

    auto win = CCDirector::sharedDirector()->getWinSize();
    auto container = CCNode::create();
    container->setTag(9910);
    container->ignoreAnchorPointForPosition(false);
    container->setAnchorPoint({ 0.5f, 0.5f });
    container->setContentSize({ 450.0f, 84.0f });
    container->setPosition({ win.width * 0.5f, win.height - 62.0f });
    pl->addChild(container, 120);

    auto bg = extension::CCScale9Sprite::create("square02b_001.png");
    bg->setColor({ 0, 0, 0 });
    bg->setOpacity(175);
    bg->setContentSize({ 450.0f, 84.0f });
    bg->setPosition({ 225.0f, 42.0f });
    container->addChild(bg);

    const char* values[] = { "-30% Speed", "+20% Speed", "+10% Speed", "-5% Speed" };
    ccColor3B colors[] = {
        { 100, 200, 255 },
        { 120, 255, 140 },
        { 255, 230, 90 },
        { 255, 140, 140 }
    };

    for (int i = 0; i < 4; ++i) {
        float cx = 60.0f + i * 110.0f;
        CCNode* iconNode = nullptr;
        if (i == 0) {
            auto ring = CCSprite::createWithSpriteFrameName("portal_04_front_001.png");
            auto back = CCSprite::createWithSpriteFrameName("portal_04_back_001.png");
            if (!ring) ring = CCSprite::createWithSpriteFrameName("portal_03_front_001.png");
            if (ring) {
                auto wrap = CCNode::create();
                if (back) { back->setScale(0.45f); wrap->addChild(back, 0); }
                ring->setScale(0.45f);
                wrap->addChild(ring, 1);
                iconNode = wrap;
            }
        } else if (i == 1) {
            auto spr = CCSprite::createWithSpriteFrameName("bump_01_001.png");
            if (spr) { spr->setScale(0.65f); iconNode = spr; }
        } else if (i == 2) {
            auto spr = CCSprite::createWithSpriteFrameName("ring_01_001.png");
            if (spr) { spr->setScale(0.55f); iconNode = spr; }
        } else {
            auto jumpLbl = CCLabelBMFont::create("Every jump", "bigFont.fnt");
            jumpLbl->setScale(0.32f);
            iconNode = jumpLbl;
        }
        if (iconNode) {
            iconNode->setPosition({ cx, 56.0f });
            container->addChild(iconNode);
        }
        auto valLbl = CCLabelBMFont::create(values[i], "goldFont.fnt");
        valLbl->setScale(0.42f);
        valLbl->setColor(colors[i]);
        valLbl->setPosition({ cx, 24.0f });
        container->addChild(valLbl);
    }

    container->setScale(0.0f);
    container->runAction(CCSequence::create(
        CCEaseBackOut::create(CCScaleTo::create(0.25f, 1.0f)),
        CCDelayTime::create(2.43f),
        CCEaseBackIn::create(CCScaleTo::create(0.32f, 0.0f)),
        CCRemoveSelf::create(),
        nullptr
    ));
}

class SecretDetailPopup : public FLAlertLayer {
protected:
    bool init(int idx) {
        if (!FLAlertLayer::init(180)) return false;
        const auto& s = g_implementedSecrets[idx];
        bool unlocked = isSecretUnlocked(s.key);
        auto win = CCDirector::sharedDirector()->getWinSize();

        float boxW = win.width - 60.0f;
        float boxH = win.height - 40.0f;

        auto bg = extension::CCScale9Sprite::create("GJ_square01.png");
        bg->setContentSize({ boxW, boxH });
        bg->setPosition({ win.width * 0.5f, win.height * 0.5f });
        m_mainLayer->addChild(bg);

        auto headerLbl = CCLabelBMFont::create(fmt::format("Level {}: {}", s.levelNum, s.levelName).c_str(), "goldFont.fnt");
        headerLbl->setPosition({ win.width * 0.5f, win.height * 0.5f + boxH * 0.5f - 22.0f });
        headerLbl->setScale(0.78f);
        m_mainLayer->addChild(headerLbl);

        auto subLbl = CCLabelBMFont::create(unlocked ? s.secretTitle : "Locked Secret", "bigFont.fnt");
        subLbl->setPosition({ win.width * 0.5f, win.height * 0.5f + boxH * 0.5f - 45.0f });
        subLbl->setScale(0.46f);
        subLbl->setColor(unlocked ? ccColor3B{ 120, 255, 140 } : ccColor3B{ 255, 210, 90 });
        m_mainLayer->addChild(subLbl);

        float innerW = boxW - 44.0f;
        float innerH = boxH - 82.0f;

        auto innerBox = extension::CCScale9Sprite::create("square02b_001.png");
        innerBox->setColor({ 0, 0, 0 });
        innerBox->setOpacity(110);
        innerBox->setContentSize({ innerW, innerH });
        innerBox->setPosition({ win.width * 0.5f, win.height * 0.5f - 18.0f });
        m_mainLayer->addChild(innerBox);

        std::string fullText = fmt::format(
            "STATUS: {}\n\nWHAT IT IS:\n{}\n\nUSER MANUAL (HOW TO FIND IT):\n{}",
            unlocked ? "DISCOVERED!" : "NOT DISCOVERED YET",
            unlocked ? s.whatItIs : "Discover this secret in-game to unlock its full description!",
            s.howToFind
        );

        auto textLbl = CCLabelBMFont::create(fullText.c_str(), "chatFont.fnt");
        textLbl->setScale(0.52f);
        textLbl->setAlignment(kCCTextAlignmentLeft);
        textLbl->setAnchorPoint({ 0.0f, 1.0f });
        textLbl->setPosition({ win.width * 0.5f - innerW * 0.5f + 14.0f, win.height * 0.5f - 18.0f + innerH * 0.5f - 10.0f });
        m_mainLayer->addChild(textLbl);

        auto menu = CCMenu::create();
        menu->setPosition({ 0.0f, 0.0f });
        m_mainLayer->addChild(menu);

        auto closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
        auto closeBtn = CCMenuItemSpriteExtra::create(closeSpr, this, menu_selector(SecretDetailPopup::onClose));
        closeBtn->setPosition({ win.width * 0.5f - boxW * 0.5f + 8.0f, win.height * 0.5f + boxH * 0.5f - 8.0f });
        menu->addChild(closeBtn);
        return true;
    }

    void onClose(CCObject*) { this->keyBackClicked(); }
public:
    static SecretDetailPopup* create(int idx) {
        auto ret = new SecretDetailPopup();
        if (ret && ret->init(idx)) { ret->autorelease(); return ret; }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};
class SecretsCodexPopup : public FLAlertLayer {
protected:
    bool init() {
        if (!FLAlertLayer::init(180)) return false;
        int total = static_cast<int>(g_implementedSecrets.size());
        auto win = CCDirector::sharedDirector()->getWinSize();

        float boxW = win.width - 70.0f;
        float boxH = win.height - 52.0f;

        auto bg = extension::CCScale9Sprite::create("GJ_square01.png");
        bg->setContentSize({ boxW, boxH });
        bg->setPosition({ win.width * 0.5f, win.height * 0.5f });
        m_mainLayer->addChild(bg);

        auto titleLbl = CCLabelBMFont::create("Secrets List", "goldFont.fnt");
        titleLbl->setPosition({ win.width * 0.5f, win.height * 0.5f + boxH * 0.5f - 22.0f });
        titleLbl->setScale(0.82f);
        m_mainLayer->addChild(titleLbl);

        float listW = boxW - 44.0f;
        float listH = boxH - 76.0f;

        auto listBg = extension::CCScale9Sprite::create("square02b_001.png");
        listBg->setColor({ 0, 0, 0 });
        listBg->setOpacity(95);
        listBg->setContentSize({ listW, listH });
        listBg->setPosition({ win.width * 0.5f, win.height * 0.5f - 4.0f });
        m_mainLayer->addChild(listBg);

        auto listMenu = CCMenu::create();
        listMenu->setPosition({ win.width * 0.5f, win.height * 0.5f - 4.0f });
        m_mainLayer->addChild(listMenu);

        for (int i = 0; i < total; ++i) {
            const auto& s = g_implementedSecrets[i];
            bool unlocked = isSecretUnlocked(s.key);
            std::string btnText = fmt::format(
                "Level {}: {} - {} [{}]",
                s.levelNum,
                s.levelName,
                unlocked ? s.secretTitle : "Locked",
                unlocked ? "FOUND" : "LOCKED"
            );
            auto spr = ButtonSprite::create(btnText.c_str(), static_cast<int>(listW - 44.0f), true, "goldFont.fnt", unlocked ? "GJ_button_01.png" : "GJ_button_04.png", 26.0f, 0.48f);
            auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(SecretsCodexPopup::onSelectSecret));
            btn->setTag(i);
            btn->setPosition({ 0.0f, (listH * 0.5f) - 26.0f - (i * 34.0f) });
            listMenu->addChild(btn);
        }

        auto menu = CCMenu::create();
        menu->setPosition({ 0.0f, 0.0f });
        m_mainLayer->addChild(menu);

        auto closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
        auto closeBtn = CCMenuItemSpriteExtra::create(closeSpr, this, menu_selector(SecretsCodexPopup::onClose));
        closeBtn->setPosition({ win.width * 0.5f - boxW * 0.5f + 8.0f, win.height * 0.5f + boxH * 0.5f - 8.0f });
        menu->addChild(closeBtn);

        auto tip = CCLabelBMFont::create("Click any level to read the full user manual!", "chatFont.fnt");
        tip->setPosition({ win.width * 0.5f, win.height * 0.5f - boxH * 0.5f + 16.0f });
        tip->setScale(0.76f);
        m_mainLayer->addChild(tip);
        return true;
    }

    void onClose(CCObject*) { this->keyBackClicked(); }
    void onSelectSecret(CCObject* sender) {
        if (auto popup = SecretDetailPopup::create(sender->getTag())) popup->show();
    }
public:
    static SecretsCodexPopup* create() {
        auto ret = new SecretsCodexPopup();
        if (ret && ret->init()) { ret->autorelease(); return ret; }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

class $modify(SecretPauseHook, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto win = CCDirector::sharedDirector()->getWinSize();

        auto menu = CCMenu::create();
        menu->setPosition({ 0.0f, 0.0f });
        this->addChild(menu);

        auto spr = ButtonSprite::create("Secrets", 84, true, "goldFont.fnt", "GJ_button_05.png", 20.0f, 0.52f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(SecretPauseHook::onOpenCodex));
        btn->setPosition({ win.width - 86.0f, 36.0f });
        menu->addChild(btn);
    }

    void onOpenCodex(CCObject*) {
        if (auto popup = SecretsCodexPopup::create()) popup->show();
    }
};

static bool g_orbActivated = false;
static bool g_reversePromptOpen = false;
static bool g_reverseActive = false;
static float g_reverseIntroTimer = 0.0f;
static float g_reverseProgress = 100.0f;
static float g_reverseStartX = 0.0f;

static bool g_botTriggeredThisDeath = false; static float g_lastBotPct = 0.0f;
static bool g_botPromptOpen = false;
static bool g_botActive = false;
static float g_botFlipTimer = 2.5f;

static bool g_xstepPromptOpen = false;
static bool g_xstepActive = false;
static float g_xstepDeathDelay = 0.0f;
static float g_xstepIntroTimer = 0.0f;
static float g_xstepTargetSpeed = 1.0f;
static float g_xstepCurrentSpeed = 1.0f;
static float g_xstepCooldown = 0.0f;

static bool g_dashArmTriggered = false;
static bool g_dashEnding = false;
static int g_dashLoopCount = 0;
static float g_dashTimer = 0.0f;
static float g_dashWaveStartX = 0.0f;
static float g_dashWaveStartY = 0.0f;
static float g_dashWaveEndX = 0.0f;

static void spawnDashToggleOrb(PlayLayer* pl) { if (!pl || !pl->m_objectLayer || pl->m_objectLayer->getChildByTag(8822)) return; auto orb = CCSprite::createWithSpriteFrameName("ring_01_001.png"); if (!orb) return; orb->setTag(8822); orb->setColor({ 255, 90, 90 }); orb->setScale(1.15f); orb->setPosition({ pl->m_player1->getPositionX() + 140.0f, pl->m_player1->getPositionY() + 45.0f }); pl->m_objectLayer->addChild(orb, 50); } 

class ReversePopupHandler : public FLAlertLayerProtocol {
public:
    void FLAlert_Clicked(FLAlertLayer*, bool btn2) override {
        g_reversePromptOpen = false;
        auto pl = PlayLayer::get();
        if (!pl) return;
        if (!btn2) { pl->m_player1->m_isLocked = false; pl->resetLevel(); return; }
        unlockSecret("secret_level_1");
        g_reverseActive = true;
        g_reverseIntroTimer = 0.95f;
        g_reverseProgress = 100.0f;
        g_reverseStartX = pl->m_player1->getPositionX();
        auto fmod = FMODAudioEngine::sharedEngine();
        fmod->stopAllMusic(true);
        std::string revSfx = Mod::get()->getResourcesDir().string() + "/endStart_reverse.ogg";
        fmod->playEffect(revSfx.c_str());
        ccBezierConfig bez;
        bez.controlPoint_1 = ccp(g_reverseStartX + 70.0f, pl->m_player1->getPositionY() + 65.0f);
        bez.controlPoint_2 = ccp(g_reverseStartX + 145.0f, 195.0f);
        bez.endPosition = ccp(g_reverseStartX + 165.0f, 185.0f);
        pl->m_player1->runAction(CCSequence::create(
            CCBezierTo::create(0.45f, bez),
            CCMoveTo::create(0.45f, ccp(g_reverseStartX - 35.0f, 185.0f)),
            nullptr
        ));
    }
};
static ReversePopupHandler g_reversePopupHandler;

class BotPopupHandler : public FLAlertLayerProtocol {
public:
    void FLAlert_Clicked(FLAlertLayer*, bool btn2) override {
        g_botPromptOpen = false;
        g_botTriggeredThisDeath = false;
        auto pl = PlayLayer::get();
        if (!pl) return;
        pl->resetLevel();
        if (!btn2) return;
        unlockSecret("secret_level_2");
        g_botActive = true;
        g_botFlipTimer = 2.5f;
    }
};
static BotPopupHandler g_botPopupHandler;

class XstepPopupHandler : public FLAlertLayerProtocol {
public:
    void FLAlert_Clicked(FLAlertLayer*, bool btn2) override {
        g_xstepPromptOpen = false;
        auto pl = PlayLayer::get();
        if (!pl) return;
        pl->resetLevel();
        if (!btn2) return;
        unlockSecret("secret_level_10");
        g_xstepActive = true;
        g_xstepIntroTimer = 3.0f;
        g_xstepTargetSpeed = 1.0f;
        g_xstepCurrentSpeed = 1.0f;
        applySpeedAndPitch(pl, 1.0f);
        FMODAudioEngine::sharedEngine()->pauseAllMusic(true);
        spawnXstepIntroBanner(pl);
    }
};
static XstepPopupHandler g_xstepPopupHandler;

class $modify(SecretPlayerHook, PlayerObject) { void playerDestroyed(bool p0) { PlayerObject::playerDestroyed(p0); auto pl = PlayLayer::get(); if (pl && this == pl->m_player1 && getSecretLevelIndex(pl->m_level) == 2 && !g_botActive && !g_botPromptOpen) { float pct = pl->getCurrentPercent(); if (pct >= 1.50f && pct <= 2.30f) { g_botTriggeredThisDeath = true; g_botPromptOpen = true; FMODAudioEngine::sharedEngine()->stopAllMusic(true); FLAlertLayer::create(&g_botPopupHandler, "Gravity Chaos", "Do you want to unleash Gravity Chaos?", "No", "Yes")->show(); } } } 
    void ringJump(RingObject* ring, bool p1) {
        auto plDash = PlayLayer::get();
        if (plDash && this == plDash->m_player1 && getSecretLevelIndex(plDash->m_level) == 22 && !g_dashArmTriggered && !g_dashEnding) { float px = this->getPositionX(); if (px >= 32500.0f && px <= 36500.0f) { g_dashArmTriggered = true; unlockSecret("secret_level_22"); FLAlertLayer::create("Ultimate Crash", "Trap Armed! Enter the wave!", "OK")->show(); } }
        PlayerObject::ringJump(ring, p1);
        auto pl = PlayLayer::get();
        if (!pl || this != pl->m_player1) return;
        int lvlIdx = getSecretLevelIndex(pl->m_level);
        if (lvlIdx == 1 && !g_orbActivated && !g_reversePromptOpen && !g_reverseActive) {
            if (this->getPositionX() > 24000.0f && this->getPositionY() > 210.0f) {
                g_orbActivated = true;
                g_reversePromptOpen = true;
                this->m_isLocked = true;
                FMODAudioEngine::sharedEngine()->pauseAllMusic(true);
                FLAlertLayer::create(&g_reversePopupHandler, "Reverse Secret", "Do you want to play in Reverse Mode?", "No", "Yes")->show();
            }
        }
        if (lvlIdx == 10 && g_xstepActive && g_xstepIntroTimer <= 0.0f && g_xstepCooldown <= 0.0f) {
            g_xstepCooldown = 0.14f;
            g_xstepTargetSpeed = std::min(2.20f, g_xstepTargetSpeed + 0.10f);
        }
    }
    void bumpPlayer(float p0, int p1, bool p2, GameObject* obj) {
        PlayerObject::bumpPlayer(p0, p1, p2, obj);
        auto pl = PlayLayer::get();
        if (pl && this == pl->m_player1 && getSecretLevelIndex(pl->m_level) == 10 && g_xstepActive && g_xstepIntroTimer <= 0.0f && g_xstepCooldown <= 0.0f) {
            g_xstepCooldown = 0.14f;
            g_xstepTargetSpeed = std::min(2.20f, g_xstepTargetSpeed + 0.20f);
        }
    }
    void pushButton(PlayerButton btn) {
        bool wasOnGround = this->m_isOnGround;
        PlayerObject::pushButton(btn);
        auto pl = PlayLayer::get();
        if (pl && this == pl->m_player1 && btn == PlayerButton::Jump && wasOnGround && getSecretLevelIndex(pl->m_level) == 10 && g_xstepActive && g_xstepIntroTimer <= 0.0f && g_xstepCooldown <= 0.0f) {
            g_xstepCooldown = 0.14f;
            g_xstepTargetSpeed = std::max(0.40f, g_xstepTargetSpeed - 0.05f);
        }
    }
    void flipGravity(bool flip, bool p1) {
        bool prev = this->m_isUpsideDown;
        PlayerObject::flipGravity(flip, p1);
        auto pl = PlayLayer::get();
        if (pl && this == pl->m_player1 && prev != flip && getSecretLevelIndex(pl->m_level) == 10 && g_xstepActive && g_xstepIntroTimer <= 0.0f && g_xstepCooldown <= 0.0f) {
            g_xstepCooldown = 0.14f;
            g_xstepTargetSpeed = std::max(0.40f, g_xstepTargetSpeed - 0.30f);
        }
    }
};
class $modify(HiddenSecretsMod, PlayLayer) {
    void resetLevel() {
        g_orbActivated = false;
        g_reversePromptOpen = false;
        g_reverseActive = false;
        g_reverseIntroTimer = 0.0f;
        if (!g_botPromptOpen) g_botTriggeredThisDeath = false;
        g_botFlipTimer = 2.5f;
        g_xstepPromptOpen = false;
        g_xstepDeathDelay = 0.0f;
        g_xstepIntroTimer = 0.0f;
        g_xstepTargetSpeed = 1.0f;
        g_xstepCurrentSpeed = 1.0f;
        g_xstepCooldown = 0.0f;
        g_dashArmTriggered = false; g_dashEnding = false; g_dashLoopCount = 0; g_dashTimer = 0.0f; g_dashWaveStartX = 0.0f; g_dashWaveEndX = 0.0f;
        this->setScale(1.0f);
        applySpeedAndPitch(this, 1.0f);
        PlayLayer::resetLevel();
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        if (player == this->m_player1 && getSecretLevelIndex(this->m_level) == 10 && !g_xstepActive && !g_xstepPromptOpen) {
            float pct = this->getCurrentPercent();
            if (pct >= 13.25f && pct <= 13.55f) {
                g_xstepPromptOpen = true;
                g_xstepDeathDelay = 0.65f;
            }
        }
        PlayLayer::destroyPlayer(player, obj);
    }

    void levelComplete() { PlayLayer::levelComplete(); }

    void update(float dt) {
        int lvlIdx = getSecretLevelIndex(this->m_level);

        if (lvlIdx == 2 && this->m_player1) { if (!this->m_player1->m_isDead) g_lastBotPct = this->getCurrentPercent();
            if (g_botPromptOpen) return;
            if (this->m_player1->m_isDead && !g_botActive && !g_botTriggeredThisDeath) {
                if (g_lastBotPct >= 1.65f && g_lastBotPct <= 2.05f) {
                    g_botTriggeredThisDeath = true;
                    g_botPromptOpen = true;
                    FMODAudioEngine::sharedEngine()->stopAllMusic(true);
                    FLAlertLayer::create(&g_botPopupHandler, "Gravity Chaos", "Do you want to unleash Gravity Chaos?", "No", "Yes")->show();
                    return;
                }
            } else if (!this->m_player1->m_isDead) { g_lastBotPct = this->getCurrentPercent(); if (g_botActive) {
                g_botFlipTimer -= dt;
                if (g_botFlipTimer <= 0.0f) {
                    g_botFlipTimer = 2.5f;
                    bool nf = !this->m_player1->m_isUpsideDown;
                    this->m_player1->flipGravity(nf, true);
                    FMODAudioEngine::sharedEngine()->playEffect("playSound_01.ogg");
                    auto fl = CCLayerColor::create(nf ? ccc4(0, 190, 255, 80) : ccc4(255, 110, 200, 80));
                    this->addChild(fl, 115);
                    fl->runAction(CCSequence::create(CCFadeOut::create(0.35f), CCRemoveSelf::create(), nullptr));
                }
            } }
        }

        if (lvlIdx == 1 && (g_reversePromptOpen || g_reverseActive)) {
            if (g_reversePromptOpen) { this->m_player1->m_isLocked = true; return; }
            if (g_reverseIntroTimer > 0.0f) {
                g_reverseIntroTimer -= dt;
                if (g_reverseIntroTimer <= 0.0f) {
                    std::string revSong = Mod::get()->getResourcesDir().string() + "/StereoMadness_reverse.mp3";
                    FMODAudioEngine::sharedEngine()->playMusic(revSong.c_str(), false, 1.0f, 0);
                }
                return;
            }
            float curX = this->m_player1->getPositionX() - (311.58f * dt);
            this->m_player1->setPositionX(curX);
            this->m_player1->setPositionY(std::clamp(this->m_player1->getPositionY(), 105.0f, 275.0f));
            this->m_gameState.m_cameraPosition.x = std::max(0.0f, curX - 110.0f);
            if (g_reverseStartX > 0.0f && this->m_percentageLabel) {
                g_reverseProgress = std::clamp((curX / g_reverseStartX) * 100.0f, 0.0f, 100.0f);
                this->m_percentageLabel->setString(fmt::format("{:.2f}%", g_reverseProgress).c_str());
            }
            if (curX <= 15.0f) { g_reverseActive = false; this->onQuit(); }
            return;
        }

        if (lvlIdx == 10) {
            if (g_xstepPromptOpen) {
                if (g_xstepDeathDelay > 0.0f) {
                    g_xstepDeathDelay -= dt;
                    if (g_xstepDeathDelay <= 0.0f) {
                        FMODAudioEngine::sharedEngine()->stopAllMusic(true);
                        FLAlertLayer::create(&g_xstepPopupHandler, "Time Secret", "Do you want to warp time?", "No", "Yes")->show();
                    }
                }
                return;
            }
            if (g_xstepActive) {
                if (g_xstepIntroTimer > 0.0f) {
                    g_xstepIntroTimer -= dt;
                    this->m_player1->m_isLocked = true;
                    if (g_xstepIntroTimer <= 0.0f) { this->m_player1->m_isLocked = false; FMODAudioEngine::sharedEngine()->resumeAllMusic(); }
                    return;
                }
                if (g_xstepCooldown > 0.0f) g_xstepCooldown -= dt;
                g_xstepCurrentSpeed += (g_xstepTargetSpeed - g_xstepCurrentSpeed) * std::min(1.0f, dt * 2.2f);
                applySpeedAndPitch(this, g_xstepCurrentSpeed);
            }
        }

                        if (lvlIdx == 22 && this->m_player1) {
            float px = this->m_player1->getPositionX();
            if (!g_dashArmTriggered && !g_dashEnding && px >= 33200.0f && px <= 34500.0f) {
                spawnDashToggleOrb(this);
            }
            if (g_dashArmTriggered && !g_dashEnding) {
                if (px >= 37800.0f && px <= 38300.0f && g_dashWaveStartX == 0.0f) {
                    g_dashWaveStartX = this->m_player1->getPositionX();
                    g_dashWaveStartY = this->m_player1->getPositionY();
                }
                if (px >= 39150.0f && g_dashWaveStartX > 0.0f) {
                    g_dashWaveEndX = this->m_player1->getPositionX();
                    g_dashEnding = true;
                    g_dashLoopCount = 1;
                    g_dashTimer = 0.0f;
                    this->m_player1->setPosition({ g_dashWaveStartX, g_dashWaveStartY });
                    this->m_gameState.m_cameraPosition.x = std::max(0.0f, g_dashWaveStartX - 110.0f);
                }
            }
            if (g_dashEnding) {
                this->m_player1->m_isLocked = true;
                g_dashTimer += dt;
                float speedMult = 1.0f + (g_dashLoopCount - 1) * 0.12f;
                float vol = std::max(0.0f, 1.0f - ((g_dashLoopCount - 1) / 10.0f));
                auto fmod = FMODAudioEngine::sharedEngine();
                if (fmod && fmod->m_backgroundMusicChannel) {
                    fmod->m_backgroundMusicChannel->setPitch(speedMult);
                    fmod->m_backgroundMusicChannel->setVolume(vol);
                }
                float curX = this->m_player1->getPositionX() + (370.0f * speedMult * dt);
                float waveY = g_dashWaveStartY + std::sin(g_dashTimer * 17.5f * speedMult) * 32.0f;
                this->m_player1->setPosition({ curX, waveY });
                this->m_gameState.m_cameraPosition.x = std::max(0.0f, curX - 130.0f);

                if (curX >= g_dashWaveEndX) {
                    g_dashLoopCount++;
                    g_dashTimer = 0.0f;
                    if (g_dashLoopCount > 10) {
                        g_dashEnding = false;
                        g_dashArmTriggered = false;
                        if (fmod) {
                            if (fmod->m_backgroundMusicChannel) { fmod->m_backgroundMusicChannel->setPitch(1.0f); fmod->m_backgroundMusicChannel->setVolume(1.0f); }
                            fmod->stopAllMusic(true);
                        }
                        this->onQuit();
                        return;
                    }
                    this->m_player1->setPosition({ g_dashWaveStartX, g_dashWaveStartY });
                    this->m_gameState.m_cameraPosition.x = std::max(0.0f, g_dashWaveStartX - 110.0f);
                    if (fmod) fmod->setMusicTimeMS(108000, true, 0);
                }
                return;
            }
        }

        PlayLayer::update(dt);
    }
};