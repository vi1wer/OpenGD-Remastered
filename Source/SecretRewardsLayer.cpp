/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#include "SecretRewardsLayer.h"

#include "AlertLayer.h"
#include "BoomScrollLayer.h"
#include "CreatorLayer.h"
#include "DialogLayer.h"
#include "GameManager.h"
#include "GJShopLayer.h"
#include "MenuItemSpriteExtra.h"
#include "MenuLayer.h"

#include "2d/ActionEase.h"
#include "2d/ActionInstant.h"
#include "2d/ActionInterval.h"
#include "2d/Label.h"
#include "2d/Layer.h"
#include "2d/Menu.h"
#include "2d/Sprite.h"
#include "2d/SpriteFrameCache.h"
#include "2d/Transition.h"
#include "AudioEngine.h"
#include "EventDispatcher.h"
#include "EventListenerKeyboard.h"
#include "GameToolbox/getTextureString.h"
#include "GameToolbox/nodes.h"
#include "base/Director.h"
#include "fmt/format.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

USING_NS_AX;

namespace
{
constexpr int kKeymasterIcon = 2;
constexpr int kKeymasterBg = 2;
constexpr int kGoldChestType = 10;
constexpr int kEntryCost = 5;

Sprite* frameSpr(const char* name)
{
	if (!name || !SpriteFrameCache::getInstance()->getSpriteFrameByName(name))
		return nullptr;
	auto* spr = Sprite::createWithSpriteFrameName(name);
	if (spr)
		spr->setStretchEnabled(false);
	return spr;
}

std::string stripDialogDelays(std::string text)
{
	// Official dialogs use <d035> delay tags — strip for our DialogLayer.
	for (;;)
	{
		const auto a = text.find("<d");
		if (a == std::string::npos)
			break;
		const auto b = text.find('>', a);
		if (b == std::string::npos)
			break;
		text.erase(a, b - a + 1);
	}
	return text;
}
} // namespace

const SecretRewardsLayer::ChestTier* SecretRewardsLayer::tiers()
{
	// Official 2.2 counts (wiki / SecretRewardsLayer).
	static constexpr ChestTier kTiers[] = {
		{1, 1, 400, "chestSpecial_01_price_001.png", 0.95f},
		{2, 5, 100, "chestSpecial_02_price_001.png", 0.95f},
		{3, 10, 60, "chestSpecial_03_price_001.png", 0.95f},
		{4, 25, 24, "chest_04_price_001.png", 0.82f},
		{5, 50, 12, "chest_05_price_001.png", 0.82f},
		{6, 100, 8, "chest_06_price_001.png", 0.82f},
	};
	return kTiers;
}

int SecretRewardsLayer::tierCount()
{
	return 6;
}

const SecretRewardsLayer::LargeChest* SecretRewardsLayer::largeChests()
{
	static constexpr LargeChest kLarge[] = {
		{7, 50, "chest_07_price_001.png"},
		{8, 100, "chest_08_price_001.png"},
		{9, 200, "chest_09_price_001.png"},
	};
	return kLarge;
}

const SecretRewardsLayer::ShopRope* SecretRewardsLayer::shopRopes()
{
	// Order matches wiki last screen.
	static constexpr ShopRope kRopes[] = {
		{0, 500, "shopRope2_001.png", "shopRope2_2_001.png", "Scratch", 3},
		{1, 200, "shopRope3_001.png", "shopRope3_2_001.png", "Potbor", 4},
		{2, 2000, "shopRope4_001.png", "shopRope4_2_001.png", "The Mechanic", 5},
		{3, 1000, "shopRope5_001.png", "shopRope5_2_001.png", "Diamond", 6},
	};
	return kRopes;
}

Color3B SecretRewardsLayer::pageColor(int page)
{
	// Geode inline SecretRewardsLayer::getPageColor
	if (page == 1)
		return {50, 50, 50};
	if (page == 2)
		return {70, 0, 120};
	return {70, 0, 75};
}

Scene* SecretRewardsLayer::scene(bool fromShop)
{
	return SecretRewardsLayer::create(fromShop);
}

SecretRewardsLayer* SecretRewardsLayer::create(bool fromShop)
{
	auto* ret = new (std::nothrow) SecretRewardsLayer();
	if (ret && ret->init(fromShop))
	{
		ret->autorelease();
		return ret;
	}
	delete ret;
	return nullptr;
}

Sprite* SecretRewardsLayer::makeClosedChestSprite(int typeId) const
{
	typeId = std::clamp(typeId, 1, 9);
	const std::string a = fmt::format("chest_0{}_01_001.png", typeId);
	const std::string b = fmt::format("chest_0{}_02_001.png", typeId);
	if (auto* spr = frameSpr(a.c_str()))
		return spr;
	if (auto* spr = frameSpr(b.c_str()))
		return spr;
	return frameSpr("chest_01_01_001.png");
}

Sprite* SecretRewardsLayer::makeOpenedChestSprite(int typeId) const
{
	typeId = std::clamp(typeId, 1, 9);
	const std::string a = fmt::format("chest_0{}_02_001.png", typeId);
	if (auto* spr = frameSpr(a.c_str()))
		return spr;
	return makeClosedChestSprite(typeId);
}

bool SecretRewardsLayer::init(bool fromShop)
{
	if (!Scene::init())
		return false;

	_fromShop = fromShop;
	auto* director = Director::getInstance();
	const auto& winSize = director->getWinSize();
	const Vec2 center = winSize / 2;

	GameToolbox::createBG(this, pageColor(0));
	_bg = dynamic_cast<Sprite*>(getChildByName("opengd_fullscreen_bg"));

	if (auto* ground = frameSpr("treasureRoomGround_001.png"))
	{
		ground->setAnchorPoint({0.5f, 0.f});
		ground->setPosition({center.x, 0.f});
		ground->setScaleX(winSize.width / std::max(ground->getContentSize().width, 1.f));
		addChild(ground, 1);
	}

	if (auto* web = frameSpr("treasureRoomSpiderweb_001.png"))
	{
		web->setAnchorPoint({1.f, 1.f});
		web->setPosition({winSize.width - 4.f, winSize.height - 4.f});
		web->setOpacity(180);
		addChild(web, 2);
	}

	if (auto* titleSpr = frameSpr("treasureRoomLabel_001.png"))
	{
		titleSpr->setPosition({center.x, winSize.height - 28.f});
		addChild(titleSpr, 8);
	}
	else
	{
		auto* titleLbl = Label::createWithBMFont(GameToolbox::getTextureString("goldFont.fnt"), "Treasure Room");
		titleLbl->setScale(0.7f);
		titleLbl->setPosition({center.x, winSize.height - 28.f});
		addChild(titleLbl, 8);
	}

	_hud = Node::create();
	addChild(_hud, 12);

	if (auto* keyIcon = frameSpr("GJ_bigKey_001.png"))
	{
		keyIcon->setScale(0.4f);
		keyIcon->setPosition({winSize.width - 18.f, winSize.height - 16.f});
		_hud->addChild(keyIcon);
	}
	_keysLabel = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "0");
	_keysLabel->setScale(0.4f);
	_keysLabel->setAnchorPoint({1.f, 0.5f});
	_keysLabel->setPosition({winSize.width - 34.f, winSize.height - 16.f});
	_hud->addChild(_keysLabel);

	if (auto* goldIcon = frameSpr("GJ_bigGoldKey_001.png"))
	{
		goldIcon->setScale(0.38f);
		goldIcon->setPosition({winSize.width - 18.f, winSize.height - 40.f});
		_hud->addChild(goldIcon);
	}
	_goldKeysLabel = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "0");
	_goldKeysLabel->setScale(0.38f);
	_goldKeysLabel->setAnchorPoint({1.f, 0.5f});
	_goldKeysLabel->setPosition({winSize.width - 34.f, winSize.height - 40.f});
	_hud->addChild(_goldKeysLabel);

	_openedLabel = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "0/604");
	_openedLabel->setScale(0.28f);
	_openedLabel->setAnchorPoint({0.f, 0.5f});
	_openedLabel->setPosition({10.f, winSize.height - 42.f});
	_hud->addChild(_openedLabel);

	_navMenu = Menu::create();
	_navMenu->setPosition({0, 0});
	addChild(_navMenu, 20);

	auto* backBtn = MenuItemSpriteExtra::create("GJ_arrow_01_001.png", [this](Node*) { goBack(); });
	backBtn->setPosition({24.f, winSize.height - 23.f});
	_navMenu->addChild(backBtn);

	_mainRoot = Node::create();
	addChild(_mainRoot, 5);
	_secondaryRoot = Node::create();
	_secondaryRoot->setVisible(false);
	addChild(_secondaryRoot, 6);

	auto* keys = EventListenerKeyboard::create();
	keys->onKeyPressed = AX_CALLBACK_2(SecretRewardsLayer::onKeyPressed, this);
	director->getEventDispatcher()->addEventListenerWithSceneGraphPriority(keys, this);

	AudioEngine::stopAll();
	AudioEngine::play2d("secretLoop.mp3", true, 0.25f);

	refreshHud();

	if (!GameManager::getInstance()->isTreasureRoomUnlocked())
		tryEnterRoom();
	else
		buildMainHub();

	return true;
}

void SecretRewardsLayer::setBgColor(Color3B color)
{
	if (_bg)
		_bg->setColor(color);
}

void SecretRewardsLayer::refreshHud()
{
	auto* gm = GameManager::getInstance();
	if (_keysLabel)
		_keysLabel->setString(std::to_string(gm->getDemonKeys()));
	if (_goldKeysLabel)
		_goldKeysLabel->setString(std::to_string(gm->getGoldKeys()));
	if (_openedLabel)
		_openedLabel->setString(fmt::format("{}/604", gm->countOpenedChests()));
}

void SecretRewardsLayer::showKeymasterLine(const std::string& text, std::function<void()> onClose)
{
	_dialog = DialogLayer::create({
		{"The Keymaster", stripDialogDelays(text), kKeymasterIcon},
	}, kKeymasterBg);
	if (!_dialog)
	{
		if (onClose)
			onClose();
		return;
	}
	_dialog->setChatPlacement(DialogChatPlacement::Center);
	_dialog->setAnimationType(DialogAnimationType::FromCenter);
	_dialog->setOnClose([this, onClose]() {
		_dialog = nullptr;
		if (onClose)
			onClose();
	});
	addChild(_dialog, 200);
	_dialog->show();
}

void SecretRewardsLayer::tryEnterRoom()
{
	auto* gm = GameManager::getInstance();
	if (gm->isTreasureRoomUnlocked())
	{
		buildMainHub();
		return;
	}
	if (gm->getDemonKeys() < kEntryCost)
		showLockedEntryDialog();
	else
		showWelcomeDialog();
}

void SecretRewardsLayer::showLockedEntryDialog()
{
	auto* gm = GameManager::getInstance();
	const int need = kEntryCost - gm->getDemonKeys();
	std::string line;
	if (gm->getDemonKeys() <= 0)
		line = "Bring me <cy>5</c> <cg>keys</c>, and I will let you pass.";
	else
		line = fmt::format("Collect <cy>{}</c> more <cg>key{}</c>, and I will let you pass.", need, need == 1 ? "" : "s");

	showKeymasterLine(line, [this]() { goBack(); });
}

void SecretRewardsLayer::showWelcomeDialog()
{
	_dialog = DialogLayer::create({
		{"The Keymaster", "Well, well, well.\nLook who it is.", kKeymasterIcon},
		{"The Keymaster", "I see you have the keys. But what comes next?", kKeymasterIcon},
		{"The Keymaster", "The door is open. Time to find out...", kKeymasterIcon},
	}, kKeymasterBg);
	if (!_dialog)
	{
		GameManager::getInstance()->unlockTreasureRoom();
		refreshHud();
		buildMainHub();
		return;
	}
	_dialog->setChatPlacement(DialogChatPlacement::Center);
	_dialog->setAnimationType(DialogAnimationType::FromCenter);
	_dialog->setOnClose([this]() {
		_dialog = nullptr;
		if (GameManager::getInstance()->unlockTreasureRoom())
		{
			refreshHud();
			buildMainHub();
		}
		else
			goBack();
	});
	addChild(_dialog, 200);
	_dialog->show();
}

void SecretRewardsLayer::buildMainHub()
{
	_inMainLayer = true;
	_viewTier = -1;
	_mainRoot->removeAllChildren();
	_secondaryRoot->removeAllChildren();
	_secondaryRoot->setVisible(false);
	_mainRoot->setVisible(true);
	_mainRoot->setPosition({0, 0});

	std::vector<Layer*> pages = {
		buildTierPage(false),
		buildTierPage(true),
		buildLargePage(),
		buildGoldPage(),
		buildShopsPage(),
	};

	_mainScroll = BoomScrollLayer::create(pages, _mainPage);
	if (!_mainScroll)
		return;
	_mainScroll->_onPageChanged = [this](int page) { onMainPageChanged(page); };
	_mainRoot->addChild(_mainScroll, 1);

	auto* menu = Menu::create();
	menu->setPosition({0, 0});
	_mainRoot->addChild(menu, 10);

	const auto& winSize = Director::getInstance()->getWinSize();
	const Vec2 center = winSize / 2;

	const char* arrowFrame = SpriteFrameCache::getInstance()->getSpriteFrameByName("GJ_arrow_03_001.png")
								 ? "GJ_arrow_03_001.png"
								 : "GJ_arrow_01_001.png";

	auto* leftSpr = frameSpr(arrowFrame);
	if (leftSpr)
	{
		leftSpr->setFlippedX(true);
		auto* left = MenuItemSpriteExtra::create(leftSpr, [this](Node*) {
			if (_mainScroll)
				_mainScroll->changePageLeft();
		});
		left->setScale(0.85f);
		left->setPosition({36.f, center.y});
		menu->addChild(left);
	}

	auto* right = MenuItemSpriteExtra::create(arrowFrame, [this](Node*) {
		if (_mainScroll)
			_mainScroll->changePageRight();
	});
	right->setScale(0.85f);
	right->setPosition({winSize.width - 36.f, center.y});
	menu->addChild(right);

	setBgColor(pageColor(_mainPage));
	refreshHud();
}

void SecretRewardsLayer::onMainPageChanged(int page)
{
	_mainPage = page;
	setBgColor(pageColor(page));
}

void SecretRewardsLayer::addTierChestButton(Layer* page, Menu* menu, int tierIndex, float x, float y)
{
	const auto& tier = tiers()[tierIndex];
	if (auto* platform = frameSpr("chestPlatform_01_001.png"))
	{
		platform->setPosition({x, y - 30.f});
		platform->setScale(0.85f);
		page->addChild(platform, 1);
	}

	auto* chest = makeClosedChestSprite(tier.typeId);
	if (!chest)
		return;
	chest->setScale(tier.scale);
	auto* btn = MenuItemSpriteExtra::create(chest, [this, tierIndex](Node*) { onTierPressed(tierIndex); });
	btn->setPosition({x, y});
	menu->addChild(btn);

	if (tier.priceFrame)
	{
		if (auto* price = frameSpr(tier.priceFrame))
		{
			price->setPosition({x, y - 52.f});
			price->setScale(0.85f);
			page->addChild(price, 4);
		}
	}
}

Layer* SecretRewardsLayer::buildTierPage(bool tier2)
{
	auto* page = Layer::create();
	const auto& winSize = Director::getInstance()->getWinSize();
	const Vec2 center = winSize / 2;

	const char* labelFrame = tier2 ? "tier2label_001.png" : "tier1label_001.png";
	if (auto* label = frameSpr(labelFrame))
	{
		label->setPosition({center.x, center.y + 118.f});
		page->addChild(label, 3);
	}

	auto* menu = Menu::create();
	menu->setPosition({0, 0});
	page->addChild(menu, 2);

	const int base = tier2 ? 3 : 0;
	for (int i = 0; i < 3; ++i)
	{
		const float x = center.x + (i - 1) * 130.f;
		const float y = center.y + 20.f;
		addTierChestButton(page, menu, base + i, x, y);
	}
	return page;
}

Layer* SecretRewardsLayer::buildLargePage()
{
	auto* page = Layer::create();
	const auto& winSize = Director::getInstance()->getWinSize();
	const Vec2 center = winSize / 2;

	auto* title = Label::createWithBMFont(GameToolbox::getTextureString("goldFont.fnt"), "Bonus Chests");
	title->setScale(0.55f);
	title->setPosition({center.x, center.y + 118.f});
	page->addChild(title, 3);

	auto* menu = Menu::create();
	menu->setPosition({0, 0});
	page->addChild(menu, 2);

	auto* gm = GameManager::getInstance();
	const int opened = gm->countOpenedChests();
	const auto* large = largeChests();
	for (int i = 0; i < 3; ++i)
	{
		const float x = center.x + (i - 1) * 130.f;
		const float y = center.y + 10.f;
		if (auto* platform = frameSpr("chestPlatform_01_001.png"))
		{
			platform->setPosition({x, y - 34.f});
			platform->setScale(0.9f);
			page->addChild(platform, 1);
		}

		const bool claimed = gm->isChestOpened(large[i].typeId, 0);
		auto* chest = claimed ? makeOpenedChestSprite(large[i].typeId) : makeClosedChestSprite(large[i].typeId);
		if (!chest)
			continue;
		chest->setScale(1.05f);
		if (!claimed && opened < large[i].requiredOpened)
			chest->setColor({90, 90, 90});

		auto* btn = MenuItemSpriteExtra::create(chest, [this, i](Node*) { onLargeChestPressed(i); });
		btn->setPosition({x, y});
		menu->addChild(btn);

		if (large[i].priceFrame)
		{
			if (auto* price = frameSpr(large[i].priceFrame))
			{
				price->setPosition({x, y - 58.f});
				price->setScale(0.8f);
				page->addChild(price, 4);
			}
		}

		auto* need = Label::createWithBMFont(
			GameToolbox::getTextureString("bigFont.fnt"),
			fmt::format("{}/{}", std::min(opened, large[i].requiredOpened), large[i].requiredOpened));
		need->setScale(0.28f);
		need->setPosition({x, y - 74.f});
		page->addChild(need, 5);
	}
	return page;
}

Layer* SecretRewardsLayer::buildGoldPage()
{
	auto* page = Layer::create();
	const auto& winSize = Director::getInstance()->getWinSize();
	const Vec2 center = winSize / 2;

	auto* title = Label::createWithBMFont(GameToolbox::getTextureString("goldFont.fnt"), "Gold Chests");
	title->setScale(0.55f);
	title->setPosition({center.x, center.y + 118.f});
	page->addChild(title, 3);

	auto* menu = Menu::create();
	menu->setPosition({0, 0});
	page->addChild(menu, 2);

	auto* gm = GameManager::getInstance();
	const int openedGold = gm->countOpenedChestsOfType(kGoldChestType);
	const int next = gm->nextUnopenedGoldChest();

	if (auto* platform = frameSpr("chestPlatform_01_001.png"))
	{
		platform->setPosition({center.x, center.y - 20.f});
		platform->setScale(1.1f);
		page->addChild(platform, 1);
	}

	auto* chest = (next < 0) ? makeOpenedChestSprite(2) : makeClosedChestSprite(2);
	if (chest)
	{
		chest->setScale(1.25f);
		if (next < 0)
			chest->setColor({120, 120, 120});
		auto* btn = MenuItemSpriteExtra::create(chest, [this](Node*) { onGoldChestPressed(); });
		btn->setPosition({center.x, center.y + 20.f});
		menu->addChild(btn);
	}

	if (auto* price = frameSpr("chest_03_price_001.png"))
	{
		price->setPosition({center.x, center.y - 48.f});
		page->addChild(price, 4);
	}

	auto* counter = Label::createWithBMFont(
		GameToolbox::getTextureString("bigFont.fnt"), fmt::format("{}/20", openedGold));
	counter->setScale(0.4f);
	counter->setPosition({center.x, center.y - 72.f});
	page->addChild(counter, 5);
	return page;
}

Layer* SecretRewardsLayer::buildShopsPage()
{
	auto* page = Layer::create();
	const auto& winSize = Director::getInstance()->getWinSize();
	const Vec2 center = winSize / 2;

	auto* title = Label::createWithBMFont(GameToolbox::getTextureString("goldFont.fnt"), "Secret Shops");
	title->setScale(0.5f);
	title->setPosition({center.x, center.y + 118.f});
	page->addChild(title, 3);

	auto* menu = Menu::create();
	menu->setPosition({0, 0});
	page->addChild(menu, 2);

	auto* gm = GameManager::getInstance();
	const auto* ropes = shopRopes();
	for (int i = 0; i < 4; ++i)
	{
		const float x = center.x + (i - 1.5f) * 95.f;
		const float y = center.y + 40.f;
		const bool unlocked = gm->isSecretShopUnlocked(ropes[i].shopIndex);
		const char* frame = unlocked ? ropes[i].ropeAltFrame : ropes[i].ropeFrame;
		auto* rope = frameSpr(frame);
		if (!rope)
			rope = frameSpr("shopRope_001.png");
		if (!rope)
			continue;
		rope->setScale(0.95f);
		if (!unlocked && gm->getDiamonds() < ropes[i].diamondCost)
			rope->setColor({140, 140, 140});

		auto* btn = MenuItemSpriteExtra::create(rope, [this, i](Node*) { onShopRopePressed(i); });
		btn->setPosition({x, y});
		menu->addChild(btn);

		auto* cost = Label::createWithBMFont(
			GameToolbox::getTextureString("bigFont.fnt"), fmt::format("{}", ropes[i].diamondCost));
		cost->setScale(0.26f);
		cost->setPosition({x, y - 70.f});
		page->addChild(cost, 4);
	}
	return page;
}

void SecretRewardsLayer::onTierPressed(int tierIndex)
{
	showSecondary(tierIndex);
}

void SecretRewardsLayer::showSecondary(int tierIndex)
{
	if (tierIndex < 0 || tierIndex >= tierCount())
		return;

	_inMainLayer = false;
	_viewTier = tierIndex;
	_secondaryPage = 0;

	const auto& winSize = Director::getInstance()->getWinSize();
	_mainRoot->runAction(EaseBounceOut::create(MoveTo::create(0.45f, {0.f, -winSize.height - 80.f})));
	_secondaryRoot->setVisible(true);
	_secondaryRoot->setPosition({0.f, winSize.height});
	_secondaryRoot->runAction(EaseBounceOut::create(MoveTo::create(0.45f, {0.f, 0.f})));
	rebuildSecondaryPage();
}

void SecretRewardsLayer::rebuildSecondaryPage()
{
	_secondaryRoot->removeAllChildren();
	const auto& winSize = Director::getInstance()->getWinSize();
	const Vec2 center = winSize / 2;
	const auto& tier = tiers()[_viewTier];

	auto* title = Label::createWithBMFont(
		GameToolbox::getTextureString("goldFont.fnt"), fmt::format("{} Key Chests", tier.keyCost));
	title->setScale(0.5f);
	title->setPosition({center.x, winSize.height - 52.f});
	_secondaryRoot->addChild(title, 3);

	auto* menu = Menu::create();
	menu->setPosition({0, 0});
	_secondaryRoot->addChild(menu, 2);

	const int pages = std::max(1, (tier.chestCount + kChestsPerPage - 1) / kChestsPerPage);
	_secondaryPage = std::clamp(_secondaryPage, 0, pages - 1);
	const int start = _secondaryPage * kChestsPerPage;
	const int end = std::min(start + kChestsPerPage, tier.chestCount);

	auto* gm = GameManager::getInstance();
	for (int i = start; i < end; ++i)
	{
		const int slot = i - start;
		const int col = slot % 4;
		const int row = slot / 4;
		const float x = center.x + (col - 1.5f) * 78.f;
		const float y = center.y + 28.f + (1.f - row) * 78.f;

		if (auto* platform = frameSpr("chestPlatform_01_001.png"))
		{
			platform->setPosition({x, y - 20.f});
			platform->setScale(0.5f);
			_secondaryRoot->addChild(platform, 1);
		}

		const bool opened = gm->isChestOpened(tier.typeId, i);
		auto* chest = opened ? makeOpenedChestSprite(tier.typeId) : makeClosedChestSprite(tier.typeId);
		if (!chest)
			continue;
		chest->setScale(0.5f);
		if (opened)
			chest->setColor({90, 90, 90});

		auto* btn = MenuItemSpriteExtra::create(chest, [this, i](Node*) { onChestPressed(_viewTier, i); });
		btn->setPosition({x, y});
		menu->addChild(btn);

		if (!opened)
		{
			auto* cost = Label::createWithBMFont(
				GameToolbox::getTextureString("bigFont.fnt"), fmt::format("{}", tier.keyCost));
			cost->setScale(0.24f);
			cost->setAnchorPoint({1.f, 0.5f});
			cost->setPosition({x + 2.f, y - 32.f});
			_secondaryRoot->addChild(cost, 4);
			if (auto* key = frameSpr("GJ_bigKey_001.png"))
			{
				key->setScale(0.18f);
				key->setAnchorPoint({0.f, 0.5f});
				key->setPosition({x + 4.f, y - 32.f});
				_secondaryRoot->addChild(key, 4);
			}
		}
	}

	auto* pageLbl = Label::createWithBMFont(
		GameToolbox::getTextureString("bigFont.fnt"), fmt::format("{}/{}", _secondaryPage + 1, pages));
	pageLbl->setScale(0.3f);
	pageLbl->setPosition({center.x, 28.f});
	_secondaryRoot->addChild(pageLbl, 5);

	if (pages > 1)
	{
		if (_secondaryPage > 0)
		{
			auto* leftSpr = frameSpr("GJ_arrow_01_001.png");
			if (leftSpr)
			{
				leftSpr->setFlippedX(true);
				auto* left = MenuItemSpriteExtra::create(leftSpr, [this](Node*) {
					_secondaryPage--;
					rebuildSecondaryPage();
				});
				left->setScale(0.7f);
				left->setPosition({40.f, center.y});
				menu->addChild(left);
			}
		}
		if (_secondaryPage < pages - 1)
		{
			auto* right = MenuItemSpriteExtra::create("GJ_arrow_01_001.png", [this](Node*) {
				_secondaryPage++;
				rebuildSecondaryPage();
			});
			right->setScale(0.7f);
			right->setPosition({winSize.width - 40.f, center.y});
			menu->addChild(right);
		}
	}
}

void SecretRewardsLayer::onChestPressed(int tierIndex, int chestIndex)
{
	if (_dialog)
		return;
	if (tierIndex < 0 || tierIndex >= tierCount())
		return;

	const auto& tier = tiers()[tierIndex];
	auto* gm = GameManager::getInstance();
	if (gm->isChestOpened(tier.typeId, chestIndex))
		return;

	if (gm->getDemonKeys() < tier.keyCost)
	{
		const char* msg = (tier.keyCost <= 1) ? "You need a <cy>key</c> to unlock this <cg>chest</c>."
											  : "You need more <cy>keys</c> to unlock this <cg>chest</c>.";
		showKeymasterLine(msg);
		return;
	}

	auto* alert = AlertLayer::create(
		"Open Chest",
		fmt::format("Open this chest for {} Demon Keys?", tier.keyCost),
		"Cancel",
		"Open",
		nullptr,
		nullptr);
	if (!alert)
		return;
	alert->setBtn2Callback([this, alert, tier, chestIndex](Node*) {
		alert->close();
		openChestReward(tier.typeId, chestIndex, tier.keyCost, false);
	});
	alert->show();
}

void SecretRewardsLayer::onLargeChestPressed(int largeIndex)
{
	if (_dialog || largeIndex < 0 || largeIndex > 2)
		return;
	const auto& chest = largeChests()[largeIndex];
	auto* gm = GameManager::getInstance();
	if (gm->isChestOpened(chest.typeId, 0))
		return;

	const int opened = gm->countOpenedChests();
	if (opened < chest.requiredOpened)
	{
		const int need = chest.requiredOpened - opened;
		showKeymasterLine(fmt::format(
			"Unlock <cy>{}</c> more <cg>Chest{}</c>, and you can collect this bonus.", need, need == 1 ? "" : "s"));
		return;
	}

	openChestReward(chest.typeId, 0, 0, false);
}

void SecretRewardsLayer::onGoldChestPressed()
{
	if (_dialog)
		return;
	auto* gm = GameManager::getInstance();
	const int next = gm->nextUnopenedGoldChest();
	if (next < 0)
	{
		showKeymasterLine("All out of <cy>Gold Chests</c> right now. Come back later.");
		return;
	}
	if (gm->getGoldKeys() < 1)
	{
		showKeymasterLine("You need a <cy>Gold Key</c> to unlock this <cg>chest</c>.");
		return;
	}
	openChestReward(kGoldChestType, next, 1, true);
}

void SecretRewardsLayer::onShopRopePressed(int shopIndex)
{
	if (_dialog || shopIndex < 0 || shopIndex > 3)
		return;

	const auto& rope = shopRopes()[shopIndex];
	auto* gm = GameManager::getInstance();
	const int diamonds = gm->getDiamonds();

	if (gm->isSecretShopUnlocked(rope.shopIndex) || diamonds >= rope.diamondCost)
	{
		if (!gm->isSecretShopUnlocked(rope.shopIndex))
			gm->unlockSecretShop(rope.shopIndex);

		std::vector<DialogPage> pages;
		if (rope.shopIndex == 0)
		{
			pages = {
				{"Scratch", "Oh, the diamonds. You found them.", rope.dialogIcon},
				{"Scratch", "Alright come in, quickly before someone sees you.", rope.dialogIcon},
			};
		}
		else
		{
			pages = {{rope.keeperName, "Alright, come in.", rope.dialogIcon}};
		}

		_dialog = DialogLayer::create(pages, kKeymasterBg);
		if (!_dialog)
		{
			Director::getInstance()->replaceScene(TransitionFade::create(0.5f, GJShopLayer::scene()));
			return;
		}
		_dialog->setChatPlacement(DialogChatPlacement::Center);
		_dialog->setOnClose([this]() {
			_dialog = nullptr;
			Director::getInstance()->replaceScene(TransitionFade::create(0.5f, GJShopLayer::scene()));
		});
		addChild(_dialog, 200);
		_dialog->show();
		return;
	}

	// Locked poke dialogs
	std::string line;
	std::string speaker = rope.keeperName;
	int icon = rope.dialogIcon;
	if (rope.shopIndex == 0)
	{
		static const char* kLines[] = {
			"Uhm, there is no rope.",
			"Just... Pretend this never happened.",
			"...",
			"I'm gonna need you to stop doing that.",
			"If RubRub sees this I am in BIG trouble.",
			"Collect <cy>500</c> <cl>Diamonds</c> and I will let you in.",
		};
		line = kLines[std::min(_scratchDialogIndex, 5)];
		if (_scratchDialogIndex < 5)
			_scratchDialogIndex++;
	}
	else if (rope.shopIndex == 1)
	{
		static const char* kLines[] = {
			"Collect more diamonds...",
			"Collect <cy>200</c> <cl>Diamonds</c> so I know you're legit, then we can talk.",
		};
		line = kLines[std::min(_potborDialogIndex, 1)];
		if (_potborDialogIndex < 1)
			_potborDialogIndex++;
	}
	else
	{
		line = fmt::format("Collect <cy>{}</c> <cl>Diamonds</c> and I will let you in.", rope.diamondCost);
	}

	_dialog = DialogLayer::create({{speaker, stripDialogDelays(line), icon}}, kKeymasterBg);
	if (!_dialog)
		return;
	_dialog->setChatPlacement(DialogChatPlacement::Center);
	_dialog->setOnClose([this]() { _dialog = nullptr; });
	addChild(_dialog, 200);
	_dialog->show();
}

SecretRewardsLayer::ChestReward SecretRewardsLayer::rewardForChest(int typeId, int chestIndex) const
{
	// Deterministic offline rewards approximating official loot pools.
	ChestReward r;
	switch (typeId)
	{
	case 1: {
		static const int kOrbs[] = {50, 100, 200, 300, 500, 1000};
		static const int kDiamonds[] = {5, 10, 20, 30, 50};
		r.orbs = kOrbs[chestIndex % 6];
		if (chestIndex % 5 == 0)
			r.diamonds = kDiamonds[chestIndex % 5];
		break;
	}
	case 2:
		r.orbs = 500 + (chestIndex % 5) * 500;
		r.diamonds = 10 + (chestIndex % 4) * 10;
		break;
	case 3:
		r.orbs = 1500 + (chestIndex % 3) * 500;
		r.diamonds = 15 + (chestIndex % 4) * 10;
		break;
	case 4:
		r.orbs = 2000;
		r.diamonds = (chestIndex % 2 == 0) ? 100 : 50;
		break;
	case 5:
		r.orbs = 3000;
		r.diamonds = 100;
		break;
	case 6:
		r.orbs = 5000;
		r.diamonds = 500;
		break;
	case 7:
		r.orbs = 2500;
		r.diamonds = 50;
		r.label = "Bonus + Green Key";
		break;
	case 8:
		r.orbs = 5000;
		r.diamonds = 100;
		break;
	case 9:
		r.orbs = 10000;
		r.diamonds = 200;
		break;
	case kGoldChestType: {
		static const int kGoldOrbs[] = {0, 1000, 0, 0, 1500, 2000, 1000, 0, 0, 2000, 2000, 0, 0, 0, 0, 2000, 1000, 2000, 0, 0};
		static const int kGoldDiamonds[] = {0, 0, 20, 30, 15, 0, 10, 30, 30, 0, 0, 30, 30, 30, 30, 0, 10, 0, 30, 30};
		r.orbs = kGoldOrbs[chestIndex % 20];
		r.diamonds = kGoldDiamonds[chestIndex % 20];
		if (r.orbs == 0 && r.diamonds == 0)
		{
			r.orbs = 1000;
			r.diamonds = 20;
		}
		break;
	}
	default:
		r.orbs = 100;
		break;
	}
	return r;
}

void SecretRewardsLayer::openChestReward(int typeId, int chestIndex, int keyCost, bool gold)
{
	auto* gm = GameManager::getInstance();
	if (gold)
	{
		if (!gm->spendGoldKeys(keyCost))
			return;
	}
	else if (keyCost > 0)
	{
		if (!gm->spendDemonKeys(keyCost))
			return;
	}

	gm->markChestOpened(typeId, chestIndex);
	const ChestReward reward = rewardForChest(typeId, chestIndex);
	if (reward.orbs > 0)
		gm->addOrbs(reward.orbs);
	if (reward.diamonds > 0)
		gm->addDiamonds(reward.diamonds);
	gm->save();

	refreshHud();
	AudioEngine::play2d("chestOpen01.ogg", false, 0.5f);

	std::string body = fmt::format("You found {} Mana Orbs\nand {} Diamonds!", reward.orbs, reward.diamonds);
	if (reward.label)
		body = fmt::format("{}\n{}", body, reward.label);
	if (auto* alert = AlertLayer::create("Chest Opened!", body))
		alert->show();

	if (_inMainLayer)
		buildMainHub();
	else
		rebuildSecondaryPage();
}

void SecretRewardsLayer::goBack()
{
	if (_dialog)
		return;

	if (!_inMainLayer)
	{
		_inMainLayer = true;
		_viewTier = -1;
		const auto& winSize = Director::getInstance()->getWinSize();
		_secondaryRoot->runAction(Sequence::create(
			EaseInOut::create(MoveTo::create(0.4f, {0.f, winSize.height + 50.f}), 2.f),
			CallFunc::create([this]() {
				_secondaryRoot->removeAllChildren();
				_secondaryRoot->setVisible(false);
			}),
			nullptr));
		_mainRoot->runAction(EaseInOut::create(MoveTo::create(0.4f, {0.f, 0.f}), 2.f));
		refreshHud();
		return;
	}

	AudioEngine::stopAll();
	AudioEngine::play2d("menuLoop.mp3", true, 0.2f);
	if (_fromShop)
		Director::getInstance()->replaceScene(TransitionFade::create(0.5f, GJShopLayer::scene()));
	else
		Director::getInstance()->replaceScene(TransitionFade::create(0.5f, CreatorLayer::scene()));
}

void SecretRewardsLayer::onKeyPressed(EventKeyboard::KeyCode keyCode, Event*)
{
	if (keyCode == EventKeyboard::KeyCode::KEY_BACK || keyCode == EventKeyboard::KeyCode::KEY_ESCAPE)
	{
		if (_dialog)
			return;
		goBack();
	}
}
