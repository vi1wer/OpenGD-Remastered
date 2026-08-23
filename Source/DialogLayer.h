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

struct DialogPage
{
	std::string speaker;
	std::string text;
};

class DialogLayer : public PopupLayer
{
public:
	static DialogLayer* create(std::vector<DialogPage> pages, const char* portraitFrame = nullptr);
	void show(Transitions transitions = kScaleUp) override;
	void setOnClose(std::function<void()> callback) { _onClose = std::move(callback); }
	void close() override;

private:
	bool init(std::vector<DialogPage> pages, const char* portraitFrame);
	void setPage(int page);
	void advance();
	void refreshText();
	void tickTypewriter(float dt);
	void parseColored(const std::string& tagged, std::string& plain, std::vector<ax::Color3B>& colors);

	std::vector<DialogPage> _pages;
	std::string _plain;
	std::vector<ax::Color3B> _colors;
	std::function<void()> _onClose;
	ax::Label* _nameLabel = nullptr;
	ax::Label* _textLabel = nullptr;
	ax::Node* _portrait = nullptr;
	ax::Node* _nextArrow = nullptr;
	int _page = 0;
	int _visibleCount = 0;
	bool _closing = false;
};
