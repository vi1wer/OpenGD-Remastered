/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#include "DialogLayer.h"

#include "2d/ActionEase.h"
#include "2d/ActionInstant.h"
#include "2d/ActionInterval.h"
#include "2d/Label.h"
#include "2d/Sprite.h"
#include "2d/SpriteFrameCache.h"
#include "EventDispatcher.h"
#include "EventListenerKeyboard.h"
#include "EventListenerTouch.h"
#include "GameToolbox/getTextureString.h"
#include "base/Director.h"
#include "platform/FileUtils.h"
#include "ui/UIScale9Sprite.h"
#include <algorithm>

USING_NS_AX;

namespace
{
Color3B colorForTag(char tag)
{
	switch (tag)
	{
	case 'g':
		return {90, 255, 90};
	case 'y':
		return {255, 255, 80};
	case 'r':
		return {255, 75, 75};
	case 'b':
		return {80, 180, 255};
	case 'o':
		return {255, 165, 50};
	case 'p':
		return {255, 110, 255};
	case 'l':
		return {170, 255, 70};
	case 'a':
		return {80, 255, 230};
	default:
		return Color3B::WHITE;
	}
}

Sprite* loadPortrait(const char* portraitFrame)
{
	const char* name = (portraitFrame && portraitFrame[0]) ? portraitFrame : "dialogIcon_005.png";
	if (auto* fileSpr = Sprite::create(GameToolbox::getTextureString(name)))
	{
		fileSpr->setStretchEnabled(false);
		return fileSpr;
	}
	if (SpriteFrameCache::getInstance()->getSpriteFrameByName(name))
	{
		if (auto* frameSpr = Sprite::createWithSpriteFrameName(name))
		{
			frameSpr->setStretchEnabled(false);
			return frameSpr;
		}
	}
	if (auto* fallback = Sprite::create(GameToolbox::getTextureString("dialogIcon_005.png")))
	{
		fallback->setStretchEnabled(false);
		return fallback;
	}
	return nullptr;
}

ui::Scale9Sprite* loadDialogPanel()
{
	const Rect cap{0.f, 0.f, 80.f, 80.f};
	if (auto* bg = ui::Scale9Sprite::create(GameToolbox::getTextureString("GJ_square01.png"), cap))
		return bg;
	if (auto* bg = ui::Scale9Sprite::create(GameToolbox::getTextureString("GJ_square02.png"), cap))
		return bg;
	return ui::Scale9Sprite::create(GameToolbox::getTextureString("square01_001.png"));
}
} // namespace

DialogLayer* DialogLayer::create(std::vector<DialogPage> pages, const char* portraitFrame)
{
	auto* ret = new (std::nothrow) DialogLayer();
	if (ret && ret->init(std::move(pages), portraitFrame))
	{
		ret->autorelease();
		return ret;
	}
	delete ret;
	return nullptr;
}

bool DialogLayer::init(std::vector<DialogPage> pages, const char* portraitFrame)
{
	if (!PopupLayer::init())
		return false;
	if (pages.empty())
		return false;

	_pages = std::move(pages);
	setColor(Color3B::BLACK);
	setOpacity(0);

	const auto& winSize = Director::getInstance()->getWinSize();
	const float boxW = std::min(winSize.width - 20.f, 440.f);
	const float boxH = 104.f;
	const Vec2 boxPos{winSize.width * 0.5f, 68.f};

	auto* bg = loadDialogPanel();
	if (!bg)
		return false;
	bg->setContentSize({boxW, boxH});
	bg->setPosition(boxPos);
	_mainLayer->addChild(bg, 0);

	_portrait = Node::create();
	_portrait->setPosition({boxPos.x - boxW * 0.5f + 52.f, boxPos.y - 2.f});
	_mainLayer->addChild(_portrait, 2);

	if (auto* icon = loadPortrait(portraitFrame))
	{
		const float target = 72.f;
		const float h = std::max(icon->getContentSize().height, 1.f);
		icon->setScale(target / h);
		_portrait->addChild(icon);
	}

	_nameLabel = Label::createWithBMFont(GameToolbox::getTextureString("goldFont.fnt"), "");
	_nameLabel->setAnchorPoint({0.f, 0.5f});
	_nameLabel->setScale(0.38f);
	_nameLabel->setPosition({boxPos.x - boxW * 0.5f + 96.f, boxPos.y + 34.f});
	_mainLayer->addChild(_nameLabel, 2);

	_textLabel = Label::createWithBMFont(GameToolbox::getTextureString("chatFont.fnt"), "", TextHAlignment::LEFT);
	_textLabel->setAnchorPoint({0.f, 1.f});
	_textLabel->setAlignment(TextHAlignment::LEFT, TextVAlignment::TOP);
	_textLabel->setDimensions(boxW - 130.f, 62.f);
	_textLabel->setScale(0.58f);
	_textLabel->setColor(Color3B::WHITE);
	_textLabel->setPosition({boxPos.x - boxW * 0.5f + 96.f, boxPos.y + 20.f});
	_mainLayer->addChild(_textLabel, 2);

	if (auto* arrow = Sprite::createWithSpriteFrameName("navArrowBtn_001.png"))
	{
		arrow->setStretchEnabled(false);
		arrow->setScale(0.28f);
		arrow->setRotation(90.f);
		arrow->setPosition({boxPos.x + boxW * 0.5f - 22.f, boxPos.y - boxH * 0.5f + 18.f});
		arrow->runAction(RepeatForever::create(Sequence::create(
			FadeTo::create(0.4f, 70),
			FadeTo::create(0.4f, 255),
			nullptr)));
		_mainLayer->addChild(arrow, 3);
		_nextArrow = arrow;
	}
	else
	{
		auto* fallbackArrow = Label::createWithBMFont(GameToolbox::getTextureString("goldFont.fnt"), "v");
		fallbackArrow->setScale(0.45f);
		fallbackArrow->setPosition({boxPos.x + boxW * 0.5f - 20.f, boxPos.y - boxH * 0.5f + 16.f});
		fallbackArrow->runAction(RepeatForever::create(Sequence::create(
			FadeTo::create(0.4f, 70),
			FadeTo::create(0.4f, 255),
			nullptr)));
		_mainLayer->addChild(fallbackArrow, 3);
		_nextArrow = fallbackArrow;
	}

	auto* tap = EventListenerTouchOneByOne::create();
	tap->setSwallowTouches(true);
	tap->onTouchBegan = [this](Touch*, Event*) {
		advance();
		return true;
	};
	_eventDispatcher->addEventListenerWithSceneGraphPriority(tap, this);

	auto* keys = EventListenerKeyboard::create();
	keys->onKeyPressed = [this](EventKeyboard::KeyCode key, Event*) {
		if (key == EventKeyboard::KeyCode::KEY_ENTER || key == EventKeyboard::KeyCode::KEY_SPACE ||
			key == EventKeyboard::KeyCode::KEY_ESCAPE || key == EventKeyboard::KeyCode::KEY_BACK)
			advance();
	};
	_eventDispatcher->addEventListenerWithSceneGraphPriority(keys, this);

	setPage(0);
	return true;
}

void DialogLayer::show(Transitions)
{
	if (!getParent())
		PopupLayer::show(kNone);
	setOpacity(0);
	runAction(FadeTo::create(0.16f, 90));
	if (_mainLayer)
	{
		_mainLayer->setPositionY(-36.f);
		_mainLayer->runAction(EaseBackOut::create(MoveTo::create(0.26f, Vec2::ZERO)));
	}
}

void DialogLayer::setPage(int page)
{
	_page = page;
	if (_page < 0 || _page >= static_cast<int>(_pages.size()))
		return;

	const auto& dlg = _pages[static_cast<size_t>(_page)];
	if (_nameLabel)
		_nameLabel->setString(dlg.speaker);

	parseColored(dlg.text, _plain, _colors);
	_visibleCount = 0;
	refreshText();
	unschedule("dialog_type");
	schedule([this](float dt) { tickTypewriter(dt); }, 0.028f, "dialog_type");
}

void DialogLayer::parseColored(const std::string& tagged, std::string& plain, std::vector<Color3B>& colors)
{
	plain.clear();
	colors.clear();
	Color3B current = Color3B::WHITE;
	for (size_t i = 0; i < tagged.size();)
	{
		if (i + 3 < tagged.size() && tagged[i] == '<' && tagged[i + 1] == 'c' && tagged[i + 3] == '>')
		{
			current = colorForTag(tagged[i + 2]);
			i += 4;
			continue;
		}
		if (i + 3 < tagged.size() && tagged.compare(i, 4, "</c>") == 0)
		{
			current = Color3B::WHITE;
			i += 4;
			continue;
		}
		plain.push_back(tagged[i]);
		colors.push_back(current);
		++i;
	}
}

void DialogLayer::refreshText()
{
	if (!_textLabel)
		return;
	const int count = std::clamp(_visibleCount, 0, static_cast<int>(_plain.size()));
	_textLabel->setString(_plain.substr(0, static_cast<size_t>(count)));
	for (int i = 0; i < count; i++)
	{
		if (_plain[static_cast<size_t>(i)] == '\n')
			continue;
		if (auto* letter = _textLabel->getLetter(i))
			letter->setColor(_colors[static_cast<size_t>(i)]);
	}
	if (_nextArrow)
		_nextArrow->setVisible(count >= static_cast<int>(_plain.size()));
}

void DialogLayer::tickTypewriter(float)
{
	if (_visibleCount >= static_cast<int>(_plain.size()))
	{
		unschedule("dialog_type");
		refreshText();
		return;
	}
	++_visibleCount;
	refreshText();
}

void DialogLayer::advance()
{
	if (_closing)
		return;
	if (_visibleCount < static_cast<int>(_plain.size()))
	{
		_visibleCount = static_cast<int>(_plain.size());
		unschedule("dialog_type");
		refreshText();
		return;
	}
	if (_page + 1 < static_cast<int>(_pages.size()))
	{
		setPage(_page + 1);
		return;
	}
	close();
}

void DialogLayer::close()
{
	if (_closing)
		return;
	_closing = true;
	unschedule("dialog_type");
	auto onClose = _onClose;
	_mainLayer->runAction(EaseBackIn::create(MoveBy::create(0.18f, {0.f, -50.f})));
	runAction(Sequence::create(
		FadeTo::create(0.18f, 0),
		CallFunc::create([this, onClose]() {
			PopupLayer::close();
			if (onClose)
				onClose();
		}),
		nullptr));
}
