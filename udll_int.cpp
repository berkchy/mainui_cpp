/*
dll_int.cpp - dll entry point
Copyright (C) 2010 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/


#include "extdll_menu.h"
#include "BaseMenu.h"
#include "Utils.h"
#include "cl_dll/IGameClientExports.h"
#include "interface.h"
#include "font/FontManager.h"
#include "dlls/cdll_dll.h"

#define GAMEMENUEXPORTS_INTERFACE_VERSION "GameMenuExports001"

ui_enginefuncs_t EngFuncs::engfuncs;
ui_extendedfuncs_t EngFuncs::textfuncs;
ui_globalvars_t	*gpGlobals;
IGameClientExports *g_pClient;
CMenu gMenu;

extern void UI_JoinGame_Show( int param1, int param2 );
extern void UI_JoinClassT_Show( int param1, int param2 );
extern void UI_JoinClassCT_Show( int param1, int param2 );
extern void UI_BuyMenu_Show( int param1, int param2 );
extern void UI_BuyMenu_Pistol_Show( int param1, int param2 );
extern void UI_BuyMenu_Shotgun_Show( int param1, int param2 );
extern void UI_BuyMenu_Submachine_Show( int param1, int param2 );
extern void UI_BuyMenu_Rifle_Show( int param1, int param2 );
extern void UI_BuyMenu_Machinegun_Show( int param1, int param2 );
extern void UI_BuyMenu_Item_Show( int param1, int param2 );

static UI_FUNCTIONS gFunctionTable = 
{
	UI_VidInit,
	UI_Init,
	UI_Shutdown,
	UI_UpdateMenu,
	UI_KeyEvent,
	UI_MouseMove,
	UI_SetActiveMenu,
	UI_AddServerToList,
	UI_GetCursorPos,
	UI_SetCursorPos,
	UI_ShowCursor,
	UI_CharEvent,
	UI_MouseInRect,
	UI_IsVisible,
	UI_CreditsActive,
	UI_FinalCredits
};

//=======================================================================
//			GetApi
//=======================================================================
extern "C" EXPORT int GetMenuAPI(UI_FUNCTIONS *pFunctionTable, ui_enginefuncs_t* pEngfuncsFromEngine, ui_globalvars_t *pGlobals)
{
	if( !pFunctionTable || !pEngfuncsFromEngine )
	{
		return false;
	}

	// copy HUD_FUNCTIONS table to engine, copy engfuncs table from engine
	memcpy( pFunctionTable, &gFunctionTable, sizeof( UI_FUNCTIONS ));
	memcpy( &EngFuncs::engfuncs, pEngfuncsFromEngine, sizeof( ui_enginefuncs_t ));
	memset( &EngFuncs::textfuncs, 0, sizeof( ui_extendedfuncs_t ));

	gpGlobals = pGlobals;

	return true;
}

static UI_EXTENDED_FUNCTIONS gExtendedTable =
{
	AddTouchButtonToList,
	UI_MenuResetPing_f,
	UI_ConnectionWarning_f,
	UI_UpdateDialog,
	UI_ShowMessageBox,
	UI_ConnectionProgress_Disconnect,
	UI_ConnectionProgress_Download,
	UI_ConnectionProgress_DownloadEnd,
	UI_ConnectionProgress_Precache,
	UI_ConnectionProgress_Connect,
	UI_ConnectionProgress_ChangeLevel,
	UI_ConnectionProgress_ParseServerInfo
};

extern "C" EXPORT int GetExtAPI( int version, UI_EXTENDED_FUNCTIONS *pFunctionTable, ui_extendedfuncs_t *pEngfuncsFromEngine )
{
	if( !pFunctionTable || !pEngfuncsFromEngine )
	{
		return false;
	}

	if( version != MENU_EXTENDED_API_VERSION )
	{
		Con_Printf( "Error: failed to initialize extended menu API. Expected by DLL: %d. Got from engine: %d\n", MENU_EXTENDED_API_VERSION, version );
		return false;
	}

	memcpy( &EngFuncs::textfuncs, pEngfuncsFromEngine, sizeof( ui_extendedfuncs_t ) );
	memcpy( pFunctionTable, &gExtendedTable, sizeof( UI_EXTENDED_FUNCTIONS ));

	return true;
}

class IGameMenuExportsCompat : public IBaseInterface
{
public:
	virtual bool Initialize( CreateInterfaceFn appFactory ) = 0;
	virtual const char *L( const char *szStr ) = 0;
	virtual bool IsActive() = 0;
	virtual bool IsMainMenuActive() = 0;
	virtual void Key( int key, int down ) = 0;
	virtual void MouseMove( int x, int y ) = 0;
	virtual HFont BuildFont( CFontBuilder &builder ) = 0;
	virtual void GetCharABCWide( HFont font, int ch, int &a, int &b, int &c ) = 0;
	virtual int GetFontTall( HFont font ) = 0;
	virtual int GetCharacterWidth( HFont font, int ch, int charH ) = 0;
	virtual void GetTextSize( HFont font, const char *text, int *wide, int *height, int size ) = 0;
	virtual int GetTextHeight( HFont font, const char *text, int size ) = 0;
	virtual int DrawCharacter( HFont font, int ch, int x, int y, int charH, const unsigned int color, bool forceAdditive ) = 0;
	virtual void SetupScoreboard( int xstart, int xend, int ystart, int yend, unsigned int color, bool drawStroke ) = 0;
	virtual void DrawScoreboard() = 0;
	virtual void DrawSpectatorMenu() = 0;
	virtual void ShowVGUIMenu( int menuType, int param1, int param2 ) = 0;
	virtual void HideVGUIMenu() = 0;
};

static class CGameMenuExports : public IGameMenuExportsCompat
{
public:
	bool Initialize( CreateInterfaceFn appFactory ) override
	{
		if( !appFactory )
			return false;

		g_pClient = static_cast<IGameClientExports *>( appFactory( GAMECLIENTEXPORTS_INTERFACE_VERSION, NULL ) );
		return g_pClient != NULL;
	}

	const char *L( const char *szStr ) override
	{
		return ::L( szStr );
	}

	bool IsActive() override
	{
		if( !uiStatic.initialized )
			return false;
		return uiStatic.client.IsActive() && !uiStatic.menu.IsActive();
	}

	bool IsMainMenuActive() override
	{
		if( !uiStatic.initialized )
			return false;
		return uiStatic.menu.IsActive();
	}

	void Key( int keynum, int down ) override
	{
		UI_KeyEvent( keynum, down );
	}

	void MouseMove( int x, int y ) override
	{
		UI_MouseMove( x, y );
	}

	HFont BuildFont( CFontBuilder &builder ) override
	{
		return builder.Create();
	}

	void GetCharABCWide( HFont font, int ch, int &a, int &b, int &c ) override
	{
		g_FontMgr->GetCharABCWide( font, ch, a, b, c );
	}

	int GetFontTall( HFont font ) override
	{
		return g_FontMgr->GetFontTall( font );
	}

	int GetCharacterWidth( HFont font, int ch, int charH ) override
	{
		return g_FontMgr->GetCharacterWidthScaled( font, ch, charH );
	}

	void GetTextSize( HFont font, const char *text, int *wide, int *height, int size ) override
	{
		g_FontMgr->GetTextSize( font, text, wide, height, size );
	}

	int GetTextHeight( HFont font, const char *text, int size ) override
	{
		return g_FontMgr->GetTextHeight( font, text, size );
	}

	int DrawCharacter( HFont font, int ch, int x, int y, int charH, const unsigned int color, bool forceAdditive ) override
	{
		return g_FontMgr->DrawCharacter( font, ch, Point( x, y ), charH, color, forceAdditive );
	}

	void SetupScoreboard( int xstart, int xend, int ystart, int yend, unsigned int color, bool drawStroke ) override
	{
	}

	void DrawScoreboard() override
	{
	}

	void DrawSpectatorMenu() override
	{
	}

	void ShowVGUIMenu( int menuType, int param1, int param2 ) override
	{
		EngFuncs::KEY_ClearStates();

		switch( menuType )
		{
		case MENU_TEAM: UI_JoinGame_Show( param1, param2 ); break;
		case MENU_CLASS_T: UI_JoinClassT_Show( param1, param2 ); break;
		case MENU_CLASS_CT: UI_JoinClassCT_Show( param1, param2 ); break;
		case MENU_BUY: UI_BuyMenu_Show( param1, param2 ); break;
		case MENU_BUY_PISTOL: UI_BuyMenu_Pistol_Show( param1, param2 ); break;
		case MENU_BUY_SHOTGUN: UI_BuyMenu_Shotgun_Show( param1, param2 ); break;
		case MENU_BUY_SUBMACHINEGUN: UI_BuyMenu_Submachine_Show( param1, param2 ); break;
		case MENU_BUY_RIFLE: UI_BuyMenu_Rifle_Show( param1, param2 ); break;
		case MENU_BUY_MACHINEGUN: UI_BuyMenu_Machinegun_Show( param1, param2 ); break;
		case MENU_BUY_ITEM: UI_BuyMenu_Item_Show( param1, param2 ); break;
		default: break;
		}
	}

	void HideVGUIMenu() override
	{
		UI_CloseClientMenu();
	}
} s_Menu;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CGameMenuExports, CGameMenuExports, GAMEMENUEXPORTS_INTERFACE_VERSION, s_Menu );
