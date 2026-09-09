/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#include "GJShopLayer.h"

#include "AlertLayer.h"
#include "DialogLayer.h"
#include "GameManager.h"
#include "MenuItemSpriteExtra.h"
#include "ShopCatalog.h"
#include "SimplePlayer.h"

#include "2d/Label.h"
#include "2d/Menu.h"
#include "2d/Sprite.h"
#include "2d/SpriteFrameCache.h"
#include "AudioEngine.h"
#include "EventDispatcher.h"
#include "EventListenerKeyboard.h"
#include "EventListenerTouch.h"
#include "GameToolbox/getTextureString.h"
#include "GameToolbox/nodes.h"
#include "UTF8.h"
#include "base/Director.h"
#include "fmt/format.h"
#include "platform/FileUtils.h"
#include <algorithm>
#include <cmath>

USING_NS_AX;

namespace
{
// GeometryDash.exe GJShopLayer::init (ShopType::Normal) — design 480×320
constexpr float kDeskY = 95.f;
constexpr float kDeskScale = 0.96f;
constexpr float kKeeperXOff = 100.f;
constexpr float kKeeperY = 230.f;
constexpr float kSignXOff = -70.f;
constexpr float kSignYOff = 38.f;
constexpr float kGridY = 93.f;
constexpr float kItemSpacing = 60.f; // ListButtonBar offset
constexpr float kArrowOffset = 221.f;
constexpr float kOwnedR = 60.f;
constexpr float kOwnedG = 30.f;
constexpr float kOwnedB = 20.f;

Color3B ownedTint()
{
	return {static_cast<uint8_t>(kOwnedR), static_cast<uint8_t>(kOwnedG), static_cast<uint8_t>(kOwnedB)};
}
} // namespace

Scene* GJShopLayer::scene()
{
	return GJShopLayer::create();
}

GJShopLayer* GJShopLayer::create()
{
	auto* ret = new (std::nothrow) GJShopLayer();
	if (ret && ret->init())
	{
		ret->autorelease();
		return ret;
	}
	delete ret;
	return nullptr;
}

float GJShopLayer::iconScaleForType(IconType type) const
{
	// GJItemIcon::scaleForType (Normal shop), then ×1.1 on the store icon
	float base = 0.8f;
	switch (type)
	{
	case IconType::kIconTypeShip:
	case IconType::kIconTypeJetpack:
		base = 0.6f;
		break;
	case IconType::kIconTypeBall:
		base = 0.75f;
		break;
	case IconType::kIconTypeUfo:
		base = 0.68f;
		break;
	case IconType::kIconTypeRobot:
	case IconType::kIconTypeSpider:
		base = 0.65f;
		break;
	case IconType::kIconTypeSwing:
		base = 0.7f;
		break;
	case IconType::kIconTypeWave:
	case IconType::kIconTypeCube:
	case IconType::kIconTypeDeathEffect:
	default:
		base = 0.8f;
		break;
	}
	return base * 1.1f;
}

Node* GJShopLayer::buildShopkeeper() const
{
	auto* keeper = Node::create();
	auto addPart = [keeper](const char* frame, const Vec2& pos, int z, bool flipX = false) {
		auto* spr = Sprite::createWithSpriteFrameName(frame);
		if (!spr)
			return;
		spr->setStretchEnabled(false);
		spr->setPosition(pos);
		spr->setFlippedX(flipX);
		keeper->addChild(spr, z);
	};

	// Idle pose from GJShopKeeper_AnimDesc.plist (both hands unflipped in official)
	addPart("shopKeeper_torso_01_001.png", {0.f, -20.5f}, 0);
	addPart("shopKeeper_head_01_001.png", {0.f, 25.5f}, 1);
	addPart("shopKeeper_eye_01_001.png", {-15.f, 25.f}, 2);
	addPart("shopKeeper_eye_01_001.png", {15.f, 25.f}, 3);
	addPart("shopKeeper_pupil_01_001.png", {-14.f, 23.f}, 4);
	addPart("shopKeeper_pupil_01_001.png", {14.f, 23.f}, 5);
	addPart("shopKeeper_jaw_01_001.png", {-0.025f, 0.5f}, 6);
	addPart("shopKeeper_hand_01_001.png", {-24.625f, -36.25f}, 7);
	addPart("shopKeeper_hand_01_001.png", {24.625f, -36.25f}, 8);
	keeper->setContentSize({80.f, 90.f});
	keeper->setAnchorPoint({0.5f, 0.5f});
	return keeper;
}

bool GJShopLayer::init()
{
	if (!Scene::init())
		return false;

	auto* director = Director::getInstance();
	const auto& winSize = director->getWinSize();
	const Vec2 center = winSize / 2;

	GameToolbox::createBG(this, {62, 38, 24});

	// Official: scale (win+5)/size, position (-2.5, -2.5), z = -2
	if (auto* bg = Sprite::createWithSpriteFrameName("shopBG_001.png"))
	{
		bg->setStretchEnabled(false);
		bg->setAnchorPoint({0.f, 0.f});
		bg->setPosition({-2.5f, -2.5f});
		bg->setScaleX((winSize.width + 5.f) / bg->getContentSize().width);
		bg->setScaleY((winSize.height + 5.f) / bg->getContentSize().height);
		addChild(bg, -2);
	}

	// Desk: (winW/2, 95), scale 0.96 — not full-bleed stretch
	if (auto* desk = Sprite::createWithSpriteFrameName("storeDesk_001.png"))
	{
		desk->setStretchEnabled(false);
		desk->setPosition({center.x, kDeskY});
		desk->setScale(kDeskScale);
		addChild(desk, 4);
	}

	_shopkeeper = buildShopkeeper();
	if (_shopkeeper)
	{
		_shopkeeper->setPosition({center.x + kKeeperXOff, kKeeperY});
		addChild(_shopkeeper, 3);

		auto* tap = EventListenerTouchOneByOne::create();
		tap->setSwallowTouches(false);
		tap->onTouchBegan = [this](Touch* touch, Event*) {
			if (!_shopkeeper || _activeDialog)
				return false;
			// Official hitbox ~80×90 around the keeper
			const Vec2 local = _shopkeeper->convertToNodeSpace(touch->getLocation());
			if (std::abs(local.x) < 40.f && local.y > -35.f && local.y < 55.f)
			{
				showReactMessage();
				return true;
			}
			return false;
		};
		_eventDispatcher->addEventListenerWithSceneGraphPriority(tap, _shopkeeper);
	}

	// Sign: (winW/2 - 70, winH - 38)
	if (auto* sign = Sprite::createWithSpriteFrameName("shopSign_001.png"))
	{
		sign->setStretchEnabled(false);
		sign->setPosition({center.x + kSignXOff, winSize.height - kSignYOff});
		addChild(sign, 8);
	}

	// Plushies (Normal shop only) — decorative / credits hook
	if (SpriteFrameCache::getInstance()->getSpriteFrameByName("plushies_001.png"))
	{
		if (auto* plush = Sprite::createWithSpriteFrameName("plushies_001.png"))
		{
			plush->setStretchEnabled(false);
			plush->setScale(1.1f);
			plush->setPosition({center.x - 130.f, 120.f});
			addChild(plush, 5);
		}
	}

	auto* nav = Menu::create();
	nav->setPosition({0, 0});
	addChild(nav, 10);

	const char* backFrame = SpriteFrameCache::getInstance()->getSpriteFrameByName("GJ_arrow_03_001.png")
		? "GJ_arrow_03_001.png"
		: "GJ_arrow_01_001.png";
	auto* backBtn = MenuItemSpriteExtra::create(backFrame, [this](Node*) { goBack(); });
	backBtn->setPosition({24.f, winSize.height - 23.f});
	nav->addChild(backBtn);

	auto* leftSpr = Sprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
	if (!leftSpr)
		leftSpr = Sprite::createWithSpriteFrameName("navArrowBtn_001.png");
	if (leftSpr)
	{
		leftSpr->setFlippedX(true);
		_leftArrow = MenuItemSpriteExtra::create(leftSpr, [this](Node*) { setPage(_page - 1); });
		_leftArrow->setPosition({center.x - kArrowOffset, kGridY});
		_leftArrow->setScale(0.6f);
		nav->addChild(_leftArrow);
	}
	auto* rightSpr = Sprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
	if (!rightSpr)
		rightSpr = Sprite::createWithSpriteFrameName("navArrowBtn_001.png");
	if (rightSpr)
	{
		_rightArrow = MenuItemSpriteExtra::create(rightSpr, [this](Node*) { setPage(_page + 1); });
		_rightArrow->setPosition({center.x + kArrowOffset, kGridY});
		_rightArrow->setScale(0.6f);
		nav->addChild(_rightArrow);
	}

	// Currency HUD ≈ (winW - 34, winH - 15), scale 0.6
	_orbLabel = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "0");
	_orbLabel->setScale(0.6f);
	_orbLabel->setAnchorPoint({1.f, 0.5f});
	_orbLabel->setPosition({winSize.width - 34.f, winSize.height - 15.f});
	addChild(_orbLabel, 10);

	if (auto* orbIcon = Sprite::createWithSpriteFrameName("currencyOrbIcon_001.png"))
	{
		orbIcon->setScale(0.6f);
		orbIcon->setAnchorPoint({0.f, 0.5f});
		orbIcon->setPosition({winSize.width - 30.f, winSize.height - 15.f});
		// Official places icon relative to label; keep it just to the right of the numbers' left
		orbIcon->setPosition({winSize.width - 18.f, winSize.height - 15.f});
		orbIcon->setAnchorPoint({0.5f, 0.5f});
		addChild(orbIcon, 10);
	}
	refreshOrbLabel();

	rebuildItemGrid();

	if (FileUtils::getInstance()->isFileExist("shop.mp3")
		|| FileUtils::getInstance()->isFileExist(GameToolbox::getTextureString("shop.mp3")))
	{
		AudioEngine::stopAll();
		AudioEngine::play2d("shop.mp3", true, 0.2f);
	}

	auto* listener = EventListenerKeyboard::create();
	listener->onKeyPressed = AX_CALLBACK_2(GJShopLayer::onKeyPressed, this);
	director->getEventDispatcher()->addEventListenerWithSceneGraphPriority(listener, this);

	return true;
}

void GJShopLayer::onEnterTransitionDidFinish()
{
	Scene::onEnterTransitionDidFinish();
	if (!_activeDialog)
		showWelcomeDialog();
}

void GJShopLayer::showWelcomeDialog()
{
	if (_activeDialog)
		return;

	// Official first-visit flavor (showReactMessage), Scratch portrait = dialogIcon_005
	_activeDialog = DialogLayer::create({
		{"Scratch", "Hey, <cg>Welcome</c> back!", 5},
	}, 2);
	if (!_activeDialog)
		return;
	_activeDialog->setChatPlacement(DialogChatPlacement::Center);
	_activeDialog->setAnimationType(DialogAnimationType::FromCenter);
	_activeDialog->setOnClose([this]() { _activeDialog = nullptr; });
	addChild(_activeDialog, 200);
	_activeDialog->show();
}

void GJShopLayer::showReactMessage()
{
	if (_activeDialog)
		return;

	static const char* kReactLines[] = {
		"Can I <cg>help</c> you?",
		"What do you <cy>want</c>?",
		"Can you <co>stop</c>?",
		"<cr>Stop</c> with the <cg>poking</c>!",
		"You're getting on my <cr>nerves</c>...",
		"<cr>...BEGONE!</c>",
	};
	const int n = static_cast<int>(sizeof(kReactLines) / sizeof(kReactLines[0]));
	const int idx = std::min(_reactIndex, n - 1);
	_reactIndex = std::min(_reactIndex + 1, n - 1);

	_activeDialog = DialogLayer::create({
		{"Scratch", kReactLines[idx], 5},
	}, 2);
	if (!_activeDialog)
		return;
	_activeDialog->setChatPlacement(DialogChatPlacement::Center);
	_activeDialog->setAnimationType(DialogAnimationType::FromCenter);
	_activeDialog->setOnClose([this]() { _activeDialog = nullptr; });
	addChild(_activeDialog, 200);
	_activeDialog->show();
}

void GJShopLayer::showCantAffordDialog()
{
	if (_activeDialog)
		return;
	_activeDialog = DialogLayer::create({
		{"Scratch", "You cannot <cg>afford</c> that <cl>item</c>.", 5},
		{"Scratch", "You <co>do not</c> have enough <cl>Mana Orbs</c>.", 5},
	}, 2);
	if (!_activeDialog)
		return;
	_activeDialog->setChatPlacement(DialogChatPlacement::Center);
	_activeDialog->setAnimationType(DialogAnimationType::FromCenter);
	_activeDialog->setOnClose([this]() { _activeDialog = nullptr; });
	addChild(_activeDialog, 200);
	_activeDialog->show();
}

void GJShopLayer::refreshOrbLabel()
{
	if (!_orbLabel)
		return;
	_orbLabel->setString(std::to_string(GameManager::getInstance()->getOrbs()));
}

int GJShopLayer::pageCount() const
{
	return std::max(1, (ShopCatalog::itemCount() + kItemsPerPage - 1) / kItemsPerPage);
}

void GJShopLayer::setPage(int page)
{
	_page = std::clamp(page, 0, pageCount() - 1);
	rebuildItemGrid();
}

void GJShopLayer::rebuildItemGrid()
{
	if (_itemMenu)
	{
		_itemMenu->removeFromParent();
		_itemMenu = nullptr;
	}

	_itemMenu = Menu::create();
	_itemMenu->setPosition({0, 0});
	addChild(_itemMenu, 100);

	if (_leftArrow)
		_leftArrow->setVisible(_page > 0);
	if (_rightArrow)
		_rightArrow->setVisible(_page < pageCount() - 1);

	auto* gm = GameManager::getInstance();
	const auto& winSize = Director::getInstance()->getWinSize();
	const float gridX = winSize.width * 0.5f;
	const int start = _page * kItemsPerPage;
	const int end = std::min(start + kItemsPerPage, ShopCatalog::itemCount());

	for (int i = start; i < end; i++)
	{
		const auto* item = ShopCatalog::itemAt(i);
		if (!item)
			continue;

		constexpr float kSlot = 60.f;
		auto* cell = Node::create();
		cell->setContentSize({kSlot, kSlot + 18.f});
		cell->setAnchorPoint({0.5f, 0.5f});

		const bool unlocked = gm->isIconUnlocked(item->type, item->itemId);

		if (auto* square = Sprite::createWithSpriteFrameName("playerSquare_001.png"))
		{
			square->setStretchEnabled(false);
			square->setPosition({kSlot * 0.5f, kSlot * 0.5f + 8.f});
			cell->addChild(square, 0);
		}

		const float iconScale = iconScaleForType(item->type);
		const Vec2 iconPos{kSlot * 0.5f, kSlot * 0.5f + 8.f};

		if (item->type == IconType::kIconTypeDeathEffect)
		{
			if (auto* icon = Sprite::createWithSpriteFrameName(StringUtils::format("explosionIcon_%02d_001.png", item->itemId)))
			{
				icon->setAnchorPoint({0.5f, 0.5f});
				icon->setPosition(iconPos);
				icon->setScale(iconScale * (30.f / std::max(icon->getContentSize().width, icon->getContentSize().height)));
				if (unlocked)
					icon->setColor(ownedTint());
				cell->addChild(icon, 1);
			}
		}
		else if (auto* icon = SimplePlayer::create(0))
		{
			icon->setPlayIdleAnimation(false);
			icon->updateGamemode(item->itemId, item->type);
			if (item->type == IconType::kIconTypeUfo && icon->m_pDomeSprite)
				icon->m_pDomeSprite->setVisible(false);
			icon->setMainColor(gm->getPlayerMainColor());
			icon->setSecondaryColor(gm->getPlayerSecondaryColor());
			icon->setGlowColor(gm->getPlayerGlowColor());
			icon->setGlow(false);
			icon->fitToSize(28.f * iconScale / 0.88f);
			icon->placeCenteredAt(iconPos);
			icon->setGlow(gm->isPlayerGlowEnabled());
			if (unlocked)
				icon->setColor(ownedTint());
			cell->addChild(icon, 1);
		}

		if (!unlocked)
		{
			auto* price = Label::createWithBMFont(
				GameToolbox::getTextureString("bigFont.fnt"),
				fmt::format("{}", item->costOrbs));
			price->setScale(0.35f);
			price->setAnchorPoint({1.f, 0.5f});
			price->setPosition({kSlot * 0.5f + 2.f, 6.f});
			cell->addChild(price, 2);
			if (auto* orb = Sprite::createWithSpriteFrameName("currencyOrbIcon_001.png"))
			{
				orb->setScale(0.32f);
				orb->setAnchorPoint({0.f, 0.5f});
				orb->setPosition({kSlot * 0.5f + 4.f, 6.f});
				cell->addChild(orb, 2);
			}
		}

		auto* btn = MenuItemSpriteExtra::create(cell, [this](Node* n) {
			onItemPressed(n->getTag());
		});
		btn->setTag(i);
		cell->setPosition({kSlot * 0.5f, (kSlot + 18.f) * 0.5f});

		const int slot = i - start;
		const int col = slot % 4;
		const int row = slot / 4;
		btn->setPosition({
			gridX + (col - 1.5f) * kItemSpacing,
			kGridY + (0.5f - row) * kItemSpacing
		});
		_itemMenu->addChild(btn);
	}
}

void GJShopLayer::onItemPressed(int index)
{
	if (_activeDialog)
		return;

	const auto* item = ShopCatalog::itemAt(index);
	if (!item)
		return;

	auto* gm = GameManager::getInstance();
	if (gm->isIconUnlocked(item->type, item->itemId))
	{
		if (auto* alert = AlertLayer::create("Buy Item", "You already own this item."))
			alert->show();
		return;
	}

	if (gm->getOrbs() < item->costOrbs)
	{
		showCantAffordDialog();
		return;
	}

	auto* alert = AlertLayer::create(
		"Buy Item",
		fmt::format("Do you want to buy this item\nfor {} Mana Orbs?", item->costOrbs),
		"Cancel",
		"Buy",
		nullptr,
		nullptr);
	if (!alert)
		return;
	alert->setBtn2Callback([this, alert, index](Node*) {
		const auto* bought = ShopCatalog::itemAt(index);
		auto* manager = GameManager::getInstance();
		if (bought && manager->spendOrbs(bought->costOrbs))
		{
			manager->unlockIcon(bought->type, bought->itemId);
			manager->save();
			refreshOrbLabel();
			rebuildItemGrid();
		}
		alert->close();
	});
	alert->show();
}

void GJShopLayer::goBack()
{
	if (_activeDialog)
		return;
	AudioEngine::stopAll();
	AudioEngine::play2d("menuLoop.mp3", true, 0.2f);
	GameToolbox::popSceneWithTransition(0.5f, popTransition::kTransitionShop);
}

void GJShopLayer::onKeyPressed(EventKeyboard::KeyCode keyCode, Event*)
{
	if (keyCode == EventKeyboard::KeyCode::KEY_BACK || keyCode == EventKeyboard::KeyCode::KEY_ESCAPE)
	{
		if (_activeDialog)
			return;
		goBack();
	}
}
