/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#pragma once

#include "PopupLayer.h"
#include "math/Vec2.h"
#include "base/Types.h"
#include <functional>
#include <string>
#include <vector>

namespace ax
{
	class Label;
	class Node;
	class Sprite;
}
namespace ax::ui
{
	class Scale9Sprite;
}

enum class DialogChatPlacement
{
	Center = 0,
	Top,
	Bottom,
};

enum class DialogAnimationType
{
	FromCenter = 0,
	FromLeft,
	FromRight,
	FromTop,
	FromTop2,
};

struct DialogPage
{
	std::string speaker;
	std::string text;
	int characterFrame = 1; // dialogIcon_%03d.png
	float textScale = 1.f;
	bool unskippable = false;
	ax::Color3B nameColor = ax::Color3B::WHITE;
};

class DialogLayer : public PopupLayer
{
public:
	// background: 1..7 → GJ_square%02d.png (official DialogLayer)
	static DialogLayer* create(std::vector<DialogPage> pages, int background = 2);
	static DialogLayer* create(std::vector<DialogPage> pages, const char* portraitFrame, int background = 2);

	void show(Transitions transitions = kNone) override;
	void setOnClose(std::function<void()> callback) { _onClose = std::move(callback); }
	void setAnimationType(DialogAnimationType type) { _animationType = type; }
	void setChatPlacement(DialogChatPlacement placement);
	void close() override;

private:
	bool init(std::vector<DialogPage> pages, int background, const char* portraitOverride);
	void setPage(int page);
	void advance();
	void refreshText();
	void tickTypewriter(float dt);
	void parseColored(const std::string& tagged, std::string& plain, std::vector<ax::Color3B>& colors);
	void updateNavButtonFrame();
	void animateIn(DialogAnimationType type);
	ax::Sprite* loadPortrait(int frame) const;

	std::vector<DialogPage> _pages;
	std::string _plain;
	std::vector<ax::Color3B> _colors;
	std::function<void()> _onClose;
	ax::ui::Scale9Sprite* _bg = nullptr;
	ax::ui::Scale9Sprite* _textBg = nullptr;
	ax::Label* _nameLabel = nullptr;
	ax::Label* _textLabel = nullptr;
	ax::Sprite* _portrait = nullptr;
	ax::Sprite* _navButton = nullptr;
	ax::Vec2 _mainPos = ax::Vec2::ZERO;
	DialogAnimationType _animationType = DialogAnimationType::FromCenter;
	DialogChatPlacement _placement = DialogChatPlacement::Center;
	int _background = 2;
	int _page = 0;
	int _visibleCount = 0;
	bool _closing = false;
	bool _animating = false;
	bool _pressConsumed = false;
	std::string _portraitOverride;
};
