
#include "menus.h"

#include <game/client/ui.h>
#include <game/localization.h>

void CMenus::RenderSettingsBlackout(CUIRect MainView)
{
	CUIRect Row;

	MainView.HSplitTop(35.0f, &Row, &MainView);
	Ui()->DoLabel(&Row, "BLACKOUT", 24.0f, TEXTALIGN_LEFT);

	MainView.HSplitTop(10.0f, nullptr, &MainView);

	MainView.HSplitTop(30.0f, &Row, &MainView);
	if(DoButton_CheckBox(&g_Config.m_ClBlackout, Localize("Enable Blackout"), g_Config.m_ClBlackout, &Row))
		g_Config.m_ClBlackout ^= 1;

	MainView.HSplitTop(8.0f, nullptr, &MainView);

	MainView.HSplitTop(30.0f, &Row, &MainView);
	if(DoButton_CheckBox(&g_Config.m_ClBlackoutCustomHook, Localize("Custom Hook"), g_Config.m_ClBlackoutCustomHook, &Row))
		g_Config.m_ClBlackoutCustomHook ^= 1;

	MainView.HSplitTop(30.0f, &Row, &MainView);
	if(DoButton_CheckBox(&g_Config.m_ClBlackoutBalanceBot, Localize("Balance Bot"), g_Config.m_ClBlackoutBalanceBot, &Row))
		g_Config.m_ClBlackoutBalanceBot ^= 1;

	MainView.HSplitTop(30.0f, &Row, &MainView);
	if(DoButton_CheckBox(&g_Config.m_ClBlackoutFrostAvoid, Localize("Frost Avoid"), g_Config.m_ClBlackoutFrostAvoid, &Row))
		g_Config.m_ClBlackoutFrostAvoid ^= 1;
}
