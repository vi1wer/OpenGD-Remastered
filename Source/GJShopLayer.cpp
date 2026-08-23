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

USING_NS_AX;

Scene* GJShopLayer::scene()
{
	return GJShopLayer::create();
}

GJShopLayer* GJShopLayer::create()
{
	auto* ret = new GJShopLayer();
	if (ret && ret->init())
	{
		ret->autorelease();
		return ret;
	}
	delete ret;
	return nullptr;
}

bool GJShopLayer::init()
{
	if (!Scene::init())
		return false;

	auto* director = Director::getInstance();
	const auto& winSize = director->getWinSize();
	const Vec2 center = winSize / 2;
	_bgScaleX = winSize.width / kDesignWidth;

	// shopBG is 480x320 and already contains the 3-shelf cabinet.
	// Official GD stretches it in X on widescreen; item X uses the same scale.
	GameToolbox::createBG(this, {62, 38, 24});
	if (auto* bg = Sprite::createWithSpriteFrameName("shopBG_001.png"))
	{
		bg->setStretchEnabled(false);
		bg->setPosition(center);
		bg->setScaleX(_bgScaleX);
		addChild(bg, 0);
	}

	if (auto* keeper = Node::create())
	{
		auto addPart = [keeper](const char* frame, const Vec2& pos, int z, bool flipX = false) {
			auto* spr = Sprite::createWithSpriteFrameName(frame);
			if (!spr)
				return;
			spr->setStretchEnabled(false);
			spr->setPosition(pos);
			spr->setFlippedX(flipX);
			keeper->addChild(spr, z);
		};

		// Idle pose from GJShopKeeper_AnimDesc.plist
		addPart("shopKeeper_torso_01_001.png", {0.f, -20.5f}, 0);
		addPart("shopKeeper_head_01_001.png", {0.f, 25.5f}, 1);
		addPart("shopKeeper_eye_01_001.png", {-15.f, 25.f}, 2);
		addPart("shopKeeper_eye_01_001.png", {15.f, 25.f}, 3);
		addPart("shopKeeper_pupil_01_001.png", {-14.f, 23.f}, 4);
		addPart("shopKeeper_pupil_01_001.png", {14.f, 23.f}, 5);
		addPart("shopKeeper_jaw_01_001.png", {0.f, 0.5f}, 6);
		addPart("shopKeeper_hand_01_001.png", {-24.625f, -36.25f}, 7);
		addPart("shopKeeper_hand_01_001.png", {24.625f, -36.25f}, 8, true);

		// Right side, behind the desk (z 2 < desk 4). Hands sit on the counter.
		keeper->setPosition({center.x + 148.f * _bgScaleX, 108.f});
		keeper->setScale(2.05f);
		addChild(keeper, 2);
		_shopkeeper = keeper;

		auto* tap = EventListenerTouchOneByOne::create();
		tap->setSwallowTouches(false);
		tap->onTouchBegan = [this](Touch* touch, Event*) {
			if (!_shopkeeper || _welcomeDialog)
				return false;
			const Vec2 local = _shopkeeper->convertToNodeSpace(touch->getLocation());
			if (std::abs(local.x) < 42.f && local.y > -8.f && local.y < 52.f)
			{
				showWelcomeDialog();
				return true;
			}
			return false;
		};
		_eventDispatcher->addEventListenerWithSceneGraphPriority(tap, keeper);
	}

	if (auto* desk = Sprite::createWithSpriteFrameName("storeDesk_001.png"))
	{
		desk->setStretchEnabled(false);
		desk->setAnchorPoint({0.5f, 0.f});
		desk->setPosition({center.x, 0.f});
		desk->setScaleX(winSize.width / desk->getContentSize().width);
		addChild(desk, 4);
	}

	if (auto* sign = Sprite::createWithSpriteFrameName("shopSign_001.png"))
	{
		sign->setStretchEnabled(false);
		sign->setPosition({center.x, winSize.height - 22.f});
		addChild(sign, 8);
	}

	auto* nav = Menu::create();
	nav->setPosition({0, 0});
	addChild(nav, 10);

	auto* backBtn = MenuItemSpriteExtra::create("GJ_arrow_01_001.png", [this](Node*) { goBack(); });
	backBtn->setPosition({24.f, winSize.height - 23.f});
	nav->addChild(backBtn);

	auto* leftSpr = Sprite::createWithSpriteFrameName("navArrowBtn_001.png");
	if (leftSpr)
	{
		leftSpr->setFlippedX(true);
		_leftArrow = MenuItemSpriteExtra::create(leftSpr, [this](Node*) { setPage(_page - 1); });
		_leftArrow->setPosition({center.x - 208.f * _bgScaleX, center.y + 8.f});
		_leftArrow->setScale(0.65f);
		nav->addChild(_leftArrow);
	}
	if (auto* rightSpr = Sprite::createWithSpriteFrameName("navArrowBtn_001.png"))
	{
		_rightArrow = MenuItemSpriteExtra::create(rightSpr, [this](Node*) { setPage(_page + 1); });
		_rightArrow->setPosition({center.x + 208.f * _bgScaleX, center.y + 8.f});
		_rightArrow->setScale(0.65f);
		nav->addChild(_rightArrow);
	}

	if (auto* orbIcon = Sprite::createWithSpriteFrameName("currencyOrbIcon_001.png"))
	{
		orbIcon->setScale(0.6f);
		orbIcon->setPosition({winSize.width - 18.f, winSize.height - 16.f});
		addChild(orbIcon, 10);
	}

	_orbLabel = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "0");
	_orbLabel->setScale(0.4f);
	_orbLabel->setAnchorPoint({1.f, 0.5f});
	_orbLabel->setPosition({winSize.width - 34.f, winSize.height - 16.f});
	addChild(_orbLabel, 10);
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
	// Must wait until TransitionMoveInT is gone. PopupLayer::show() parents to
	// getRunningScene(); during the slide that scene is the transition and is
	// destroyed a frame later, so the dialog never stays on screen.
	if (!_welcomeDialog)
		showWelcomeDialog();
}

void GJShopLayer::showWelcomeDialog()
{
	if (_welcomeDialog)
		return;

	_welcomeDialog = DialogLayer::create({
		{"Scratch", "My shop is still <co>under construction</c>... It's <cy>not fully ready</c> yet."},
		{"Scratch", "But this will be <cg>fixed</c> in <cl>future updates</c>!"},
		{"Scratch", "Come back <cb>later</c>..... <cr>Or right now!</c>"},
	}, "dialogIcon_005.png");
	if (!_welcomeDialog)
		return;
	_welcomeDialog->setOnClose([this]() { _welcomeDialog = nullptr; });
	addChild(_welcomeDialog, 200);
	_welcomeDialog->show();
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

namespace
{
// shopBG is 480x320, unscaled in Y, centered on screen. Item local space is
// bottom-left of a fixed slot (Axmol Node children are never centered).
constexpr float kSlotW = 72.f;
constexpr float kSlotH = 78.f;
constexpr float kIconX = 36.f;
constexpr float kIconY = 46.f;
constexpr float kPriceY = 14.f;
constexpr float kShopRowY[] = {18.f, -56.f};
constexpr float kShopColX[] = {-108.f, -36.f, 36.f, 108.f};
} // namespace

void GJShopLayer::rebuildItemGrid()
{
	if (_itemMenu)
	{
		_itemMenu->removeFromParent();
		_itemMenu = nullptr;
	}

	_itemMenu = Menu::create();
	_itemMenu->setPosition({0, 0});
	addChild(_itemMenu, 6);

	if (_leftArrow)
		_leftArrow->setVisible(_page > 0);
	if (_rightArrow)
		_rightArrow->setVisible(_page < pageCount() - 1);

	auto* gm = GameManager::getInstance();
	const auto& winSize = Director::getInstance()->getWinSize();
	const int start = _page * kItemsPerPage;
	const int end = std::min(start + kItemsPerPage, ShopCatalog::itemCount());

	for (int i = start; i < end; i++)
	{
		const auto* item = ShopCatalog::itemAt(i);
		if (!item)
			continue;

		auto* cell = Node::create();
		cell->setContentSize({kSlotW, kSlotH});
		cell->setAnchorPoint({0.f, 0.f});
		const bool unlocked = gm->isIconUnlocked(item->type, item->itemId);

		if (item->type == IconType::kIconTypeDeathEffect)
		{
			if (auto* icon = Sprite::createWithSpriteFrameName(StringUtils::format("explosionIcon_%02d_001.png", item->itemId)))
			{
				icon->setAnchorPoint({0.5f, 0.5f});
				icon->setPosition({kIconX, kIconY});
				icon->setScale(30.f / std::max(icon->getContentSize().width, icon->getContentSize().height));
				if (unlocked)
					icon->setColor({50, 50, 50});
				cell->addChild(icon);
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
			icon->fitToSize(30.f);
			icon->placeCenteredAt({kIconX, kIconY});
			icon->setGlow(gm->isPlayerGlowEnabled());
			if (unlocked)
				icon->setColor({45, 45, 45});
			cell->addChild(icon);
		}

		if (!unlocked)
		{
			auto* price = Label::createWithBMFont(
				GameToolbox::getTextureString("bigFont.fnt"),
				fmt::format("{}", item->costOrbs));
			price->setScale(0.28f);
			price->setAnchorPoint({1.f, 0.5f});
			price->setPosition({kIconX + 2.f, kPriceY});
			cell->addChild(price);
			if (auto* orb = Sprite::createWithSpriteFrameName("currencyOrbIcon_001.png"))
			{
				orb->setScale(0.28f);
				orb->setAnchorPoint({0.f, 0.5f});
				orb->setPosition({kIconX + 6.f, kPriceY});
				cell->addChild(orb);
			}
		}

		auto* btn = MenuItemSpriteExtra::create(cell, [this](Node* n) {
			onItemPressed(n->getTag());
		});
		btn->setTag(i);
		// MenuItemSpriteExtra::init recenters the node; pin it so the slot
		// position is the center of this fixed-size cell.
		cell->setAnchorPoint({0.5f, 0.5f});
		cell->setPosition({kSlotW * 0.5f, kSlotH * 0.5f});
		const int slot = i - start;
		const int col = slot % 4;
		const int row = slot / 4;
		btn->setPosition({
			winSize.width / 2 + kShopColX[col] * _bgScaleX,
			winSize.height / 2 + kShopRowY[row]
		});
		_itemMenu->addChild(btn);
	}
}

void GJShopLayer::onItemPressed(int index)
{
	const auto* item = ShopCatalog::itemAt(index);
	if (!item)
		return;

	auto* gm = GameManager::getInstance();
	if (gm->isIconUnlocked(item->type, item->itemId))
	{
		if (auto* alert = AlertLayer::create("The Shop", "You already own this item."))
			alert->show();
		return;
	}

	if (gm->getOrbs() < item->costOrbs)
	{
		if (auto* alert = AlertLayer::create("The Shop", "Not enough Mana Orbs."))
			alert->show();
		return;
	}

	auto* alert = AlertLayer::create(
		"The Shop",
		fmt::format("Buy this item for {} Mana Orbs?", item->costOrbs),
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
	AudioEngine::stopAll();
	AudioEngine::play2d("menuLoop.mp3", true, 0.2f);
	GameToolbox::popSceneWithTransition(0.5f, popTransition::kTransitionShop);
}

void GJShopLayer::onKeyPressed(EventKeyboard::KeyCode keyCode, Event*)
{
	if (keyCode == EventKeyboard::KeyCode::KEY_BACK || keyCode == EventKeyboard::KeyCode::KEY_ESCAPE)
	{
		if (_welcomeDialog)
			return;
		goBack();
	}
}
