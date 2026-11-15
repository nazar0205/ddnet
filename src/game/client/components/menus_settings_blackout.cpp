
#include "menus.h"

#include <game/client/ui.h>
#include <game/localization.h>

void CMenus::RenderSettingsBlackout(CUIRect MainView)
{
	static int s_Enabled = 1;
	static int s_CustomHook = 1;
	static int s_BalanceBot = 0;
	static int s_FrostAvoid = 0;

	CUIRect Row;

	MainView.HSplitTop(35.0f, &Row, &MainView);
	Ui()->DoLabel(&Row, "BLACKOUT", 24.0f, TEXTALIGN_LEFT);

	MainView.HSplitTop(10.0f, nullptr, &MainView);

	MainView.HSplitTop(30.0f, &Row, &MainView);
	if(DoButton_CheckBox(&s_Enabled, Localize("Enable Blackout"), s_Enabled, &Row))
		s_Enabled ^= 1;

	MainView.HSplitTop(8.0f, nullptr, &MainView);

	MainView.HSplitTop(30.0f, &Row, &MainView);
	if(DoButton_CheckBox(&s_CustomHook, Localize("Custom Hook"), s_CustomHook, &Row))
		s_CustomHook ^= 1;

	MainView.HSplitTop(30.0f, &Row, &MainView);
	if(DoButton_CheckBox(&s_BalanceBot, Localize("Balance Bot"), s_BalanceBot, &Row))
		s_BalanceBot ^= 1;

	MainView.HSplitTop(30.0f, &Row, &MainView);
	if(DoButton_CheckBox(&s_FrostAvoid, Localize("Frost Avoid"), s_FrostAvoid, &Row))
		s_FrostAvoid ^= 1;
}
