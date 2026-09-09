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
#include "EventListenerMouse.h"
#include "EventListenerTouch.h"
#include "EventMouse.h"
#include "GameToolbox/getTextureString.h"
#include "base/Director.h"
#include "fmt/format.h"
#include "platform/FileUtils.h"
#include "ui/UIScale9Sprite.h"
#include <algorithm>
#include <cmath>

USING_NS_AX;

namespace
{
// GeometryDash.exe DialogLayer::init / displayDialogObject (2.2)
constexpr float kPanelW = 380.f;
constexpr float kPanelH = 100.f;
constexpr float kCap = 80.f;
constexpr float kInnerW = 280.f;
constexpr float kInnerH = 80.f;
constexpr float kInnerX = 40.f;
constexpr float kInnerY = 0.f;
constexpr float kNameX = -93.f;
constexpr float kNameY = 36.f;
constexpr float kPortraitX = -143.f;
constexpr float kNavY = -50.f;
constexpr float kTextX = -92.f;
constexpr float kTextY = 0.f;
constexpr float kTextW = 220.f;
constexpr float kTextLineH = 20.f; // TextArea lineHeight
constexpr float kTypeDelay = 0.02f;
constexpr float kBottomY = 70.f;
constexpr float kHalfW = 191.f; // animateIn FromLeft/Right
constexpr float kNameMaxW = 200.f;
constexpr float kNameScale = 0.5f;

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

ui::Scale9Sprite* makeSquare(const std::string& path, float w, float h)
{
	const Rect cap{0.f, 0.f, kCap, kCap};
	auto* bg = ui::Scale9Sprite::create(path, cap);
	if (!bg)
		bg = ui::Scale9Sprite::create(GameToolbox::getTextureString("GJ_square02.png"), cap);
	if (!bg)
		return nullptr;
	bg->setContentSize({w, h});
	return bg;
}
} // namespace

DialogLayer* DialogLayer::create(std::vector<DialogPage> pages, int background)
{
	return create(std::move(pages), nullptr, background);
}

DialogLayer* DialogLayer::create(std::vector<DialogPage> pages, const char* portraitFrame, int background)
{
	auto* ret = new (std::nothrow) DialogLayer();
	if (ret && ret->init(std::move(pages), background, portraitFrame))
	{
		ret->autorelease();
		return ret;
	}
	delete ret;
	return nullptr;
}

bool DialogLayer::init(std::vector<DialogPage> pages, int background, const char* portraitOverride)
{
	if (!PopupLayer::init())
		return false;
	if (pages.empty())
		return false;

	_pages = std::move(pages);
	_background = std::clamp(background, 1, 7);
	if (portraitOverride && portraitOverride[0])
		_portraitOverride = portraitOverride;

	// Official DialogLayer is a dim layer; keep light so the box reads clearly.
	setColor(Color3B::BLACK);
	setOpacity(0);

	_bg = makeSquare(GameToolbox::getTextureString(fmt::format("GJ_square{:02}.png", _background)), kPanelW, kPanelH);
	if (!_bg)
		return false;
	_bg->setPosition({0.f, 0.f});
	_mainLayer->addChild(_bg, 0);

	_textBg = makeSquare(GameToolbox::getTextureString("square02b_001.png"), kInnerW, kInnerH);
	if (_textBg)
	{
		_textBg->setPosition({kInnerX, kInnerY});
		_textBg->setColor({0, 0, 0});
		_textBg->setOpacity(50); // official displayDialogObject: setOpacity(0x32)
		_mainLayer->addChild(_textBg, 1);
	}

	_nameLabel = Label::createWithBMFont(GameToolbox::getTextureString("goldFont.fnt"), " ");
	// Official: setAnchorPoint(0, 1), setPosition(-93, 36), limitLabelWidth(200, 0.5, 0)
	_nameLabel->setAnchorPoint({0.f, 1.f});
	_nameLabel->setScale(kNameScale);
	_nameLabel->setPosition({kNameX, kNameY});
	_mainLayer->addChild(_nameLabel, 3);

	// Official TextArea: width 220/scale, lineHeight 20, anchor (0, 0.5), pos (-92, 0), then setScale(textScale)
	_textLabel = Label::createWithBMFont(GameToolbox::getTextureString("chatFont.fnt"), "", TextHAlignment::LEFT);
	_textLabel->setAnchorPoint({0.f, 0.5f});
	_textLabel->setAlignment(TextHAlignment::LEFT, TextVAlignment::CENTER);
	_textLabel->setDimensions(kTextW, kTextLineH * 4.f);
	_textLabel->setOverflow(Label::Overflow::CLAMP);
	_textLabel->setPosition({kTextX, kTextY});
	_textLabel->setColor(Color3B::WHITE);
	_mainLayer->addChild(_textLabel, 3);

	if (SpriteFrameCache::getInstance()->getSpriteFrameByName("GJ_chatBtn_01_001.png"))
		_navButton = Sprite::createWithSpriteFrameName("GJ_chatBtn_01_001.png");
	if (!_navButton)
		_navButton = Sprite::create(GameToolbox::getTextureString("GJ_chatBtn_01_001.png"));
	if (!_navButton && SpriteFrameCache::getInstance()->getSpriteFrameByName("navArrowBtn_001.png"))
		_navButton = Sprite::createWithSpriteFrameName("navArrowBtn_001.png");
	if (_navButton)
	{
		_navButton->setStretchEnabled(false);
		_navButton->setPosition({0.f, kNavY});
		_navButton->setScale(0.85f);
		_mainLayer->addChild(_navButton, 4);
	}

	setChatPlacement(DialogChatPlacement::Center);

	// PopupLayer::init installs a swallow-only touch listener — replace it.
	_eventDispatcher->removeEventListenersForTarget(this);

	// One physical click must advance exactly once. Desktop often delivers both
	// a synthesized Touch and a Mouse event for the same LMB press.
	auto tryPressAdvance = [this]() {
		if (_pressConsumed || _closing)
			return;
		_pressConsumed = true;
		advance();
	};
	auto releasePress = [this]() { _pressConsumed = false; };

	auto* tap = EventListenerTouchOneByOne::create();
	tap->setSwallowTouches(true);
	tap->onTouchBegan = [tryPressAdvance](Touch*, Event*) {
		tryPressAdvance();
		return true;
	};
	tap->onTouchEnded = [releasePress](Touch*, Event*) { releasePress(); };
	tap->onTouchCancelled = [releasePress](Touch*, Event*) { releasePress(); };
	_eventDispatcher->addEventListenerWithSceneGraphPriority(tap, this);

	auto* mouse = EventListenerMouse::create();
	mouse->onMouseDown = [tryPressAdvance](Event* event) {
		auto* mouseEvent = static_cast<EventMouse*>(event);
		if (mouseEvent->getMouseButton() == EventMouse::MouseButton::BUTTON_LEFT)
			tryPressAdvance();
	};
	mouse->onMouseUp = [releasePress](Event* event) {
		auto* mouseEvent = static_cast<EventMouse*>(event);
		if (mouseEvent->getMouseButton() == EventMouse::MouseButton::BUTTON_LEFT)
			releasePress();
	};
	_eventDispatcher->addEventListenerWithSceneGraphPriority(mouse, this);

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

void DialogLayer::setChatPlacement(DialogChatPlacement placement)
{
	_placement = placement;
	const auto winSize = Director::getInstance()->getWinSize();
	switch (placement)
	{
	case DialogChatPlacement::Center:
		_mainPos = {winSize.width * 0.5f, winSize.height * 0.5f};
		break;
	case DialogChatPlacement::Top:
		_mainPos = {winSize.width * 0.5f, (winSize.height - 50.f) - 20.f};
		break;
	case DialogChatPlacement::Bottom:
	default:
		_mainPos = {winSize.width * 0.5f, kBottomY};
		break;
	}
	if (_mainLayer)
		_mainLayer->setPosition(_mainPos);
}

Sprite* DialogLayer::loadPortrait(int frame) const
{
	if (!_portraitOverride.empty())
	{
		if (auto* fileSpr = Sprite::create(GameToolbox::getTextureString(_portraitOverride)))
		{
			fileSpr->setStretchEnabled(false);
			return fileSpr;
		}
		if (SpriteFrameCache::getInstance()->getSpriteFrameByName(_portraitOverride))
		{
			if (auto* frameSpr = Sprite::createWithSpriteFrameName(_portraitOverride))
			{
				frameSpr->setStretchEnabled(false);
				return frameSpr;
			}
		}
	}

	frame = std::max(1, frame);
	const std::string name = fmt::format("dialogIcon_{:03}.png", frame);
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
	if (auto* fallback = Sprite::create(GameToolbox::getTextureString("dialogIcon_001.png")))
	{
		fallback->setStretchEnabled(false);
		return fallback;
	}
	return nullptr;
}

void DialogLayer::show(Transitions)
{
	if (!getParent())
		PopupLayer::show(kNone);
	animateIn(_animationType);
}

void DialogLayer::animateIn(DialogAnimationType type)
{
	_animationType = type;
	if (!_mainLayer)
		return;

	_mainLayer->stopAllActions();
	_mainLayer->setPosition(_mainPos);
	_mainLayer->setScale(1.f);

	auto* director = Director::getInstance();
	switch (type)
	{
	case DialogAnimationType::FromCenter:
		_mainLayer->setScale(0.1f);
		_mainLayer->runAction(EaseElasticOut::create(ScaleTo::create(0.5f, 1.f), 0.6f));
		break;
	case DialogAnimationType::FromLeft: {
		_mainLayer->setPosition({-kHalfW, _mainPos.y});
		_mainLayer->runAction(EaseElasticOut::create(MoveTo::create(0.5f, _mainPos), 0.6f));
		break;
	}
	case DialogAnimationType::FromRight: {
		_mainLayer->setPosition({director->getWinSize().width + kHalfW, _mainPos.y});
		_mainLayer->runAction(EaseElasticOut::create(MoveTo::create(0.5f, _mainPos), 0.6f));
		break;
	}
	case DialogAnimationType::FromTop: {
		_mainLayer->setPosition({_mainPos.x, director->getWinSize().height + 51.f});
		_mainLayer->runAction(EaseElasticOut::create(MoveTo::create(0.5f, _mainPos), 0.6f));
		break;
	}
	case DialogAnimationType::FromTop2: {
		_mainLayer->setPosition({_mainPos.x, director->getWinSize().height - 51.f});
		_mainLayer->runAction(EaseElasticOut::create(MoveTo::create(0.5f, _mainPos), 0.6f));
		break;
	}
	}

	setOpacity(0);
	runAction(FadeTo::create(0.14f, 120));
}

void DialogLayer::setPage(int page)
{
	_page = page;
	if (_page < 0 || _page >= static_cast<int>(_pages.size()))
		return;

	const auto& dlg = _pages[static_cast<size_t>(_page)];
	if (_nameLabel)
	{
		_nameLabel->setString(dlg.speaker.empty() ? " " : dlg.speaker);
		_nameLabel->setColor(dlg.nameColor);
		// limitLabelWidth(200, 0.5, 0) — shrink long names to fit
		_nameLabel->setScale(kNameScale);
		const float nw = _nameLabel->getContentSize().width * kNameScale;
		if (nw > kNameMaxW && nw > 0.f)
			_nameLabel->setScale(kNameMaxW / _nameLabel->getContentSize().width);
	}

	if (_portrait)
	{
		_portrait->removeFromParent();
		_portrait = nullptr;
	}
	_portrait = loadPortrait(dlg.characterFrame);
	if (_portrait)
	{
		_portrait->setPosition({kPortraitX, 0.f});
		// Official loads dialogIcon at native size (~1.0). Cap height to fit the 100px box.
		const float h = std::max(_portrait->getContentSize().height, 1.f);
		if (h > 92.f)
			_portrait->setScale(92.f / h);
		_mainLayer->addChild(_portrait, 2);
	}

	if (_textLabel)
	{
		const float ts = std::clamp(dlg.textScale, 0.35f, 1.25f);
		// Official: create width = 220/scale, then setScale(scale) → on-screen width stays ~220
		_textLabel->setDimensions(kTextW / ts, (kTextLineH * 4.f) / ts);
		_textLabel->setScale(ts);
	}

	parseColored(dlg.text, _plain, _colors);
	_visibleCount = 0;
	_animating = true;
	refreshText();
	unschedule("dialog_type");
	schedule([this](float dt) { tickTypewriter(dt); }, kTypeDelay, "dialog_type");
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

void DialogLayer::updateNavButtonFrame()
{
	if (!_navButton)
		return;
	const bool done = _visibleCount >= static_cast<int>(_plain.size());
	const char* frame = done ? "GJ_chatBtn_02_001.png" : "GJ_chatBtn_01_001.png";
	if (SpriteFrameCache::getInstance()->getSpriteFrameByName(frame))
		_navButton->setSpriteFrame(SpriteFrameCache::getInstance()->getSpriteFrameByName(frame));
	_navButton->setVisible(true);
	_navButton->setOpacity(255);
	_navButton->stopAllActions();
	if (done)
	{
		_navButton->runAction(RepeatForever::create(Sequence::create(
			FadeTo::create(0.4f, 70), FadeTo::create(0.4f, 255), nullptr)));
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
	updateNavButtonFrame();
}

void DialogLayer::tickTypewriter(float)
{
	if (_visibleCount >= static_cast<int>(_plain.size()))
	{
		_animating = false;
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

	const auto& dlg = _pages[static_cast<size_t>(_page)];
	if (_animating && _visibleCount < static_cast<int>(_plain.size()))
	{
		if (dlg.unskippable)
			return;
		_visibleCount = static_cast<int>(_plain.size());
		_animating = false;
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
	runAction(Sequence::create(
		FadeTo::create(0.14f, 0),
		CallFunc::create([this, onClose]() {
			PopupLayer::close();
			if (onClose)
				onClose();
		}),
		nullptr));
}
