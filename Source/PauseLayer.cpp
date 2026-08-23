/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#include "PauseLayer.h"

#include "PlayLayer.h"
#include "ButtonSprite.h"
#include "MenuItemSpriteExtra.h"
#include "GJGameLevel.h"
#include "ComingSoonLayer.h"
#include "GameManager.h"
#include "DropDownLayer.h"

#include "2d/Layer.h"
#include "2d/Node.h"
#include "2d/Label.h"
#include "2d/Menu.h"
#include "2d/Sprite.h"
#include "2d/SpriteFrameCache.h"
#include "base/Director.h"
#include "EventListenerTouch.h"
#include "EventDispatcher.h"
#include "AudioEngine.h"
#include "GameToolbox/getTextureString.h"
#include "GameToolbox/nodes.h"

#include <algorithm>
#include <functional>
#include <fmt/format.h>
#include <new>

USING_NS_AX;

namespace
{
Sprite* loadFileSprite(const char* file)
{
	if (auto* spr = Sprite::create(GameToolbox::getTextureString(file)))
	{
		spr->setStretchEnabled(false);
		return spr;
	}
	if (auto* spr = Sprite::create(file))
	{
		spr->setStretchEnabled(false);
		return spr;
	}
	return nullptr;
}

Node* frameSprite(const char* frame, const char* fallbackText = nullptr)
{
	if (SpriteFrameCache::getInstance()->getSpriteFrameByName(frame))
	{
		auto* spr = Sprite::createWithSpriteFrameName(frame);
		if (spr)
		{
			spr->setStretchEnabled(false);
			return spr;
		}
	}
	if (fallbackText && fallbackText[0])
		return ButtonSprite::create(fallbackText);
	return nullptr;
}

void addVanillaProgressBar(Node* parent, float y, int percent, const char* title, const Color3B& fillColor)
{
	percent = std::clamp(percent, 0, 100);
	const std::string tex = GameToolbox::getTextureString("GJ_progressBar_001.png");
	const float cx = Director::getInstance()->getWinSize().width * 0.5f;

	auto* bar = Sprite::create(tex);
	if (!bar)
		return;
	bar->setStretchEnabled(false);
	bar->setCascadeColorEnabled(false);
	bar->setCascadeOpacityEnabled(false);
	bar->setPosition({cx, y});
	bar->setColor({0, 0, 0});
	bar->setOpacity(125);
	parent->addChild(bar, 3);

	auto* fill = Sprite::create(tex);
	if (fill && percent > 0)
	{
		fill->setStretchEnabled(false);
		fill->setAnchorPoint({0.f, 0.5f});
		fill->setPosition({cx - bar->getContentSize().width * 0.5f + 1.36f, y});
		fill->setColor(fillColor);
		fill->setOpacity(255);
		fill->setTextureRect({0.f, 0.f, bar->getContentSize().width * (percent / 100.f),
							  bar->getTextureRect().size.height});
		fill->setScaleX(0.992f);
		fill->setScaleY(0.86f);
		parent->addChild(fill, 4);
	}

	auto* pct = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), fmt::format("{}%", percent));
	if (pct)
	{
		pct->setScale(0.5f);
		pct->setPosition({cx, y});
		parent->addChild(pct, 5);
	}

	auto* titleLbl = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), title);
	if (titleLbl)
	{
		titleLbl->setScale(0.5f);
		titleLbl->setPosition({cx, y + 20.f});
		parent->addChild(titleLbl, 4);
	}
}

class VolumeSlider : public Node
{
public:
	static VolumeSlider* create(float value, std::function<void(float)> changed, std::function<void()> ended)
	{
		auto* ret = new (std::nothrow) VolumeSlider();
		if (ret && ret->init(value, std::move(changed), std::move(ended)))
		{
			ret->autorelease();
			return ret;
		}
		AX_SAFE_DELETE(ret);
		return nullptr;
	}

	bool init(float value, std::function<void(float)> changed, std::function<void()> ended)
	{
		if (!Node::init())
			return false;

		_changed = std::move(changed);
		_ended = std::move(ended);
		_value = std::clamp(value, 0.f, 1.f);

		_groove = loadFileSprite("slidergroove.png");
		_bar = loadFileSprite("sliderBar.png");
		_thumb = loadFileSprite("sliderthumb.png");
		if (!_thumb)
			_thumb = loadFileSprite("sliderthumbsel.png");

		if (!_groove)
			return true;

		_groove->setPosition({0.f, 0.f});
		addChild(_groove, 1);
		_halfW = _groove->getContentSize().width * 0.5f;
		_barWidth = std::max(_groove->getContentSize().width - 8.f, 8.f);
		_barHeight = _bar ? _bar->getContentSize().height : 8.f;

		if (_bar)
		{
			_bar->setStretchEnabled(false);
			_bar->getTexture()->setTexParameters({backend::SamplerFilter::NEAREST, backend::SamplerFilter::NEAREST,
												  backend::SamplerAddressMode::REPEAT, backend::SamplerAddressMode::REPEAT});
			_bar->setAnchorPoint({0.f, 0.5f});
			_bar->setPosition({4.f, _groove->getContentSize().height * 0.5f});
			_groove->addChild(_bar, -1);
		}

		if (_thumb)
			addChild(_thumb, 3);

		constexpr float kVisualWidth = 168.f;
		setScale(kVisualWidth / std::max(_groove->getContentSize().width, 1.f));

		refresh();

		auto* listener = EventListenerTouchOneByOne::create();
		listener->setSwallowTouches(true);
		listener->onTouchBegan = [this](Touch* touch, Event*) {
			const Vec2 world = touch->getLocation();
			const Vec2 center = convertToWorldSpace({0.f, 0.f});
			const float halfW = _halfW * getScaleX() + 10.f;
			const float halfH = 18.f;
			if (std::abs(world.x - center.x) > halfW || std::abs(world.y - center.y) > halfH)
				return false;
			setValueFromTouch(touch, true);
			return true;
		};
		listener->onTouchMoved = [this](Touch* touch, Event*) { setValueFromTouch(touch, true); };
		listener->onTouchEnded = [this](Touch* touch, Event*) {
			setValueFromTouch(touch, true);
			if (_ended)
				_ended();
		};
		Director::getInstance()->getEventDispatcher()->addEventListenerWithSceneGraphPriority(listener, this);
		return true;
	}

private:
	Sprite* _groove = nullptr;
	Sprite* _bar = nullptr;
	Sprite* _thumb = nullptr;
	float _barWidth = 80.f;
	float _barHeight = 8.f;
	float _halfW = 40.f;
	float _value = 1.f;
	std::function<void(float)> _changed;
	std::function<void()> _ended;

	void refresh()
	{
		if (_bar)
			_bar->setTextureRect({0.f, 0.f, _barWidth * _value, _barHeight});
		if (_thumb)
			_thumb->setPosition({(_value - 0.5f) * _barWidth, 0.f});
	}

	void setValueFromTouch(Touch* touch, bool notify)
	{
		const float localX = convertToNodeSpace(touch->getLocation()).x;
		_value = std::clamp(localX / std::max(_barWidth, 1.f) + 0.5f, 0.f, 1.f);
		refresh();
		if (notify && _changed)
			_changed(_value);
	}
};

MenuItemSpriteExtra* makeBtn(Node* spr, const std::function<void(Node*)>& cb)
{
	if (!spr)
		return nullptr;
	return MenuItemSpriteExtra::create(spr, cb);
}
} // namespace

PauseLayer* PauseLayer::create(PlayLayer* playLayer)
{
	auto* ret = new (std::nothrow) PauseLayer();
	if (ret && ret->init(playLayer))
	{
		ret->autorelease();
		return ret;
	}
	AX_SAFE_DELETE(ret);
	return nullptr;
}

bool PauseLayer::init(PlayLayer* playLayer)
{
	if (!LayerColor::initWithColor(Color4B(0, 0, 0, 150)))
		return false;

	_playLayer = playLayer;
	const auto& winSize = Director::getInstance()->getWinSize();
	setAnchorPoint(Vec2::ZERO);
	setPosition(Vec2::ZERO);
	setContentSize(winSize);
	setIgnoreAnchorPointForPosition(true);

	auto* listener = EventListenerTouchOneByOne::create();
	listener->setSwallowTouches(true);
	listener->onTouchBegan = [](Touch*, Event*) { return true; };
	Director::getInstance()->getEventDispatcher()->addEventListenerWithSceneGraphPriority(listener, this);

	std::string levelName = "Unknown";
	if (playLayer && playLayer->getLevel() && !playLayer->getLevel()->_levelName.empty())
		levelName = playLayer->getLevel()->_levelName;

	auto* title = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), levelName, TextHAlignment::CENTER);
	if (title)
	{
		title->setAnchorPoint({0.5f, 0.5f});
		GameToolbox::limitLabelWidth(title, 300.f, 0.8f, 0.3f);
		title->setPosition({winSize.width * 0.5f, winSize.height * 0.5f + 125.f});
		addChild(title, 2);
	}

	int normalBest = 0;
	int practiceBest = 0;
	if (playLayer && playLayer->getLevel())
	{
		if (auto* gm = GameManager::getInstance())
		{
			normalBest = gm->getLevelBest(playLayer->getLevel()->_levelID, false);
			practiceBest = gm->getLevelBest(playLayer->getLevel()->_levelID, true);
		}
	}

	addVanillaProgressBar(this, winSize.height * 0.5f + 70.f, normalBest, "Normal Mode", {0, 255, 0});
	addVanillaProgressBar(this, winSize.height * 0.5f + 20.f, practiceBest, "Practice Mode", {0, 255, 255});

	auto* menu = Menu::create();
	addChild(menu, 10);

	const bool practice = playLayer && playLayer->isPracticeMode();
	auto* practiceBtn = makeBtn(frameSprite(practice ? "GJ_normalBtn_001.png" : "GJ_practiceBtn_001.png",
											practice ? "Normal" : "Practice"),
								AX_CALLBACK_1(PauseLayer::onPractice, this));
	auto* resumeBtn = makeBtn(frameSprite("GJ_playBtn2_001.png", "Resume"), AX_CALLBACK_1(PauseLayer::onResume, this));
	auto* exitBtn = makeBtn(frameSprite("GJ_menuBtn_001.png", "Exit"), AX_CALLBACK_1(PauseLayer::onExitLevel, this));
	auto* replayBtn = makeBtn(frameSprite("GJ_replayBtn_001.png", "Restart"), AX_CALLBACK_1(PauseLayer::onRestart, this));

	if (practiceBtn)
		menu->addChild(practiceBtn);
	if (resumeBtn)
		menu->addChild(resumeBtn);
	if (exitBtn)
		menu->addChild(exitBtn);
	if (replayBtn)
		menu->addChild(replayBtn);

	const float btnY = -25.f;
	const float gap = 18.f;
	float totalW = 0.f;
	int count = 0;
	MenuItemSpriteExtra* row[] = {practiceBtn, resumeBtn, exitBtn, replayBtn};
	for (auto* item : row)
	{
		if (!item)
			continue;
		totalW += item->getContentSize().width;
		++count;
	}
	if (count > 1)
		totalW += gap * static_cast<float>(count - 1);

	float x = -totalW * 0.5f;
	for (auto* item : row)
	{
		if (!item)
			continue;
		const float w = item->getContentSize().width;
		item->setPosition({x + w * 0.5f, btnY});
		x += w + gap;
	}

	if (auto* optionsBtn = makeBtn(frameSprite("GJ_optionsBtn_001.png"), AX_CALLBACK_1(PauseLayer::onSettings, this)))
	{
		optionsBtn->setPosition(menu->convertToNodeSpace({winSize.width - 36.f, winSize.height - 36.f}));
		menu->addChild(optionsBtn);
	}

	auto* gm = GameManager::getInstance();
	const float musicVal = gm ? gm->getMusicVolume() : 1.f;
	const float sfxVal = gm ? gm->getSfxVolume() : 1.f;
	const float sliderY = 48.f;
	const float labelY = 73.f;
	const float musicX = winSize.width * 0.5f - 120.f;
	const float sfxX = winSize.width * 0.5f + 120.f;

	auto saveVolumes = []() {
		if (auto* mgr = GameManager::getInstance())
			mgr->save();
	};

	if (auto* musicLbl = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "Music"))
	{
		musicLbl->setScale(0.5f);
		musicLbl->setPosition({musicX, labelY});
		addChild(musicLbl, 5);
	}
	if (auto* musicSlider = VolumeSlider::create(
			musicVal,
			[this](float v) {
				if (auto* mgr = GameManager::getInstance())
					mgr->setMusicVolumePercent(static_cast<int>(v * 100.f + 0.5f));
				if (_playLayer)
					_playLayer->applyMusicVolume();
			},
			saveVolumes))
	{
		musicSlider->setPosition({musicX, sliderY});
		addChild(musicSlider, 6);
	}

	if (auto* sfxLbl = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "SFX"))
	{
		sfxLbl->setScale(0.5f);
		sfxLbl->setPosition({sfxX, labelY});
		addChild(sfxLbl, 5);
	}
	if (auto* sfxSlider = VolumeSlider::create(
			sfxVal,
			[](float v) {
				if (auto* mgr = GameManager::getInstance())
					mgr->setSfxVolumePercent(static_cast<int>(v * 100.f + 0.5f));
			},
			saveVolumes))
	{
		sfxSlider->setPosition({sfxX, sliderY});
		addChild(sfxSlider, 6);
	}

	return true;
}

void PauseLayer::showSoon(const char* featureName)
{
	auto* soon = ComingSoonLayer::create(featureName);
	if (!soon)
		return;
	addChild(soon, 50);
	soon->show();
}

namespace
{
void addSettingToggle(PauseLayer* pause, PlayLayer* playLayer, Menu* menu, float y, const char* title, bool* flag)
{
	if (!menu || !flag)
		return;

	const char* onFrame = "GJ_checkOn_001.png";
	const char* offFrame = "GJ_checkOff_001.png";
	auto* box = MenuItemSpriteExtra::create(*flag ? onFrame : offFrame, [playLayer, flag, onFrame, offFrame](Node* n) {
		*flag = !*flag;
		if (auto* item = dynamic_cast<MenuItemSpriteExtra*>(n))
			item->setSpriteFrame(*flag ? onFrame : offFrame);
		if (auto* gm = GameManager::getInstance())
			gm->save();
		if (playLayer)
			playLayer->applyHudVisibility();
	});
	if (!box)
		return;
	box->setPosition({-130.f, y});
	box->setScale(0.8f);
	menu->addChild(box);

	if (auto* label = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), title))
	{
		label->setAnchorPoint({0.f, 0.5f});
		label->setScale(0.35f);
		label->setPosition({-100.f, y});
		menu->addChild(label);
	}
}
} // namespace

void PauseLayer::onResume(Node*)
{
	if (_playLayer)
		_playLayer->resumeGame();
}

void PauseLayer::onRestart(Node*)
{
	if (!_playLayer)
		return;
	AudioEngine::stopAll();
	AudioEngine::play2d("playSound_01.ogg", false, 0.5f);
	_playLayer->resetLevel();
	_playLayer->resumeGame();
}

void PauseLayer::onExitLevel(Node*)
{
	if (_playLayer)
		_playLayer->exit();
}

void PauseLayer::onPractice(Node*)
{
	if (!_playLayer)
		return;
	_playLayer->togglePracticeMode();
	AudioEngine::stopAll();
	AudioEngine::play2d("playSound_01.ogg", false, 0.5f);
	_playLayer->resetLevel();
	_playLayer->resumeGame();
}

void PauseLayer::onSettings(Node*)
{
	auto* gm = GameManager::getInstance();
	if (!gm)
		return;

	auto* content = Layer::create();
	auto* menu = Menu::create();
	menu->setPosition({0.f, 0.f});
	addSettingToggle(this, _playLayer, menu, 70.f, "Show Hitboxes", &gm->_showHitboxes);
	addSettingToggle(this, _playLayer, menu, 35.f, "Show Hitboxes On Death", &gm->_showHitboxesOnDeath);
	addSettingToggle(this, _playLayer, menu, 0.f, "Show Progressbar", &gm->_showProgressBar);
	addSettingToggle(this, _playLayer, menu, -35.f, "Show Percentage", &gm->_showPercentage);
	addSettingToggle(this, _playLayer, menu, -70.f, "Auto Checkpoints", &gm->_autoCheckpoints);
	content->addChild(menu);

	auto* drop = DropDownLayer::create(content, "Settings");
	if (!drop)
		return;
	addChild(drop, 80);
	drop->showLayer(false, false);
}
