#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/LevelCell.hpp>

using namespace geode::prelude;

// ---------------------------------------------------------------- data

// diff: 0 = off(keep real), 1 Auto, 2 Easy, 3 Normal, 4 Hard, 5 Harder, 6 Insane,
//       7 Easy Demon, 8 Medium Demon, 9 Hard Demon, 10 Insane Demon, 11 Extreme Demon
// feature: 0 none, 1 featured, 2 epic, 3 legendary, 4 mythic
struct FakeData {
	bool enabled = false;
	int stars = 0;
	int diff = 0;
	int feature = 0;
	bool coins = false;
};

static const char* DIFF_NAMES[] = {
	"Keep Real", "Auto", "Easy", "Normal", "Hard", "Harder", "Insane",
	"Easy Demon", "Medium Demon", "Hard Demon", "Insane Demon", "Extreme Demon"
};
static const char* FEATURE_NAMES[] = { "None", "Featured", "Epic", "Legendary", "Mythic" };

static std::string keyFor(GJGameLevel* lvl) {
	return fmt::format("fake_{}", (int)lvl->m_levelID.value());
}

static FakeData loadFake(GJGameLevel* lvl) {
	FakeData d;
	auto key = keyFor(lvl);
	if (!Mod::get()->hasSavedValue(key)) return d;
	auto s = Mod::get()->getSavedValue<std::string>(key);
	int e = 0, c = 0;
	if (sscanf(s.c_str(), "%d,%d,%d,%d,%d", &e, &d.stars, &d.diff, &d.feature, &c) == 5) {
		d.enabled = e != 0;
		d.coins = c != 0;
	}
	return d;
}

static void saveFake(GJGameLevel* lvl, FakeData const& d) {
	Mod::get()->setSavedValue<std::string>(
		keyFor(lvl),
		fmt::format("{},{},{},{},{}", d.enabled ? 1 : 0, d.stars, d.diff, d.feature, d.coins ? 1 : 0)
	);
}

static void applyFake(GJGameLevel* lvl) {
	if (!lvl) return;
	auto d = loadFake(lvl);
	if (!d.enabled) return;

	lvl->m_stars = d.stars;
	lvl->m_coinsVerified = d.coins ? 1 : 0;

	// featured / epic / legendary / mythic (m_isEpic: 0 none, 1 epic, 2 legendary, 3 mythic)
	lvl->m_featured = d.feature >= 1 ? 1 : 0;
	lvl->m_isEpic = d.feature >= 2 ? d.feature - 1 : 0;

	if (d.diff != 0) {
		lvl->m_autoLevel = false;
		lvl->m_demon = 0;
		lvl->m_ratings = 10;
		if (d.diff == 1) {
			lvl->m_autoLevel = true;
			lvl->m_ratingsSum = 10;
		}
		else if (d.diff <= 6) {
			lvl->m_ratingsSum = 10 * (d.diff - 1); // easy=1 ... insane=5
		}
		else {
			static const int demonMap[] = { 3, 4, 0, 5, 6 }; // easy, medium, hard, insane, extreme
			lvl->m_demon = 1;
			lvl->m_demonDifficulty = demonMap[d.diff - 7];
			lvl->m_ratingsSum = 50;
		}
	}
}

// ---------------------------------------------------------------- popup

class FakeRatePopup : public Popup<GJGameLevel*> {
protected:
	GJGameLevel* m_lvl = nullptr;
	FakeData m_data;
	TextInput* m_starsInput = nullptr;
	CCLabelBMFont* m_diffLabel = nullptr;
	CCLabelBMFont* m_featLabel = nullptr;
	CCMenuItemToggler* m_coinToggle = nullptr;

	bool setup(GJGameLevel* level) override {
		m_lvl = level;
		m_data = loadFake(level);
		this->setTitle("Fake Rate");

		// stars
		m_starsInput = TextInput::create(70.f, "Stars", "bigFont.fnt");
		m_starsInput->setFilter("0123456789");
		m_starsInput->setMaxCharCount(2);
		m_starsInput->setString(std::to_string(m_data.enabled ? m_data.stars : (int)level->m_stars.value()));
		m_mainLayer->addChildAtPosition(m_starsInput, Anchor::Center, {0, 60});

		auto starsTxt = CCLabelBMFont::create("Stars (0-10)", "goldFont.fnt");
		starsTxt->setScale(0.5f);
		m_mainLayer->addChildAtPosition(starsTxt, Anchor::Center, {0, 85});

		// difficulty selector
		m_diffLabel = CCLabelBMFont::create(DIFF_NAMES[m_data.diff], "bigFont.fnt");
		m_diffLabel->setScale(0.5f);
		m_mainLayer->addChildAtPosition(m_diffLabel, Anchor::Center, {0, 20});
		addArrows({0, 20}, menu_selector(FakeRatePopup::onDiffPrev), menu_selector(FakeRatePopup::onDiffNext), 110.f);

		// feature selector
		m_featLabel = CCLabelBMFont::create(FEATURE_NAMES[m_data.feature], "bigFont.fnt");
		m_featLabel->setScale(0.5f);
		m_mainLayer->addChildAtPosition(m_featLabel, Anchor::Center, {0, -20});
		addArrows({0, -20}, menu_selector(FakeRatePopup::onFeatPrev), menu_selector(FakeRatePopup::onFeatNext), 110.f);

		// verified coins
		m_coinToggle = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(FakeRatePopup::onCoins), 0.7f);
		m_coinToggle->toggle(m_data.coins);
		m_buttonMenu->addChildAtPosition(m_coinToggle, Anchor::Center, {-60, -58});
		auto coinTxt = CCLabelBMFont::create("Verified Coins", "bigFont.fnt");
		coinTxt->setScale(0.4f);
		coinTxt->setAnchorPoint({0, 0.5f});
		m_mainLayer->addChildAtPosition(coinTxt, Anchor::Center, {-40, -58});

		// buttons
		auto apply = CCMenuItemSpriteExtra::create(
			ButtonSprite::create("Apply"), this, menu_selector(FakeRatePopup::onApply));
		m_buttonMenu->addChildAtPosition(apply, Anchor::Bottom, {50, 28});

		auto reset = CCMenuItemSpriteExtra::create(
			ButtonSprite::create("Reset"), this, menu_selector(FakeRatePopup::onReset));
		m_buttonMenu->addChildAtPosition(reset, Anchor::Bottom, {-50, 28});

		return true;
	}

	void addArrows(CCPoint pos, SEL_MenuHandler left, SEL_MenuHandler right, float spread) {
		auto l = CCSprite::createWithSpriteFrameName("edit_leftBtn_001.png");
		auto r = CCSprite::createWithSpriteFrameName("edit_rightBtn_001.png");
		m_buttonMenu->addChildAtPosition(CCMenuItemSpriteExtra::create(l, this, left), Anchor::Center, {pos.x - spread, pos.y});
		m_buttonMenu->addChildAtPosition(CCMenuItemSpriteExtra::create(r, this, right), Anchor::Center, {pos.x + spread, pos.y});
	}

	void onDiffPrev(CCObject*) { m_data.diff = (m_data.diff + 11) % 12; m_diffLabel->setString(DIFF_NAMES[m_data.diff]); }
	void onDiffNext(CCObject*) { m_data.diff = (m_data.diff + 1) % 12; m_diffLabel->setString(DIFF_NAMES[m_data.diff]); }
	void onFeatPrev(CCObject*) { m_data.feature = (m_data.feature + 4) % 5; m_featLabel->setString(FEATURE_NAMES[m_data.feature]); }
	void onFeatNext(CCObject*) { m_data.feature = (m_data.feature + 1) % 5; m_featLabel->setString(FEATURE_NAMES[m_data.feature]); }
	void onCoins(CCObject*) { m_data.coins = !m_coinToggle->isToggled(); }

	void refreshLevelPage() {
		this->onClose(nullptr);
		CCDirector::get()->replaceScene(
			CCTransitionFade::create(0.3f, LevelInfoLayer::scene(m_lvl, false)));
	}

	void onApply(CCObject*) {
		int stars = geode::utils::numFromString<int>(m_starsInput->getString()).unwrapOr(0);
		m_data.stars = std::clamp(stars, 0, 10);
		m_data.enabled = true;
		saveFake(m_lvl, m_data);
		applyFake(m_lvl);
		refreshLevelPage();
	}

	void onReset(CCObject*) {
		m_data = FakeData{};
		saveFake(m_lvl, m_data);
		Notification::create("Reset! Re-open the level from the server to see the real rating.",
			NotificationIcon::Info)->show();
		this->onClose(nullptr);
	}

public:
	static FakeRatePopup* create(GJGameLevel* level) {
		auto ret = new FakeRatePopup();
		if (ret->initAnchored(300.f, 240.f, level)) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}
};

// ---------------------------------------------------------------- hooks

class $modify(FakeRateInfoLayer, LevelInfoLayer) {
	bool init(GJGameLevel* level, bool challenge) {
		applyFake(level);
		if (!LevelInfoLayer::init(level, challenge)) return false;

		auto menu = CCMenu::create();
		menu->setPosition({0, 0});
		auto spr = ButtonSprite::create("Fake Rate");
		spr->setScale(0.7f);
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(FakeRateInfoLayer::onFakeRate));
		auto win = CCDirector::get()->getWinSize();
		btn->setPosition({win.width - 60.f, 30.f});
		menu->addChild(btn);
		this->addChild(menu, 10);
		return true;
	}

	void onFakeRate(CCObject*) {
		FakeRatePopup::create(m_level)->show();
	}
};

// makes fake ratings also show in level lists / search results
class $modify(FakeRateCell, LevelCell) {
	void loadFromLevel(GJGameLevel* level) {
		applyFake(level);
		LevelCell::loadFromLevel(level);
	}
};
