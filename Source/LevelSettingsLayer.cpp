/*************************************************************************
	OpenGD - Open source Geometry Dash.
	Copyright (C) 2023  OpenGD Team
*************************************************************************/

#include "LevelSettingsLayer.h"

#include "BaseGameLayer.h"
#include "ButtonSprite.h"
#include "GameToolbox/getTextureString.h"
#include "GJGameLevel.h"
#include "GroundLayer.h"
#include "LevelEditorLayer.h"
#include "LevelTools.h"
#include "MenuItemSpriteExtra.h"
#include "PlayerObject.h"
#include "SpriteColor.h"

#include "2d/Label.h"
#include "2d/Menu.h"
#include "2d/Sprite.h"
#include "2d/SpriteFrameCache.h"
#include "base/Director.h"
#include "math/Rect.h"
#include "ui/UIScale9Sprite.h"

#include <algorithm>
#include <fmt/format.h>
#include <functional>
#include <string>
#include <vector>

USING_NS_AX;

namespace
{
	constexpr int kChBG = 1000;
	constexpr int kChG = 1001;
	constexpr int kChLine = 1002;
	constexpr int kChG2 = 1003;
	constexpr int kChMG = 1009;
	constexpr int kChMG2 = 1014;
	constexpr int kMaxOfficialSong = 21;
	constexpr int kMaxBg = 59;
	constexpr int kMaxGround = 22;
	constexpr int kMaxMg = 12;
	constexpr int kMaxFont = 12;

	// GD SelectSettingLayer StartingSpeed: display boost_01..05; value 0<->idx1, value 1<->idx0.
	int speedValueToIdx(int value)
	{
		switch (std::clamp(value, 0, 4))
		{
		case 0: return 1;
		case 1: return 0;
		default: return value;
		}
	}

	int speedIdxToValue(int idx)
	{
		switch (std::clamp(idx, 0, 4))
		{
		case 0: return 1;
		case 1: return 0;
		default: return idx;
		}
	}

	const char* speedFrameByIdx(int idx)
	{
		static const char* kFrames[] = {
			"boost_01_001.png",
			"boost_02_001.png",
			"boost_03_001.png",
			"boost_04_001.png",
			"boost_05_001.png",
		};
		return kFrames[std::clamp(idx, 0, 4)];
	}

	const char* speedFrame(int speed)
	{
		return speedFrameByIdx(speedValueToIdx(speed));
	}

	const char* modeFrame(PlayerGamemode mode)
	{
		switch (mode)
		{
		case PlayerGamemodeShip: return "gj_shipBtn_off_001.png";
		case PlayerGamemodeBall: return "gj_ballBtn_off_001.png";
		case PlayerGamemodeUFO: return "gj_birdBtn_off_001.png";
		case PlayerGamemodeWave: return "gj_dartBtn_off_001.png";
		case PlayerGamemodeRobot: return "gj_robotBtn_off_001.png";
		case PlayerGamemodeSpider: return "gj_spiderBtn_off_001.png";
		case PlayerGamemodeSwing: return "gj_swingBtn_off_001.png";
		default: return "gj_iconBtn_off_001.png";
		}
	}

	Sprite* tryFrameSprite(const char* frame, float scale = 1.f)
	{
		if (!SpriteFrameCache::getInstance()->getSpriteFrameByName(frame))
			return nullptr;
		auto* spr = Sprite::createWithSpriteFrameName(frame);
		if (spr)
			spr->setScale(scale);
		return spr;
	}

	Sprite* tryAnySprite(const char* name)
	{
		if (SpriteFrameCache::getInstance()->getSpriteFrameByName(name))
			return Sprite::createWithSpriteFrameName(name);
		if (auto* spr = Sprite::create(GameToolbox::getTextureString(name)))
			return spr;
		return nullptr;
	}

	void fitSprite(Sprite* spr, float maxSide)
	{
		if (!spr || maxSide <= 0.f)
			return;
		const auto cs = spr->getContentSize();
		const float m = std::max(cs.width, cs.height);
		if (m > 0.f)
			spr->setScale(maxSide / m);
	}

	void cropToSquare(Sprite* spr, float displaySize)
	{
		if (!spr)
			return;
		const auto cs = spr->getContentSize();
		if (cs.width <= 0.f || cs.height <= 0.f)
			return;
		const float side = std::min(cs.width, cs.height);
		spr->setTextureRect(Rect((cs.width - side) * 0.5f, (cs.height - side) * 0.28f, side, side));
		spr->setScale(displaySize / side);
	}

	const char* toggleTex(bool selected)
	{
		return selected ? "GJ_button_01.png" : "GJ_button_04.png";
	}

	ButtonSprite* makeBigBtn(const char* text, int width, bool selected, float scale = 0.55f, float height = 24.f)
	{
		return ButtonSprite::create(
			text, width, 0, scale, true, GameToolbox::getTextureString("goldFont.fnt"),
			GameToolbox::getTextureString(toggleTex(selected)), height);
	}

	Sprite* artIcon(const char* kind, int id, float size)
	{
		const int clamped = std::max(1, id);
		Sprite* spr = tryAnySprite(fmt::format("{}_{:02}_001.png", kind, clamped).c_str());
		if (!spr)
			spr = tryAnySprite(fmt::format("{}_01_001.png", kind).c_str());
		if (spr)
			fitSprite(spr, size);
		return spr;
	}

	class ColorPickPopup : public PopupLayer
	{
	public:
		static ColorPickPopup* create(LevelEditorLayer* editor, int channel, std::string title,
			std::function<void()> onChanged)
		{
			auto* ret = new (std::nothrow) ColorPickPopup();
			if (ret && ret->init(editor, channel, std::move(title), std::move(onChanged)))
			{
				ret->autorelease();
				return ret;
			}
			AX_SAFE_DELETE(ret);
			return nullptr;
		}

	private:
		bool init(LevelEditorLayer* editor, int channel, std::string title, std::function<void()> onChanged)
		{
			if (!PopupLayer::init())
				return false;
			_editor = editor;
			_channel = channel;
			_onChanged = std::move(onChanged);

			const auto winSize = Director::getInstance()->getWinSize();
			auto* bg = ui::Scale9Sprite::create(GameToolbox::getTextureString("GJ_square01.png"));
			if (!bg)
				return false;
			bg->setContentSize({260.f, 180.f});
			bg->setPosition(winSize * 0.5f);
			_mainLayer->addChild(bg);

			if (auto* lbl = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), title.empty() ? "Color" : title))
			{
				lbl->setScale(0.5f);
				lbl->setPosition({winSize.width * 0.5f, winSize.height * 0.5f + 65.f});
				_mainLayer->addChild(lbl);
			}

			_preview = Sprite::create(GameToolbox::getTextureString("square02_001.png"));
			if (!_preview)
				_preview = Sprite::create(GameToolbox::getTextureString("square01_001.png"));
			if (_preview)
			{
				_preview->setScale(1.2f);
				_preview->setPosition({winSize.width * 0.5f, winSize.height * 0.5f + 25.f});
				_mainLayer->addChild(_preview);
			}

			_menu = Menu::create();
			_menu->setPosition({0.f, 0.f});
			_mainLayer->addChild(_menu);

			auto addRow = [this, winSize](const char* name, int which, float y) {
				if (auto* lbl = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), name))
				{
					lbl->setScale(0.35f);
					lbl->setPosition({winSize.width * 0.5f - 70.f, winSize.height * 0.5f + y});
					_mainLayer->addChild(lbl);
				}
				auto make = [this, which, winSize, y](const char* frame, float x, int delta) {
					Node* spr = tryFrameSprite(frame, 0.5f);
					if (!spr)
						return;
					auto* btn = MenuItemSpriteExtra::create(spr, [this, which, delta](Node*) {
						bump(which, delta);
					});
					btn->setPosition({x, winSize.height * 0.5f + y});
					_menu->addChild(btn);
				};
				make("edit_leftBtn2_001.png", winSize.width * 0.5f + 10.f, -8);
				make("edit_rightBtn2_001.png", winSize.width * 0.5f + 50.f, 8);
			};

			addRow("R", 0, -10.f);
			addRow("G", 1, -40.f);
			addRow("B", 2, -70.f);

			auto* okSpr = ButtonSprite::create("OK", 60, 0, 0.5f);
			auto* ok = MenuItemSpriteExtra::create(okSpr, [this](Node*) { close(); });
			ok->setPosition({winSize.width * 0.5f, winSize.height * 0.5f - 70.f});
			_menu->addChild(ok);

			refreshPreview();
			return true;
		}

		void bump(int which, int delta)
		{
			if (!_editor)
				return;
			auto& ch = _editor->colorChannels()[ _channel ];
			auto& c = ch._color;
			auto apply = [&](uint8_t& v) {
				int n = static_cast<int>(v) + delta;
				v = static_cast<uint8_t>(std::clamp(n, 0, 255));
			};
			if (which == 0)
				apply(c.r);
			else if (which == 1)
				apply(c.g);
			else
				apply(c.b);
			refreshPreview();
			_editor->applyLevelSettingsVisuals();
			if (_onChanged)
				_onChanged();
		}

		void refreshPreview()
		{
			if (!_editor || !_preview)
				return;
			auto it = _editor->colorChannels().find(_channel);
			if (it != _editor->colorChannels().end())
				_preview->setColor(it->second._color);
		}

		LevelEditorLayer* _editor = nullptr;
		int _channel = 1000;
		std::function<void()> _onChanged;
		Menu* _menu = nullptr;
		Sprite* _preview = nullptr;
	};

	class OptionsPopup : public PopupLayer
	{
	public:
		static OptionsPopup* create(LevelEditorLayer* editor)
		{
			auto* ret = new (std::nothrow) OptionsPopup();
			if (ret && ret->init(editor))
			{
				ret->autorelease();
				return ret;
			}
			AX_SAFE_DELETE(ret);
			return nullptr;
		}

	private:
		bool init(LevelEditorLayer* editor)
		{
			if (!PopupLayer::init())
				return false;
			_editor = editor;
			const auto winSize = Director::getInstance()->getWinSize();
			auto* bg = ui::Scale9Sprite::create(GameToolbox::getTextureString("GJ_square01.png"));
			if (!bg)
				return false;
			bg->setContentSize({280.f, 220.f});
			bg->setPosition(winSize * 0.5f);
			_mainLayer->addChild(bg);

			if (auto* title = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "Options"))
			{
				title->setScale(0.55f);
				title->setPosition({winSize.width * 0.5f, winSize.height * 0.5f + 85.f});
				_mainLayer->addChild(title);
			}

			_menu = Menu::create();
			_menu->setPosition({0.f, 0.f});
			_mainLayer->addChild(_menu);
			rebuild();
			return true;
		}

		void rebuild()
		{
			_menu->removeAllChildren();
			const auto winSize = Director::getInstance()->getWinSize();
			auto& s = _editor->levelSettings();
			struct Row
			{
				const char* name;
				bool* flag;
			};
			Row rows[] = {
				{"Mini Mode", &s.mini},
				{"Dual Mode", &s.dual},
				{"2-Player", &s.twoPlayer},
				{"Flip Gravity", &s.flipGravity},
			};
			float y = 40.f;
			for (auto& row : rows)
			{
				const bool on = *row.flag;
				const std::string caption = fmt::format("{}: {}", row.name, on ? "ON" : "OFF");
				auto* spr = makeBigBtn(caption.c_str(), 180, on, 0.38f, 28.f);
				bool* flag = row.flag;
				auto* btn = MenuItemSpriteExtra::create(spr, [this, flag](Node*) {
					*flag = !*flag;
					rebuild();
				});
				btn->setPosition({winSize.width * 0.5f, winSize.height * 0.5f + y});
				_menu->addChild(btn);
				y -= 35.f;
			}
			auto* okSpr = makeBigBtn("OK", 70, true, 0.45f, 28.f);
			auto* ok = MenuItemSpriteExtra::create(okSpr, [this](Node*) { close(); });
			ok->setPosition({winSize.width * 0.5f, winSize.height * 0.5f - 85.f});
			_menu->addChild(ok);
		}

		LevelEditorLayer* _editor = nullptr;
		Menu* _menu = nullptr;
	};

	// Mirrors GD SelectSettingLayer for StartingSpeed UI.
	class SelectSpeedPopup : public PopupLayer
	{
	public:
		static SelectSpeedPopup* create(LevelEditorLayer* editor, std::function<void()> onChanged)
		{
			auto* ret = new (std::nothrow) SelectSpeedPopup();
			if (ret && ret->init(editor, std::move(onChanged)))
			{
				ret->autorelease();
				return ret;
			}
			AX_SAFE_DELETE(ret);
			return nullptr;
		}

	private:
		bool init(LevelEditorLayer* editor, std::function<void()> onChanged)
		{
			if (!PopupLayer::init() || !editor)
				return false;
			_editor = editor;
			_onChanged = std::move(onChanged);
			_selectedIdx = speedValueToIdx(_editor->levelSettings().speed);

			const auto winSize = Director::getInstance()->getWinSize();
			auto* bg = ui::Scale9Sprite::create(GameToolbox::getTextureString("GJ_square01.png"), Rect(0, 0, 80, 80));
			if (!bg)
				bg = ui::Scale9Sprite::create(GameToolbox::getTextureString("GJ_square01.png"));
			if (!bg)
				return false;
			bg->setContentSize({340.f, 160.f});
			bg->setPosition(winSize * 0.5f);
			_mainLayer->addChild(bg);

			if (auto* title = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "Select Speed"))
			{
				title->setScale(0.55f);
				title->setPosition({winSize.width * 0.5f, winSize.height * 0.5f + 55.f});
				_mainLayer->addChild(title);
			}

			_menu = Menu::create();
			_menu->setPosition({0.f, 0.f});
			_mainLayer->addChild(_menu);
			rebuildIcons();

			auto* okSpr = ButtonSprite::create(
				"OK", 40, 0, 0.7f, true, GameToolbox::getTextureString("goldFont.fnt"),
				GameToolbox::getTextureString("GJ_button_01.png"), 30.f);
			if (okSpr)
			{
				auto* ok = MenuItemSpriteExtra::create(okSpr, [this](Node*) { close(); });
				ok->setPosition({winSize.width * 0.5f, winSize.height * 0.5f - 52.f});
				_menu->addChild(ok);
			}
			return true;
		}

		void rebuildIcons()
		{
			for (auto* btn : _iconBtns)
			{
				if (btn)
					btn->removeFromParent();
			}
			_iconBtns.clear();

			const auto winSize = Director::getInstance()->getWinSize();
			constexpr float kSpacing = 52.f;
			const float startX = winSize.width * 0.5f - 2.f * kSpacing;
			for (int idx = 0; idx < 5; ++idx)
			{
				Sprite* spr = tryAnySprite(speedFrameByIdx(idx));
				if (!spr)
					continue;
				const bool selected = idx == _selectedIdx;
				fitSprite(spr, 32.f);
				// Unselected = darkened gray so the active speed is obvious
				spr->setColor(selected ? Color3B::WHITE : Color3B(70, 70, 70));
				const int captured = idx;
				auto* btn = MenuItemSpriteExtra::create(spr, [this, captured](Node*) { selectIdx(captured); });
				btn->setPosition({startX + idx * kSpacing, winSize.height * 0.5f + 5.f});
				_menu->addChild(btn);
				_iconBtns.push_back(btn);
			}
		}

		void selectIdx(int idx)
		{
			_selectedIdx = std::clamp(idx, 0, 4);
			_editor->levelSettings().speed = speedIdxToValue(_selectedIdx);
			if (_onChanged)
				_onChanged();
			rebuildIcons();
		}

		LevelEditorLayer* _editor = nullptr;
		std::function<void()> _onChanged;
		Menu* _menu = nullptr;
		int _selectedIdx = 1;
		std::vector<MenuItemSpriteExtra*> _iconBtns;
	};

	Node* makeModeButtonSprite(PlayerGamemode mode, bool selected)
	{
		Sprite* spr = tryAnySprite(modeFrame(mode));
		if (!spr)
			spr = tryAnySprite("gj_iconBtn_off_001.png");
		if (!spr)
			return nullptr;
		fitSprite(spr, 36.f);
		// Same *_off texture: selected bright, unselected darkened gray
		spr->setColor(selected ? Color3B::WHITE : Color3B(75, 75, 75));
		return spr;
	}

	class SelectModePopup : public PopupLayer
	{
	public:
		static SelectModePopup* create(LevelEditorLayer* editor, std::function<void()> onChanged)
		{
			auto* ret = new (std::nothrow) SelectModePopup();
			if (ret && ret->init(editor, std::move(onChanged)))
			{
				ret->autorelease();
				return ret;
			}
			AX_SAFE_DELETE(ret);
			return nullptr;
		}

	private:
		bool init(LevelEditorLayer* editor, std::function<void()> onChanged)
		{
			if (!PopupLayer::init() || !editor)
				return false;
			_editor = editor;
			_onChanged = std::move(onChanged);
			_selected = _editor->levelSettings().gamemode;

			const auto winSize = Director::getInstance()->getWinSize();
			auto* bg = ui::Scale9Sprite::create(GameToolbox::getTextureString("GJ_square01.png"), Rect(0, 0, 80, 80));
			if (!bg)
				bg = ui::Scale9Sprite::create(GameToolbox::getTextureString("GJ_square01.png"));
			if (!bg)
				return false;
			bg->setContentSize({400.f, 160.f});
			bg->setPosition(winSize * 0.5f);
			_mainLayer->addChild(bg);

			if (auto* title = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "Select Mode"))
			{
				title->setScale(0.55f);
				title->setPosition({winSize.width * 0.5f, winSize.height * 0.5f + 55.f});
				_mainLayer->addChild(title);
			}

			_menu = Menu::create();
			_menu->setPosition({0.f, 0.f});
			_mainLayer->addChild(_menu);
			rebuildIcons();

			auto* okSpr = ButtonSprite::create(
				"OK", 40, 0, 0.7f, true, GameToolbox::getTextureString("goldFont.fnt"),
				GameToolbox::getTextureString("GJ_button_01.png"), 30.f);
			if (okSpr)
			{
				auto* ok = MenuItemSpriteExtra::create(okSpr, [this](Node*) { close(); });
				ok->setPosition({winSize.width * 0.5f, winSize.height * 0.5f - 52.f});
				_menu->addChild(ok);
			}
			return true;
		}

		void rebuildIcons()
		{
			for (auto* btn : _iconBtns)
			{
				if (btn)
					btn->removeFromParent();
			}
			_iconBtns.clear();

			static constexpr PlayerGamemode kModes[] = {
				PlayerGamemodeCube, PlayerGamemodeShip, PlayerGamemodeBall, PlayerGamemodeUFO,
				PlayerGamemodeWave, PlayerGamemodeRobot, PlayerGamemodeSpider, PlayerGamemodeSwing,
			};

			const auto winSize = Director::getInstance()->getWinSize();
			constexpr float kSpacing = 42.f;
			const float startX = winSize.width * 0.5f - 3.5f * kSpacing;
			for (int i = 0; i < 8; ++i)
			{
				const PlayerGamemode mode = kModes[i];
				Node* spr = makeModeButtonSprite(mode, mode == _selected);
				if (!spr)
					continue;
				auto* btn = MenuItemSpriteExtra::create(spr, [this, mode](Node*) { selectMode(mode); });
				btn->setPosition({startX + i * kSpacing, winSize.height * 0.5f + 5.f});
				_menu->addChild(btn);
				_iconBtns.push_back(btn);
			}
		}

		void selectMode(PlayerGamemode mode)
		{
			_selected = mode;
			_editor->levelSettings().gamemode = mode;
			if (_onChanged)
				_onChanged();
			rebuildIcons();
		}

		LevelEditorLayer* _editor = nullptr;
		std::function<void()> _onChanged;
		Menu* _menu = nullptr;
		PlayerGamemode _selected = PlayerGamemodeCube;
		std::vector<MenuItemSpriteExtra*> _iconBtns;
	};

	class MoreColorsPopup : public PopupLayer
	{
	public:
		static MoreColorsPopup* create(LevelEditorLayer* editor, std::function<void()> onChanged)
		{
			auto* ret = new (std::nothrow) MoreColorsPopup();
			if (ret && ret->init(editor, std::move(onChanged)))
			{
				ret->autorelease();
				return ret;
			}
			AX_SAFE_DELETE(ret);
			return nullptr;
		}

	private:
		bool init(LevelEditorLayer* editor, std::function<void()> onChanged)
		{
			if (!PopupLayer::init() || !editor)
				return false;
			_editor = editor;
			_onChanged = std::move(onChanged);
			const auto winSize = Director::getInstance()->getWinSize();

			auto* bg = ui::Scale9Sprite::create(GameToolbox::getTextureString("GJ_square01.png"));
			if (!bg)
				return false;
			bg->setContentSize({300.f, 240.f});
			bg->setPosition(winSize * 0.5f);
			_mainLayer->addChild(bg);

			if (auto* title = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "MORE COLORS"))
			{
				title->setScale(0.45f);
				title->setPosition({winSize.width * 0.5f, winSize.height * 0.5f + 95.f});
				_mainLayer->addChild(title);
			}

			_menu = Menu::create();
			_menu->setPosition({0.f, 0.f});
			_mainLayer->addChild(_menu);

			static constexpr int kIds[] = {1000, 1001, 1002, 1003, 1004, 1005, 1006, 1007, 1009, 1010, 1011, 1012, 1013, 1014};
			float y = 60.f;
			float x = -70.f;
			int col = 0;
			for (int id : kIds)
			{
				if (!_editor->colorChannels().contains(id))
					_editor->colorChannels()[id] = SpriteColor(Color3B::WHITE, 255, false);
				const std::string caption = fmt::format("{}", id);
				auto* spr = ButtonSprite::create(caption, 55, 0, 0.32f);
				spr->setColor(_editor->colorChannels()[id]._color);
				auto* btn = MenuItemSpriteExtra::create(spr, [this, id](Node*) {
					if (auto* pop = ColorPickPopup::create(_editor, id, fmt::format("Color {}", id), [this] {
							if (_onChanged)
								_onChanged();
						}))
						pop->show();
				});
				btn->setPosition({winSize.width * 0.5f + x, winSize.height * 0.5f + y});
				_menu->addChild(btn);
				x += 70.f;
				if (++col >= 3)
				{
					col = 0;
					x = -70.f;
					y -= 36.f;
				}
			}

			auto* okSpr = ButtonSprite::create("OK", 70, 0, 0.5f);
			auto* ok = MenuItemSpriteExtra::create(okSpr, [this](Node*) { close(); });
			ok->setPosition({winSize.width * 0.5f, winSize.height * 0.5f - 100.f});
			_menu->addChild(ok);
			return true;
		}

		LevelEditorLayer* _editor = nullptr;
		std::function<void()> _onChanged;
		Menu* _menu = nullptr;
	};
} // namespace

LevelSettingsLayer* LevelSettingsLayer::create(LevelEditorLayer* editor)
{
	auto* ret = new (std::nothrow) LevelSettingsLayer();
	if (ret && ret->init(editor))
	{
		ret->autorelease();
		return ret;
	}
	AX_SAFE_DELETE(ret);
	return nullptr;
}

bool LevelSettingsLayer::init(LevelEditorLayer* editor)
{
	if (!PopupLayer::init() || !editor)
		return false;
	_editor = editor;
	ensureChannels();
	auto* level = _editor->getLevel();
	_customSong = level && level->_songID > 0;
	buildUI();
	refreshUI();
	return true;
}

void LevelSettingsLayer::ensureChannels()
{
	auto& ch = _editor->colorChannels();
	if (!ch.contains(kChBG))
		ch[kChBG] = SpriteColor(Color3B::WHITE, 255, false);
	if (!ch.contains(kChG))
		ch[kChG] = SpriteColor(Color3B(0, 102, 255), 255, false);
	if (!ch.contains(kChLine))
		ch[kChLine] = SpriteColor(Color3B::WHITE, 255, false);
	if (!ch.contains(kChG2))
		ch[kChG2] = SpriteColor(Color3B(60, 60, 60), 255, false);
	if (!ch.contains(kChMG))
		ch[kChMG] = SpriteColor(Color3B::WHITE, 255, false);
	if (!ch.contains(kChMG2))
		ch[kChMG2] = SpriteColor(Color3B(100, 100, 100), 255, false);
}

ButtonSprite* LevelSettingsLayer::makeToggleButton(const char* text, int width, bool selected)
{
	return makeBigBtn(text, width, selected);
}

void LevelSettingsLayer::setToggleSelected(ButtonSprite* btn, bool selected)
{
	if (!btn)
		return;
	btn->updateBGImage(GameToolbox::getTextureString(toggleTex(selected)));
}

Sprite* LevelSettingsLayer::makeColorSwatch(int channelId, float size)
{
	Sprite* spr = tryAnySprite("GJ_colorBtn_001.png");
	if (!spr)
		spr = tryAnySprite("square02_001.png");
	if (!spr)
		return nullptr;
	fitSprite(spr, size);
	auto it = _editor->colorChannels().find(channelId);
	if (it != _editor->colorChannels().end())
		spr->setColor(it->second._color);
	return spr;
}

void LevelSettingsLayer::buildUI()
{
	const auto winSize = Director::getInstance()->getWinSize();
	const Vec2 c = winSize * 0.5f;

	// Exact floats from GeometryDash 2.2081 LevelSettingsLayer::init (RVA 0x31EA20)
	// and SongSelectNode::init (RVA 0x0C6DD0).
	auto* panel = ui::Scale9Sprite::create(GameToolbox::getTextureString("GJ_square01.png"), Rect(0, 0, 80, 80));
	if (!panel)
		panel = ui::Scale9Sprite::create(GameToolbox::getTextureString("GJ_square01.png"));
	if (!panel)
		return;
	panel->setContentSize({455.f, 305.f});
	panel->setPosition(c);
	_mainLayer->addChild(panel);

	_menu = Menu::create();
	_menu->setPosition({0.f, 0.f});
	_mainLayer->addChild(_menu);

	auto addLabel = [&](const char* text, float x, float y, float scale, bool gold) -> Label* {
		const char* font = gold ? "goldFont.fnt" : "bigFont.fnt";
		auto* lbl = Label::createWithBMFont(GameToolbox::getTextureString(font), text);
		if (!lbl)
			return nullptr;
		lbl->setScale(scale);
		lbl->setPosition({c.x + x, c.y + y});
		_mainLayer->addChild(lbl);
		return lbl;
	};

	auto addItem = [&](Node* spr, float x, float y, std::function<void(Node*)> cb) -> MenuItemSpriteExtra* {
		if (!spr)
			return nullptr;
		auto* btn = MenuItemSpriteExtra::create(spr, std::move(cb));
		btn->setPosition({c.x + x, c.y + y});
		_menu->addChild(btn);
		return btn;
	};

	auto makeSongToggle = [&](const char* text, bool selected) -> ButtonSprite* {
		return ButtonSprite::create(
			text, 50, 0, 0.4f, true, GameToolbox::getTextureString("bigFont.fnt"),
			GameToolbox::getTextureString(toggleTex(selected)), 20.f);
	};

	// --- Select Color (W/2, H/2+135), goldFont 0.8 ---
	addLabel("Select Color:", 0.f, 135.f, 0.8f, true);

	// Color labels at SelectColor.Y - 20; swatches at label.Y - 22
	struct SwatchSlot
	{
		const char* name;
		int channel;
		Sprite** out;
		float x;
	};
	const SwatchSlot slots[] = {
		{"BG:", kChBG, &_swatchBG, -138.f},
		{"G:", kChG, &_swatchG, -92.f},
		{"G2:", kChG2, &_swatchG2, -46.f},
		{"Line:", kChLine, &_swatchLine, 0.f},
		{"MG:", kChMG, &_swatchMG, 46.f},
		{"MG2:", kChMG2, &_swatchMG2, 92.f},
	};
	for (const auto& s : slots)
	{
		addLabel(s.name, s.x, 115.f, 0.35f, false);
		*s.out = makeColorSwatch(s.channel, 22.f);
		if (*s.out)
		{
			const int ch = s.channel;
			const char* title = s.name;
			addItem(*s.out, s.x, 93.f, [this, ch, title](Node*) { openColorPicker(ch, title); });
		}
	}
	addLabel("More:", 138.f, 115.f, 0.35f, false);
	if (Sprite* moreSpr = tryAnySprite("GJ_colorBtn_001.png"))
	{
		fitSprite(moreSpr, 22.f);
		moreSpr->setColor({20, 20, 20});
		if (auto* plus = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "+"))
		{
			plus->setScale(0.45f);
			plus->setPosition(moreSpr->getContentSize() * 0.5f);
			moreSpr->addChild(plus);
		}
		addItem(moreSpr, 138.f, 93.f, [this](Node*) { openMoreColors(); });
	}

	// --- Left column: X = W/2-185; Speed Y = H/2+135, Mode +55, Options -25 ---
	addLabel("Speed:", -185.f, 135.f, 0.6f, true);
	_speedIcon = tryAnySprite(speedFrame(_editor->levelSettings().speed));
	if (_speedIcon)
	{
		fitSprite(_speedIcon, 28.f);
		addItem(_speedIcon, -185.f, 101.f, [this](Node*) { openSpeedSelect(); });
	}

	addLabel("Mode:", -185.f, 55.f, 0.6f, true);
	_modeIcon = tryAnySprite(modeFrame(_editor->levelSettings().gamemode));
	if (_modeIcon)
	{
		fitSprite(_modeIcon, 28.f);
		addItem(_modeIcon, -185.f, 23.f, [this](Node*) { openModeSelect(); });
	}

	addLabel("Options:", -185.f, -25.f, 0.6f, true);
	Node* gear = tryAnySprite("GJ_optionsBtn_001.png");
	if (!gear)
		gear = tryAnySprite("GJ_optionsBtn02_001.png");
	if (auto* gearSpr = dynamic_cast<Sprite*>(gear))
		gearSpr->setScale(0.6f);
	if (!gear)
		gear = makeBigBtn("Opt", 40, true, 0.4f, 24.f);
	if (gear)
		addItem(gear, -185.f, -57.f, [this](Node*) { openOptions(); });

	// --- Game Type: label at base+(0,28)=+59; Classic/Platformer at base (±65,+31) ---
	// base point from init is (W/2, H/2+31); label is then moved +28 on Y.
	addLabel("Game Type:", 0.f, 59.f, 0.7f, true);
	const bool plat = _editor->levelSettings().platformer;
	_classicBtn = ButtonSprite::create(
		"Classic", 100, 0, 0.55f, true, GameToolbox::getTextureString("goldFont.fnt"),
		GameToolbox::getTextureString(toggleTex(!plat)), 24.f);
	_platformerBtn = ButtonSprite::create(
		"Platformer", 100, 0, 0.55f, true, GameToolbox::getTextureString("goldFont.fnt"),
		GameToolbox::getTextureString(toggleTex(plat)), 24.f);
	if (_classicBtn)
		addItem(_classicBtn, -65.f, 31.f, [this](Node*) { setPlatformer(false); });
	if (_platformerBtn)
		addItem(_platformerBtn, 65.f, 31.f, [this](Node*) { setPlatformer(true); });

	// --- Right column: X = W/2+185; BG +135, G +70, MG +5; icons label.Y-32; Font at -70 ---
	addLabel("BG:", 185.f, 135.f, 0.6f, true);
	_bgPreview = artIcon("bgIcon", _editor->levelSettings()._bgID, 36.f);
	if (!_bgPreview)
	{
		_bgPreview = Sprite::create(GameToolbox::getTextureString(
			fmt::format("game_bg_{:02}_001.png", std::max(1, _editor->levelSettings()._bgID))));
		if (_bgPreview)
			cropToSquare(_bgPreview, 36.f);
	}
	if (_bgPreview)
		addItem(_bgPreview, 185.f, 103.f, [this](Node*) { cycleBg(1); });

	addLabel("G:", 185.f, 70.f, 0.6f, true);
	_gPreview = artIcon("gIcon", _editor->levelSettings()._groundID, 36.f);
	if (!_gPreview)
	{
		_gPreview = Sprite::create(GameToolbox::getTextureString(
			fmt::format("groundSquare_{:02}_001.png", std::max(1, _editor->levelSettings()._groundID))));
		if (_gPreview)
			cropToSquare(_gPreview, 36.f);
	}
	if (_gPreview)
		addItem(_gPreview, 185.f, 38.f, [this](Node*) { cycleGround(1); });

	addLabel("MG:", 185.f, 5.f, 0.6f, true);
	const int mgId = _editor->levelSettings()._mgID;
	if (mgId > 0)
		_mgPreview = artIcon("mgIcon", mgId, 36.f);
	if (!_mgPreview)
	{
		_mgPreview = tryAnySprite("GJ_colorBtn_001.png");
		if (_mgPreview)
		{
			fitSprite(_mgPreview, 36.f);
			_mgPreview->setColor({10, 10, 10});
		}
	}
	if (_mgPreview)
	{
		_mgNoneMark = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "x");
		if (_mgNoneMark)
		{
			_mgNoneMark->setScale(0.5f);
			_mgNoneMark->setPosition(_mgPreview->getContentSize() * 0.5f);
			_mgNoneMark->setVisible(mgId <= 0);
			_mgPreview->addChild(_mgNoneMark);
		}
		addItem(_mgPreview, 185.f, -27.f, [this](Node*) { cycleMg(1); });
	}

	// Font button text is always "Font"; width ~0x1E height 28 scale 0.8
	_fontBtn = ButtonSprite::create(
		"Font", 40, 0, 0.8f, true, GameToolbox::getTextureString("goldFont.fnt"),
		GameToolbox::getTextureString("GJ_button_04.png"), 28.f);
	if (_fontBtn)
		addItem(_fontBtn, 185.f, -70.f, [this](Node*) { cycleFont(1); });

	// --- SongSelectNode: ref (W/2, H/2-67); children via ccpAdd(ref, local) ---
	// Select Song at ref+(-65,+60) => ( -65, -7 )
	// Song bg Y = selectSong.Y - 58 => -65; Normal at selectSong+(~+40,-1); Custom +62
	if (auto* songTitle = Label::createWithBMFont(GameToolbox::getTextureString("goldFont.fnt"), "Select Song:"))
	{
		songTitle->setScale(0.8f);
		songTitle->setPosition({c.x - 65.f, c.y - 7.f});
		_mainLayer->addChild(songTitle);
	}

	_normalSongBtn = makeSongToggle("Normal", !_customSong);
	_customSongBtn = makeSongToggle("Custom", _customSong);
	if (_normalSongBtn)
		addItem(_normalSongBtn, -25.f, -8.f, [this](Node*) { setCustomSong(false); });
	if (_customSongBtn)
		addItem(_customSongBtn, 37.f, -8.f, [this](Node*) { setCustomSong(true); });

	auto* songBox = ui::Scale9Sprite::create(GameToolbox::getTextureString("square02_001.png"));
	if (!songBox)
		songBox = ui::Scale9Sprite::create(GameToolbox::getTextureString("square02b_001.png"));
	if (songBox)
	{
		songBox->setContentSize({300.f, 36.f});
		songBox->setColor({0, 0, 0});
		songBox->setOpacity(180);
		songBox->setPosition({c.x, c.y - 65.f});
		_mainLayer->addChild(songBox);
	}

	_songLabel = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "");
	if (_songLabel)
	{
		_songLabel->setScale(0.4f);
		_songLabel->setPosition({c.x, c.y - 65.f});
		_mainLayer->addChild(_songLabel);
	}

	auto addSongArrow = [&](const char* frame, float xOff, int delta) {
		Sprite* arrow = tryAnySprite(frame);
		if (!arrow)
			return;
		arrow->setScale(1.2f);
		addItem(arrow, xOff, -65.f, [this, delta](Node*) { cycleSong(delta); });
	};
	addSongArrow("edit_leftBtn_001.png", -120.f, -1);
	addSongArrow("edit_rightBtn_001.png", 120.f, 1);

	// OK: (W/2, H/2-130), GJ_button_01, width 40, height 30, scale 0.8
	auto* okSpr = ButtonSprite::create(
		"OK", 40, 0, 0.8f, true, GameToolbox::getTextureString("goldFont.fnt"),
		GameToolbox::getTextureString("GJ_button_01.png"), 30.f);
	if (okSpr)
		addItem(okSpr, 0.f, -130.f, [this](Node*) { onOk(); });
}

void LevelSettingsLayer::refreshUI()
{
	updateSpeedIcon();
	updateModeIcon();
	updateSongLabel();
	updatePreviewSprites();
	updateGameTypeButtons();
	updateSongModeButtons();
	updateSwatches();
}

void LevelSettingsLayer::updateSwatches()
{
	auto paint = [&](Sprite* s, int ch) {
		if (!s)
			return;
		auto it = _editor->colorChannels().find(ch);
		if (it != _editor->colorChannels().end())
			s->setColor(it->second._color);
	};
	paint(_swatchBG, kChBG);
	paint(_swatchG, kChG);
	paint(_swatchG2, kChG2);
	paint(_swatchLine, kChLine);
	paint(_swatchMG, kChMG);
	paint(_swatchMG2, kChMG2);
}

void LevelSettingsLayer::updateSpeedIcon()
{
	if (!_speedIcon)
		return;
	const char* frame = speedFrame(_editor->levelSettings().speed);
	if (SpriteFrameCache::getInstance()->getSpriteFrameByName(frame))
		_speedIcon->setSpriteFrame(frame);
	fitSprite(_speedIcon, 30.f);
}

void LevelSettingsLayer::updateModeIcon()
{
	if (!_modeIcon)
		return;
	const char* frame = modeFrame(_editor->levelSettings().gamemode);
	if (SpriteFrameCache::getInstance()->getSpriteFrameByName(frame))
		_modeIcon->setSpriteFrame(frame);
	fitSprite(_modeIcon, 28.f);
}

void LevelSettingsLayer::updateSongLabel()
{
	if (!_songLabel || !_editor->getLevel())
		return;
	auto* level = _editor->getLevel();
	if (_customSong)
	{
		const int id = std::max(1, level->_songID);
		_songLabel->setString(fmt::format("ID: {}", id));
	}
	else
	{
		const int id = level->_officialSongID != 0 ? level->_officialSongID : level->_musicID;
		const int clamped = std::clamp(id, 0, kMaxOfficialSong);
		// SongSelectNode format: "%02d: %s"
		_songLabel->setString(fmt::format("{:02}: {}", clamped, LevelTools::getAudioTitle(clamped)));
	}
	_songLabel->setScale(0.45f);
	const float maxW = 200.f;
	if (_songLabel->getContentSize().width * _songLabel->getScale() > maxW)
		_songLabel->setScale(maxW / _songLabel->getContentSize().width);
}

void LevelSettingsLayer::updatePreviewSprites()
{
	auto& s = _editor->levelSettings();
	auto reloadArt = [&](Sprite* spr, const char* kind, int id, float size) {
		if (!spr)
			return;
		const int clamped = std::max(1, id);
		const std::string frame = fmt::format("{}_{:02}_001.png", kind, clamped);
		if (SpriteFrameCache::getInstance()->getSpriteFrameByName(frame))
		{
			spr->setSpriteFrame(frame);
			fitSprite(spr, size);
			spr->setColor(Color3B::WHITE);
			return;
		}
		auto* tmp = Sprite::create(GameToolbox::getTextureString(
			kind[0] == 'b' ? fmt::format("game_bg_{:02}_001.png", clamped)
						   : fmt::format("groundSquare_{:02}_001.png", clamped)));
		if (tmp && tmp->getTexture())
		{
			spr->setTexture(tmp->getTexture());
			spr->setTextureRect(tmp->getTextureRect());
			cropToSquare(spr, size);
		}
	};

	if (_bgPreview)
	{
		reloadArt(_bgPreview, "bgIcon", s._bgID, 36.f);
		auto it = _editor->colorChannels().find(kChBG);
		if (it != _editor->colorChannels().end() &&
			!SpriteFrameCache::getInstance()->getSpriteFrameByName(fmt::format("bgIcon_{:02}_001.png", std::max(1, s._bgID))))
			_bgPreview->setColor(it->second._color);
	}
	if (_gPreview)
	{
		reloadArt(_gPreview, "gIcon", s._groundID, 36.f);
		auto it = _editor->colorChannels().find(kChG);
		if (it != _editor->colorChannels().end() &&
			!SpriteFrameCache::getInstance()->getSpriteFrameByName(fmt::format("gIcon_{:02}_001.png", std::max(1, s._groundID))))
			_gPreview->setColor(it->second._color);
	}
	if (_mgPreview)
	{
		const bool none = s._mgID <= 0;
		if (!none)
		{
			const std::string frame = fmt::format("mgIcon_{:02}_001.png", s._mgID);
			if (SpriteFrameCache::getInstance()->getSpriteFrameByName(frame))
			{
				_mgPreview->setSpriteFrame(frame);
				fitSprite(_mgPreview, 36.f);
				_mgPreview->setColor(Color3B::WHITE);
			}
		}
		else
			_mgPreview->setColor({10, 10, 10});
		if (_mgNoneMark)
			_mgNoneMark->setVisible(none);
	}
}

void LevelSettingsLayer::updateGameTypeButtons()
{
	const bool plat = _editor->levelSettings().platformer;
	setToggleSelected(_classicBtn, !plat);
	setToggleSelected(_platformerBtn, plat);
}

void LevelSettingsLayer::updateSongModeButtons()
{
	setToggleSelected(_normalSongBtn, !_customSong);
	setToggleSelected(_customSongBtn, _customSong);
}

void LevelSettingsLayer::cycleSpeed(int delta)
{
	auto& s = _editor->levelSettings().speed;
	s = (s + delta) % 5;
	if (s < 0)
		s += 5;
	updateSpeedIcon();
}

void LevelSettingsLayer::openSpeedSelect()
{
	if (auto* pop = SelectSpeedPopup::create(_editor, [this] { updateSpeedIcon(); }))
		pop->show();
}

void LevelSettingsLayer::openModeSelect()
{
	if (auto* pop = SelectModePopup::create(_editor, [this] { updateModeIcon(); }))
		pop->show();
}

void LevelSettingsLayer::cycleMode(int delta)
{
	int m = static_cast<int>(_editor->levelSettings().gamemode) + delta;
	constexpr int kCount = static_cast<int>(PlayerGamemodeSwing) + 1;
	m %= kCount;
	if (m < 0)
		m += kCount;
	_editor->levelSettings().gamemode = static_cast<PlayerGamemode>(m);
	updateModeIcon();
}

void LevelSettingsLayer::cycleFont(int delta)
{
	auto& f = _editor->levelSettings()._fontID;
	f = (f + delta) % (kMaxFont + 1);
	if (f < 0)
		f += kMaxFont + 1;
}

void LevelSettingsLayer::cycleBg(int delta)
{
	auto& id = _editor->levelSettings()._bgID;
	id += delta;
	if (id < 1)
		id = kMaxBg;
	if (id > kMaxBg)
		id = 1;
	_editor->applyLevelSettingsVisuals();
	updatePreviewSprites();
}

void LevelSettingsLayer::cycleGround(int delta)
{
	auto& id = _editor->levelSettings()._groundID;
	id += delta;
	if (id < 1)
		id = kMaxGround;
	if (id > kMaxGround)
		id = 1;
	_editor->applyLevelSettingsVisuals();
	updatePreviewSprites();
}

void LevelSettingsLayer::cycleMg(int delta)
{
	auto& id = _editor->levelSettings()._mgID;
	id += delta;
	if (id < 0)
		id = kMaxMg;
	if (id > kMaxMg)
		id = 0;
	_editor->applyLevelSettingsVisuals();
	updatePreviewSprites();
}

void LevelSettingsLayer::setPlatformer(bool on)
{
	_editor->levelSettings().platformer = on;
	updateGameTypeButtons();
}

void LevelSettingsLayer::setCustomSong(bool on)
{
	_customSong = on;
	auto* level = _editor->getLevel();
	if (!level)
		return;
	if (_customSong)
	{
		if (level->_songID <= 0)
			level->_songID = 1;
	}
	else
	{
		level->_songID = 0;
		if (level->_officialSongID == 0 && level->_musicID == 0)
			level->_musicID = 0;
	}
	updateSongModeButtons();
	updateSongLabel();
}

void LevelSettingsLayer::cycleSong(int delta)
{
	auto* level = _editor->getLevel();
	if (!level)
		return;
	if (_customSong)
	{
		level->_songID = std::max(1, level->_songID + delta);
	}
	else
	{
		int id = level->_officialSongID != 0 ? level->_officialSongID : level->_musicID;
		id = std::clamp(id + delta, 0, kMaxOfficialSong);
		level->_officialSongID = id;
		level->_musicID = id;
		level->_songID = 0;
	}
	updateSongLabel();
}

void LevelSettingsLayer::openOptions()
{
	if (auto* pop = OptionsPopup::create(_editor))
		pop->show();
}

void LevelSettingsLayer::openColorPicker(int channelId, const char* title)
{
	if (auto* pop = ColorPickPopup::create(_editor, channelId, title ? std::string(title) : std::string("Color"), [this] {
			updateSwatches();
			updatePreviewSprites();
		}))
		pop->show();
}

void LevelSettingsLayer::openMoreColors()
{
	if (auto* pop = MoreColorsPopup::create(_editor, [this] {
			updateSwatches();
			updatePreviewSprites();
		}))
		pop->show();
}

void LevelSettingsLayer::onOk()
{
	_editor->applyLevelSettingsVisuals();
	_editor->saveLevelString();
	close();
}
