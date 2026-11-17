
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
	if(DoButton_CheckBox(&g_Config.m_ClBlackoutBlatantAvoid, Localize("Blatant Avoid"), g_Config.m_ClBlackoutBlatantAvoid, &Row))
		g_Config.m_ClBlackoutBlatantAvoid ^= 1;

	MainView.HSplitTop(8.0f, nullptr, &MainView);

	MainView.HSplitTop(30.0f, &Row, &MainView);
	if(DoButton_CheckBox(&g_Config.m_ClBlackoutBlatantDirection, Localize("Blatant Direction"), g_Config.m_ClBlackoutBlatantDirection, &Row))
		g_Config.m_ClBlackoutBlatantDirection ^= 1;

	MainView.HSplitTop(30.0f, &Row, &MainView);
	if(DoButton_CheckBox(&g_Config.m_ClBlackoutBlatantHook, Localize("Blatant Hook"), g_Config.m_ClBlackoutBlatantHook, &Row))
		g_Config.m_ClBlackoutBlatantHook ^= 1;

	MainView.HSplitTop(30.0f, &Row, &MainView);
	if(DoButton_CheckBox(&g_Config.m_ClBlackoutBlatantJump, Localize("Emergency Jump"), g_Config.m_ClBlackoutBlatantJump, &Row))
		g_Config.m_ClBlackoutBlatantJump ^= 1;

	MainView.HSplitTop(30.0f, &Row, &MainView);
	if(DoButton_CheckBox(&g_Config.m_ClBlackoutBlatantHookPulse, Localize("Hook Pulse"), g_Config.m_ClBlackoutBlatantHookPulse, &Row))
		g_Config.m_ClBlackoutBlatantHookPulse ^= 1;

	MainView.HSplitTop(30.0f, &Row, &MainView);
	Ui()->DoScrollbarOption(&g_Config.m_ClBlackoutBlatantHookPulseTicks, &g_Config.m_ClBlackoutBlatantHookPulseTicks, &Row, Localize("Hook Pulse Interval"), 2, 60);
}
