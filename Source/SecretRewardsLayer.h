/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#pragma once

#include "2d/Scene.h"
#include "EventKeyboard.h"
#include "math/Vec2.h"
#include "base/Types.h"

namespace ax
{
	class Event;
	class Label;
	class Layer;
	class Menu;
	class Node;
	class Sprite;
}

class BoomScrollLayer;
class DialogLayer;

// Treasure Room (SecretRewardsLayer) — 1:1 layout with official GD 2.2.
class SecretRewardsLayer : public ax::Scene
{
public:
	static ax::Scene* scene(bool fromShop = false);
	static SecretRewardsLayer* create(bool fromShop = false);
	bool init(bool fromShop);
	void onKeyPressed(ax::EventKeyboard::KeyCode keyCode, ax::Event* event);

private:
	enum class MainPage
	{
		Tier1 = 0,
		Tier2 = 1,
		Large = 2,
		Gold = 3,
		Shops = 4,
		Count = 5,
	};

	struct ChestTier
	{
		int typeId; // 1..6
		int keyCost;
		int chestCount;
		const char* priceFrame;
		float scale;
	};

	struct LargeChest
	{
		int typeId; // 7..9
		int requiredOpened;
		const char* priceFrame;
	};

	struct ShopRope
	{
		int shopIndex; // 0 Scratch, 1 Community, 2 Mechanic, 3 Diamond
		int diamondCost;
		const char* ropeFrame;
		const char* ropeAltFrame;
		const char* keeperName;
		int dialogIcon;
	};

	struct ChestReward
	{
		int orbs = 0;
		int diamonds = 0;
		const char* label = nullptr;
	};

	void goBack();
	void refreshHud();
	void setBgColor(ax::Color3B color);
	void tryEnterRoom();
	void showLockedEntryDialog();
	void showWelcomeDialog();
	void buildMainHub();
	void onMainPageChanged(int page);
	ax::Layer* buildTierPage(bool tier2);
	ax::Layer* buildLargePage();
	ax::Layer* buildGoldPage();
	ax::Layer* buildShopsPage();
	void addTierChestButton(ax::Layer* page, ax::Menu* menu, int tierIndex, float x, float y);
	void onTierPressed(int tierIndex);
	void showSecondary(int tierIndex);
	void rebuildSecondaryPage();
	void onChestPressed(int tierIndex, int chestIndex);
	void onLargeChestPressed(int largeIndex);
	void onGoldChestPressed();
	void onShopRopePressed(int shopIndex);
	void openChestReward(int typeId, int chestIndex, int keyCost, bool gold);
	void showKeymasterLine(const std::string& text, std::function<void()> onClose = nullptr);
	ChestReward rewardForChest(int typeId, int chestIndex) const;
	ax::Sprite* makeClosedChestSprite(int typeId) const;
	ax::Sprite* makeOpenedChestSprite(int typeId) const;
	static const ChestTier* tiers();
	static int tierCount();
	static const LargeChest* largeChests();
	static const ShopRope* shopRopes();
	static ax::Color3B pageColor(int page);

	ax::Sprite* _bg = nullptr;
	ax::Node* _hud = nullptr;
	ax::Label* _keysLabel = nullptr;
	ax::Label* _goldKeysLabel = nullptr;
	ax::Label* _openedLabel = nullptr;
	ax::Menu* _navMenu = nullptr;
	ax::Node* _mainRoot = nullptr;
	ax::Node* _secondaryRoot = nullptr;
	BoomScrollLayer* _mainScroll = nullptr;
	DialogLayer* _dialog = nullptr;
	int _viewTier = -1;
	int _secondaryPage = 0;
	int _mainPage = 0;
	int _scratchDialogIndex = 0;
	int _potborDialogIndex = 0;
	int _mechanicDialogIndex = 0;
	int _diamondDialogIndex = 0;
	bool _fromShop = false;
	bool _inMainLayer = true;
	static constexpr int kChestsPerPage = 12; // 4×3 like official grids
};
