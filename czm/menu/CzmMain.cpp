/*
CzmMain.cpp - Counter Zombie Mobile main screen and in-game pause window
Copyright (C) 2026 Counter Zombie Mobile authors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "extdll_menu.h"
#include "BaseMenu.h"
#include "Utils.h"
#include "Framework.h"
#include "keydefs.h"
#include "CzmMenu.h"

#define UI_DIR "gfx/czm/ui/"
#define CZM_MAX_TILES 8
#define CZM_ABOUT_ROWS 6

#define CZM_TELEGRAM_URL "https://t.me/blackmodru"
#define CZM_GROUP_URL "https://t.me/counter_zombie_mobile"
#define CZM_SOURCE_URL "https://github.com/kurostreet/cs16-client"

namespace
{

// the bell counts unread news and an update waiting to be installed
int UnreadNotifications( void )
{
	return CZM_NewsUnread() + ( CZM_Update() ? 1 : 0 );
}

// the day this build was made, 11.10.2026
void BuildDate( char *out, size_t size )
{
	static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
	const char *date = __DATE__; // "Oct 11 2026"
	char month[4] = { date[0], date[1], date[2], 0 };
	const char *m = strstr( months, month );

	snprintf( out, size, "%02d.%02d.%s", atoi( date + 4 ), m ? (int)( m - months ) / 3 + 1 : 0, date + 7 );
}

} // namespace

/*
=================
CCzmMainMenu

Header (nick, bell, language), one big tile per game mode and the bottom navigation.
Everything is drawn by hand, the window has no stock items.
=================
*/
class CCzmMainMenu : public CMenuFramework
{
public:
	CCzmMainMenu();

	void Show() override;
	void Draw() override;
	void Think() override;
	bool KeyDown( int key ) override;
	bool KeyUp( int key ) override;
	void Char( int key ) override;

private:
	enum EOverlay
	{
		OV_NONE = 0,
		OV_QUIT,
		OV_BELL,
		OV_ABOUT,
		OV_CHAT,
		OV_SETUP, // first start: language and nickname
	};

	enum EHit
	{
		HIT_NONE = 0,
		HIT_PROFILE,
		HIT_BELL,
		HIT_LANG_RU,
		HIT_LANG_EN,
		HIT_BACKDROP,
		HIT_PANEL,
		HIT_CLOSE,
		HIT_QUIT_YES,
		HIT_QUIT_NO,
		HIT_VERSION,
		HIT_SETUP_RU,
		HIT_SETUP_EN,
		HIT_SETUP_NICK,
		HIT_SETUP_GO,
		HIT_ABOUT_TELEGRAM,
		HIT_ABOUT_GROUP,
		HIT_UPDATE,
		HIT_ABOUT_SOURCE,
		HIT_NAV,                    // 4 navigation items
		HIT_TILE = HIT_NAV + 4,     // one per server
		HIT_LAST = HIT_TILE + CZM_MAX_TILES
	};

	void Layout( void );
	int HitTest( void ) const;
	bool IsPressed( int hit ) const { return m_iPressed == hit && HitTest() == hit; }
	void Activate( int hit );

	void DrawTitle( void );
	void DrawHeader( void );
	void DrawTile( int index, CzmRect r );
	void DrawNav( void );
	void DrawOverlay( void );
	void DrawAbout( void );
	void DrawBell( void );
	void DrawSetup( void );
	void FinishSetup( void );

	int m_overlay;
	int m_iPressed;
	int m_iVersionTaps;
	double m_flVersionTapTime;

	// layout, recalculated every frame from the screen size
	int W, H, m_iMargin;
	char m_szNick[64];
	int m_iNickX, m_iNickW;
	int m_iTiles;
	int m_iPanelLineY;
	CzmRect m_avatar, m_profile, m_bell, m_lang;
	CzmRect m_tiles[CZM_MAX_TILES], m_nav[4];
	CzmRect m_panel, m_close, m_quitYes, m_quitNo, m_version;
	CzmRect m_aboutLogo, m_aboutRows[CZM_ABOUT_ROWS];
	CzmRect m_update, m_updateButton;
	CzmRect m_setupLang, m_setupNick, m_setupGo;
	CzmTextEdit m_nick;
	bool m_bNickWarning;
};

CCzmMainMenu::CCzmMainMenu() : CMenuFramework( "CzmMain" )
{
	m_overlay = OV_NONE;
	m_iPressed = HIT_NONE;
	m_iVersionTaps = 0;
	m_flVersionTapTime = 0.0;
	W = H = m_iMargin = 0;
	m_szNick[0] = 0;
	m_iNickX = m_iNickW = 0;
	m_iTiles = 0;
	m_iPanelLineY = 0;
	memset( &m_nick, 0, sizeof( m_nick ));
	m_bNickWarning = false;
}

void CCzmMainMenu::Show()
{
	m_overlay = OV_NONE;
	m_iPressed = HIT_NONE;
	CMenuFramework::Show();
	CZM_QueryServers( true );
	CZM_ApplyStartupSettings();

	if( CZM_FirstStart( ))
	{
		m_overlay = OV_SETUP;
		m_bNickWarning = false;
		memset( &m_nick, 0, sizeof( m_nick ));
	}

	// the engine build string would sit on the navigation bar, the version is in About
	EngFuncs::CvarSetValue( "scr_drawversion", 0.0f );

	// a game started from the server list ended: back to the list
	if( m_overlay == OV_NONE )
		CZM_ServerListReturn();
}

void CCzmMainMenu::Think()
{
	CMenuFramework::Think();

	// keep the tiles fresh while the player looks at them
	if( m_pStack->Current() == this && !CL_IsActive() && EngFuncs::GetCvarFloat( "czm_loading" ) == 0.0f )
		CZM_QueryServers( false );
}

void CCzmMainMenu::Layout( void )
{
	HFont nickFont = CZM_Font( CZMF_NICK );
	HFont hintFont = CZM_Font( CZMF_SMALL );
	int avatar, top, chip, langW, pencil, right;
	int tileW, tileH, gap, areaTop, areaBottom, total, y;

	W = ScreenWidth;
	H = ScreenHeight;
	m_iMargin = W * 0.03f;

	// header: avatar, nick and the hint under it
	avatar = H * 0.078f;
	top = H * 0.022f;
	m_avatar = CZM_Rect( m_iMargin, top, avatar, avatar );

	CZM_CleanName( EngFuncs::GetCvarString( "name" ), m_szNick, sizeof( m_szNick ));
	if( !m_szNick[0] )
		Q_strncpy( m_szNick, "Player", sizeof( m_szNick ));

	m_iNickX = m_iMargin + avatar + H * 0.022f;
	m_iNickW = Q_min( CZM_TextWidth( nickFont, m_szNick ), (int)( W * 0.34f ));
	pencil = H * 0.028f;
	right = Q_max( m_iNickX + m_iNickW + (int)( H * 0.012f ) + pencil, m_iNickX + CZM_TextWidth( hintFont, CZM_T( "nick_hint" )));
	m_profile = CZM_Rect( m_iMargin, top, right - m_iMargin, avatar );

	// header right: bell and language switch
	chip = H * 0.066f;
	langW = H * 0.22f;
	m_lang = CZM_Rect( W - m_iMargin - langW, top, langW, chip );
	m_bell = CZM_Rect( m_lang.x - H * 0.025f - chip, top, chip, chip );

	// mode tiles keep the picture aspect (3.7:1)
	m_iTiles = Q_min( CZM_ServerCount(), CZM_MAX_TILES );
	tileH = H * 0.205f;
	tileW = tileH * 37 / 10;
	if( tileW > W * 0.42f )
	{
		tileW = W * 0.42f;
		tileH = tileW * 10 / 37;
	}

	gap = H * 0.03f;
	areaTop = H * 0.13f;
	areaBottom = H * 0.87f;
	total = m_iTiles * tileH + Q_max( 0, m_iTiles - 1 ) * gap;
	if( total > areaBottom - areaTop && m_iTiles > 0 )
	{
		tileH = ( areaBottom - areaTop - ( m_iTiles - 1 ) * gap ) / m_iTiles;
		tileW = tileH * 37 / 10;
		total = m_iTiles * tileH + ( m_iTiles - 1 ) * gap;
	}

	y = areaTop + ( areaBottom - areaTop - total ) / 2;
	for( int i = 0; i < m_iTiles; i++ )
		m_tiles[i] = CZM_Rect( W - m_iMargin - tileW, y + i * ( tileH + gap ), tileW, tileH );

	// bottom navigation
	y = H * 0.885f;
	for( int i = 0; i < 4; i++ )
		m_nav[i] = CZM_Rect( W * i / 4, y, W * ( i + 1 ) / 4 - W * i / 4, H - y );

	// panels
	if( m_overlay == OV_SETUP )
	{
		int pad = H * 0.05f;
		int pw = H * 0.86f, ph = H * 0.70f;
		int lineH = H * 0.075f;

		m_panel = CZM_Rect(( W - pw ) / 2, ( H - ph ) / 2, pw, ph );
		m_setupLang = CZM_Rect( m_panel.x + ( pw - H * 0.30f ) / 2, m_panel.y + ph * 0.30f, H * 0.30f, lineH );
		m_setupNick = CZM_Rect( m_panel.x + pad, m_panel.y + ph * 0.50f, pw - pad * 2, lineH );
		m_setupGo = CZM_Rect( m_panel.x + ( pw - H * 0.40f ) / 2, m_panel.y + ph - pad - H * 0.085f, H * 0.40f, H * 0.085f );
	}
	else if( m_overlay == OV_QUIT )
	{
		int pad = H * 0.04f;
		int bh = H * 0.085f;
		int pw = H * 0.70f, ph = H * 0.36f;
		int bw;

		m_panel = CZM_Rect(( W - pw ) / 2, ( H - ph ) / 2, pw, ph );
		bw = ( pw - pad * 3 ) / 2;
		m_quitNo = CZM_Rect( m_panel.x + pad, m_panel.y + ph - pad - bh, bw, bh );
		m_quitYes = CZM_Rect( m_panel.x + pad * 2 + bw, m_quitNo.y, bw, bh );
	}
	else
	{
		bool about = m_overlay == OV_ABOUT, bell = m_overlay == OV_BELL;
		int pw = about || bell ? Q_min( (int)( H * 1.16f ), (int)( W * 0.80f )) : Q_min( (int)( H * 1.0f ), (int)( W * 0.62f ));
		int ph = about || bell ? H * 0.86f : H * 0.62f;
		int pad = H * 0.045f;
		int closeSize = H * 0.09f;
		int rowH = H * 0.07f, y;

		m_panel = CZM_Rect(( W - pw ) / 2, ( H - ph ) / 2, pw, ph );
		m_close = CZM_Rect( m_panel.x + pw - closeSize - H * 0.015f, m_panel.y + H * 0.015f, closeSize, closeSize );
		m_iPanelLineY = m_panel.y + pad * 0.8f + CZM_FontHeight( CZM_Font( CZMF_PANEL_TITLE )) + H * 0.02f;

		// About: logo, then the developer, the group, website, source code, version (five taps open the console), last update
		m_aboutLogo = CZM_Rect( m_panel.x + pad, m_iPanelLineY + H * 0.025f, pw - pad * 2, H * 0.085f );
		y = m_aboutLogo.y + m_aboutLogo.h + H * 0.02f;
		for( int i = 0; i < CZM_ABOUT_ROWS; i++ )
			m_aboutRows[i] = CZM_Rect( m_panel.x + pad, y + i * rowH, pw - pad * 2, rowH );
		m_version = m_aboutRows[4];

		// bell: the update card on top, the news under it
		m_update = CZM_Rect( m_panel.x + pad, m_iPanelLineY + H * 0.025f, pw - pad * 2, CZM_Update() ? (int)( H * 0.25f ) : 0 );
		m_updateButton = CZM_Rect( m_update.x + m_update.w - H * 0.03f - H * 0.36f, m_update.y + m_update.h - H * 0.03f - H * 0.08f,
			H * 0.36f, H * 0.08f );
	}
}

int CCzmMainMenu::HitTest( void ) const
{
	const int grow = ScreenHeight * 0.015f;

	if( m_overlay == OV_SETUP )
	{
		if( m_setupLang.HasCursor( ))
			return uiStatic.cursorX < m_setupLang.x + m_setupLang.w / 2 ? HIT_SETUP_RU : HIT_SETUP_EN;
		if( m_setupNick.HasCursor( ))
			return HIT_SETUP_NICK;
		if( m_setupGo.HasCursor( ))
			return HIT_SETUP_GO;
		return HIT_PANEL; // the window stays until the player finishes it
	}

	if( m_overlay != OV_NONE )
	{
		if( m_overlay == OV_QUIT )
		{
			if( m_quitYes.HasCursor( ))
				return HIT_QUIT_YES;
			if( m_quitNo.HasCursor( ))
				return HIT_QUIT_NO;
		}
		else
		{
			if( m_close.HasCursor( ))
				return HIT_CLOSE;
			if( m_overlay == OV_ABOUT && m_version.HasCursor( ))
				return HIT_VERSION;
			if( m_overlay == OV_ABOUT && m_aboutRows[0].HasCursor( ))
				return HIT_ABOUT_TELEGRAM;
			if( m_overlay == OV_ABOUT && m_aboutRows[1].HasCursor( ))
				return HIT_ABOUT_GROUP;
			if( m_overlay == OV_ABOUT && m_aboutRows[3].HasCursor( ))
				return HIT_ABOUT_SOURCE;
			if( m_overlay == OV_BELL && CZM_Update() && m_updateButton.Grown( grow ).HasCursor( ))
				return HIT_UPDATE;
		}

		return m_panel.HasCursor() ? HIT_PANEL : HIT_BACKDROP;
	}

	if( m_profile.Grown( grow ).HasCursor( ))
		return HIT_PROFILE;
	if( m_bell.Grown( grow ).HasCursor( ))
		return HIT_BELL;
	if( m_lang.Grown( grow ).HasCursor( ))
		return uiStatic.cursorX < m_lang.x + m_lang.w / 2 ? HIT_LANG_RU : HIT_LANG_EN;

	for( int i = 0; i < m_iTiles; i++ )
	{
		if( m_tiles[i].HasCursor( ))
			return HIT_TILE + i;
	}

	for( int i = 0; i < 4; i++ )
	{
		if( m_nav[i].HasCursor( ))
			return HIT_NAV + i;
	}

	return HIT_NONE;
}

void CCzmMainMenu::Activate( int hit )
{
	if( hit == HIT_PANEL || hit == HIT_NONE )
		return;

	if( hit != HIT_VERSION )
		CZM_PlayClick();

	switch( hit )
	{
	case HIT_PROFILE:
		CZM_OpenSettings( CZM_TAB_PROFILE );
		break;
	case HIT_NAV + 0:
		CZM_OpenChat();
		break;
	case HIT_NAV + 1:
		CZM_OpenServerList();
		break;
	case HIT_NAV + 2:
		CZM_OpenSettings( CZM_TAB_LAST );
		break;
	case HIT_ABOUT_TELEGRAM:
		EngFuncs::ShellExecute( CZM_TELEGRAM_URL, NULL, false );
		break;
	case HIT_ABOUT_GROUP:
		EngFuncs::ShellExecute( CZM_GROUP_URL, NULL, false );
		break;
	case HIT_ABOUT_SOURCE:
		EngFuncs::ShellExecute( CZM_SOURCE_URL, NULL, false );
		break;
	case HIT_SETUP_RU:
	case HIT_SETUP_EN:
		CZM_SetLanguage( hit == HIT_SETUP_RU );
		break;
	case HIT_SETUP_NICK:
		m_nick.Begin( m_nick.text, m_setupNick );
		break;
	case HIT_SETUP_GO:
		FinishSetup();
		break;
	case HIT_NAV + 3:
		m_overlay = OV_ABOUT;
		m_iVersionTaps = 0;
		break;
	case HIT_BELL:
		m_overlay = OV_BELL;
		CZM_NewsOpen();
		break;
	case HIT_UPDATE:
	{
		const CzmUpdateInfo *update = CZM_Update();
		int state;
		double done, total;
		char error[32];

		if( !update )
			break;
		CZM_UpdateState( &state, &done, &total, error, sizeof( error ));
		if( state == CZM_UPDATE_DOWNLOADING )
			CZM_UpdateCancel();
		else if( state == CZM_UPDATE_READY || state == CZM_UPDATE_PERMISSION )
			CZM_UpdateInstall();
		else if( state == CZM_UPDATE_IDLE || state == CZM_UPDATE_ERROR )
			CZM_UpdateStart( update->url, update->sha256, update->size );
		break;
	}
	case HIT_LANG_RU:
		CZM_SetLanguage( true );
		break;
	case HIT_LANG_EN:
		CZM_SetLanguage( false );
		break;
	case HIT_BACKDROP:
	case HIT_CLOSE:
	case HIT_QUIT_NO:
		// the news seen in the bell are read
		if( m_overlay == OV_BELL && !CZM_NewsLoading( ))
			CZM_NewsMarkRead();
		m_overlay = OV_NONE;
		break;
	case HIT_QUIT_YES:
		EngFuncs::ClientCmd( false, "quit\n" );
		break;
	case HIT_VERSION:
	{
		double now = EngFuncs::DoubleTime();

		if( now - m_flVersionTapTime > 2.0 )
			m_iVersionTaps = 0;

		m_flVersionTapTime = now;
		if( ++m_iVersionTaps >= 5 )
		{
			m_iVersionTaps = 0;
			m_overlay = OV_NONE;
			CZM_OpenConsole();
		}
		break;
	}
	default:
		if( hit >= HIT_TILE && hit < HIT_TILE + m_iTiles )
			CZM_ConnectToServer( hit - HIT_TILE );
		break;
	}
}

void CCzmMainMenu::FinishSetup( void )
{
	m_nick.End();

	if( !CZM_NickIsValid( m_nick.text ))
	{
		m_bNickWarning = true;
		return;
	}

	CZM_SetNick( m_nick.text );
	CZM_FinishFirstStart();
	m_overlay = OV_NONE;
}

void CCzmMainMenu::Char( int key )
{
	m_nick.Char( key );
	m_bNickWarning = false;
}

bool CCzmMainMenu::KeyDown( int key )
{
	Layout();

	if( m_nick.active && m_nick.KeyDown( key ))
	{
		m_bNickWarning = false;
		return true;
	}

	if( UI::Key::IsLeftMouse( key ))
	{
		m_iPressed = HitTest();

		// a tap outside the field hides the keyboard
		if( m_nick.active && m_iPressed != HIT_SETUP_NICK )
			m_nick.End();
		return true;
	}

	// the first start window stays until the player finishes it
	if( UI::Key::IsEscape( key ) && m_overlay == OV_SETUP )
	{
		m_nick.End();
		return true;
	}

	// Back on the phone: close a panel or ask before quitting
	if( UI::Key::IsEscape( key ))
	{
		m_overlay = ( m_overlay == OV_NONE ) ? OV_QUIT : OV_NONE;
		m_iPressed = HIT_NONE;
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_OUT] );
		return true;
	}

	return CMenuFramework::KeyDown( key );
}

bool CCzmMainMenu::KeyUp( int key )
{
	if( UI::Key::IsLeftMouse( key ))
	{
		int pressed = m_iPressed;
		int hit;

		Layout();
		hit = HitTest();
		m_iPressed = HIT_NONE;

		// act on release over the same element, a slide away cancels the tap
		if( hit != HIT_NONE && hit == pressed )
			Activate( hit );

		return true;
	}

	return CMenuFramework::KeyUp( key );
}

void CCzmMainMenu::DrawTitle( void )
{
	int aspect = CZM_ImageAspect1000( "gfx/czm/title.png" );
	int h = H * 0.12f;
	int maxW = ( m_iTiles > 0 ? m_tiles[0].x : W ) - m_iMargin - H * 0.03f;

	if( aspect <= 0 || maxW <= 0 )
		return;

	if( h * aspect / 1000 > maxW )
		h = maxW * 1000 / aspect;

	CZM_DrawImage( "gfx/czm/title.png", m_iMargin, H * 0.165f, h * aspect / 1000, h );
}

void CCzmMainMenu::DrawHeader( void )
{
	HFont nickFont = CZM_Font( CZMF_NICK );
	HFont hintFont = CZM_Font( CZMF_SMALL );
	HFont langFont = CZM_Font( CZMF_LANG );
	bool profilePressed = IsPressed( HIT_PROFILE );
	int nickY, pencil, iconSize, unread, inset, segment, textY;
	bool russian = CZM_IsRussian();

	// avatar, nick, hint
	CZM_AvatarPending(); // a photo from the gallery may have just arrived
	CZM_DrawImage( CZM_CurrentAvatarImage(), m_avatar.x, m_avatar.y, m_avatar.w, m_avatar.h, profilePressed ? PackRGBA( 190, 190, 190, 255 ) : CZM_White());
	nickY = m_avatar.y + m_avatar.h * 0.04f;
	CZM_DrawText( nickFont, m_iNickX, nickY, m_iNickW, m_szNick, profilePressed ? CZM_Grey() : CZM_White());
	pencil = H * 0.028f;
	CZM_DrawImage( UI_DIR "icon_pencil.png", m_iNickX + m_iNickW + H * 0.012f, nickY + ( CZM_FontHeight( nickFont ) - pencil ) / 2,
		pencil, pencil, PackRGBA( 200, 205, 215, 255 ));
	CZM_DrawText( hintFont, m_iNickX, m_avatar.y + m_avatar.h * 0.62f, W * 0.4f, CZM_T( "nick_hint" ), CZM_Grey());

	// bell with the unread counter
	CZM_DrawImage( UI_DIR "chip_round.png", m_bell.x, m_bell.y, m_bell.w, m_bell.h, IsPressed( HIT_BELL ) ? PackRGBA( 170, 170, 170, 255 ) : CZM_White());
	iconSize = m_bell.h * 0.58f;
	CZM_DrawImage( UI_DIR "icon_bell.png", m_bell.x + ( m_bell.w - iconSize ) / 2, m_bell.y + ( m_bell.h - iconSize ) / 2, iconSize, iconSize );

	unread = UnreadNotifications();
	if( unread > 0 )
	{
		HFont badgeFont = CZM_Font( CZMF_SMALL );
		int d = m_bell.h * 0.44f;
		int bx = m_bell.x + m_bell.w - d * 3 / 4;
		int by = m_bell.y - d / 4;
		char count[8];

		snprintf( count, sizeof( count ), unread > 9 ? "9+" : "%d", unread );
		CZM_DrawImage( UI_DIR "dot.png", bx, by, d, d, PackRGBA( 239, 68, 68, 255 ));
		CZM_DrawText( badgeFont, bx, by + ( d - CZM_FontHeight( badgeFont )) / 2, d, count, CZM_White(), QM_CENTER );
	}

	// RU | EN
	CZM_DrawPill( UI_DIR "chip_lang.png", m_lang.x, m_lang.y, m_lang.w, m_lang.h );
	inset = m_lang.h * 0.09f;
	segment = m_lang.w / 2 - inset;
	CZM_DrawPill( UI_DIR "pill_lang_on.png", russian ? m_lang.x + inset : m_lang.x + m_lang.w / 2, m_lang.y + inset, segment, m_lang.h - inset * 2 );
	textY = m_lang.y + ( m_lang.h - CZM_FontHeight( langFont )) / 2;
	CZM_DrawText( langFont, m_lang.x + inset, textY, segment, "RU", russian ? CZM_White() : CZM_Grey(), QM_CENTER );
	CZM_DrawText( langFont, m_lang.x + m_lang.w / 2, textY, segment, "EN", russian ? CZM_Grey() : CZM_White(), QM_CENTER );
}

void CCzmMainMenu::DrawTile( int index, CzmRect r )
{
	CzmServer *server = CZM_Server( index );
	HFont statusFont = CZM_Font( CZMF_TILE_STATUS );
	int margin, textX, textW, statusY, dot, x, right;
	unsigned int dotColor;
	char text[128];

	if( !server )
		return;

	// pressed tiles sink a little
	if( IsPressed( HIT_TILE + index ))
	{
		int dx = r.w * 0.02f, dy = r.h * 0.02f;
		r = CZM_Rect( r.x + dx, r.y + dy, r.w - dx * 2, r.h - dy * 2 );
	}

	margin = r.h / 10;
	CZM_DrawImage( UI_DIR "tile_shadow.png", r.x - margin, r.y - margin + r.h * 0.05f, r.w + margin * 2, r.h + margin * 2 );
	CZM_DrawImage( server->tile, r.x, r.y, r.w, r.h );

	// text sits right of the slanted edge and left of the play button
	textX = r.x + r.h * 0.40f;
	textW = r.x + r.w - r.h * 0.58f - textX;
	CZM_DrawText( CZM_Font( CZMF_TILE_TITLE ), textX, r.y + r.h * 0.12f, textW, server->title, CZM_White());

	if( server->map[0] )
	{
		snprintf( text, sizeof( text ), "%s  %s", CZM_T( "lbl_map" ), server->map );
		CZM_DrawText( CZM_Font( CZMF_TILE_TEXT ), textX, r.y + r.h * 0.47f, textW, text, PackRGBA( 215, 220, 228, 255 ));
	}

	statusY = r.y + r.h * 0.69f;
	dot = r.h * 0.085f;
	x = textX + dot + r.h * 0.05f;
	right = textX + textW;

	if( server->answerTime <= 0.0 && server->misses == 0 )
	{
		dotColor = PackRGBA( 150, 156, 166, 255 );
		CZM_DrawText( statusFont, x, statusY, right - x, CZM_T( "querying" ), CZM_Grey());
	}
	else if( server->answerTime <= 0.0 || server->misses >= 2 )
	{
		dotColor = PackRGBA( 248, 113, 113, 255 );
		CZM_DrawText( statusFont, x, statusY, right - x, CZM_T( "offline" ), PackRGBA( 248, 113, 113, 255 ));
	}
	else
	{
		bool full = server->maxplayers > 0 && server->players >= server->maxplayers;

		dotColor = full ? PackRGBA( 250, 204, 21, 255 ) : PackRGBA( 74, 222, 128, 255 );
		snprintf( text, sizeof( text ), "%d/%d %s", server->players, server->maxplayers, CZM_T( "in_game" ));
		CZM_DrawText( statusFont, x, statusY, right - x, text, CZM_White());
		x += CZM_TextWidth( statusFont, text );

		if( server->ping >= 0 && x < right )
		{
			CZM_DrawText( statusFont, x, statusY, right - x, "  ·  ", CZM_Grey());
			x += CZM_TextWidth( statusFont, "  ·  " );
			snprintf( text, sizeof( text ), "%d %s", server->ping, CZM_T( "ms" ));
			CZM_DrawText( statusFont, x, statusY, right - x, text, CZM_PingColor( server->ping ));
		}
	}

	CZM_DrawImage( UI_DIR "dot.png", textX, statusY + ( CZM_FontHeight( statusFont ) - dot ) / 2, dot, dot, dotColor );
}

void CCzmMainMenu::DrawNav( void )
{
	static const char *icons[4] = { UI_DIR "icon_chat.png", UI_DIR "icon_globe.png", UI_DIR "icon_gear.png", UI_DIR "icon_info.png" };
	static const char *labels[4] = { "nav_chat", "nav_servers", "nav_settings", "nav_about" };
	HFont font = CZM_Font( CZMF_NAV );
	int top = m_nav[0].y;
	int iconSize = H * 0.042f;

	UI_FillRect( 0, top, W, H - top, PackRGBA( 8, 10, 14, 190 ));
	UI_FillRect( 0, top, W, CZM_LineWidth(), CZM_White( 40 ));

	for( int i = 0; i < 4; i++ )
	{
		const CzmRect &r = m_nav[i];
		bool pressed = IsPressed( HIT_NAV + i );
		unsigned int color = pressed ? CZM_White() : PackRGBA( 225, 228, 235, 255 );

		if( pressed )
			UI_FillRect( r.x, r.y, r.w, r.h, CZM_White( 20 ));

		CZM_DrawImage( icons[i], r.x + ( r.w - iconSize ) / 2, top + H * 0.014f, iconSize, iconSize, color );
		CZM_DrawText( font, r.x, top + H * 0.072f - CZM_FontHeight( font ) / 2, r.w, CZM_T( labels[i] ), color, QM_CENTER );

		if( i > 0 )
			UI_FillRect( r.x, top + H * 0.025f, CZM_LineWidth(), H - top - H * 0.05f, CZM_White( 30 ));
	}
}

void CCzmMainMenu::DrawAbout( void )
{
	static const char *icons[CZM_ABOUT_ROWS] = { UI_DIR "icon_send.png", UI_DIR "icon_users.png", UI_DIR "icon_globe.png", UI_DIR "icon_code.png",
		UI_DIR "icon_tag.png", UI_DIR "icon_calendar.png" };
	static const char *labels[CZM_ABOUT_ROWS] = { "about_tg", "about_group", "about_site", "about_source", "about_version", "about_updated" };
	HFont labelFont = CZM_Font( CZMF_PANEL_TEXT );
	HFont valueFont = CZM_Font( CZMF_TILE_STATUS );
	const CzmRect &logoRow = m_aboutLogo;
	int logo = logoRow.h;
	int textX = logoRow.x + logo + H * 0.03f;
	char date[32];

	CZM_DrawImage( UI_DIR "avatar.png", logoRow.x, logoRow.y, logo, logo );
	CZM_DrawText( CZM_Font( CZMF_NICK ), textX, logoRow.y + logo * 0.10f, logoRow.x + logoRow.w - textX, "Counter Zombie Mobile", CZM_White());
	CZM_DrawText( CZM_Font( CZMF_SMALL ), textX, logoRow.y + logo * 0.58f, logoRow.x + logoRow.w - textX, "32 players", CZM_Grey());

	BuildDate( date, sizeof( date ));

	for( int i = 0; i < CZM_ABOUT_ROWS; i++ )
	{
		const CzmRect &r = m_aboutRows[i];
		bool link = i <= 1 || i == 3;
		bool pressed = ( i == 0 && IsPressed( HIT_ABOUT_TELEGRAM )) || ( i == 1 && IsPressed( HIT_ABOUT_GROUP )) || ( i == 3 && IsPressed( HIT_ABOUT_SOURCE ));
		int chip = r.h * 0.62f, icon = chip * 0.52f, chevron = r.h * 0.30f;
		int labelX = r.x + chip + H * 0.022f, valueRight = r.x + r.w - ( link ? chevron + H * 0.012f : 0 );
		const char *value;
		unsigned int valueColor = CZM_White();
		int valueW;

		switch( i )
		{
		case 0: value = "Telegram @blackmodru"; valueColor = PackRGBA( 125, 196, 255, 255 ); break;
		case 1: value = "t.me/counter_zombie_mobile"; valueColor = PackRGBA( 125, 196, 255, 255 ); break;
		case 2: value = CZM_T( "about_site_soon" ); valueColor = PackRGBA( 250, 204, 21, 255 ); break;
		case 3: value = "github.com/kurostreet/cs16-client"; valueColor = PackRGBA( 125, 196, 255, 255 ); break;
		case 4: value = CZM_Version(); break;
		default: value = date; break;
		}

		if( pressed )
			CZM_DrawSliced( UI_DIR "card.png", r.x - H * 0.012f, r.y + H * 0.004f, r.w + H * 0.024f, r.h - H * 0.008f, 30, (int)( H * 0.02f ));
		if( i > 0 )
			UI_FillRect( labelX, r.y, r.x + r.w - labelX, CZM_LineWidth(), CZM_White( 24 ));

		CZM_DrawImage( UI_DIR "chip_round.png", r.x, r.y + ( r.h - chip ) / 2, chip, chip );
		CZM_DrawImage( icons[i], r.x + ( chip - icon ) / 2, r.y + ( r.h - icon ) / 2, icon, icon, PackRGBA( 220, 224, 232, 255 ));

		// the value keeps its full width, the label gives way
		valueW = Q_min( CZM_TextWidth( valueFont, value ), ( valueRight - labelX ) * 3 / 5 );
		CZM_DrawText( labelFont, labelX, r.y + ( r.h - CZM_FontHeight( labelFont )) / 2, valueRight - valueW - H * 0.02f - labelX,
			CZM_T( labels[i] ), PackRGBA( 165, 172, 182, 255 ));
		CZM_DrawText( valueFont, valueRight - valueW, r.y + ( r.h - CZM_FontHeight( valueFont )) / 2, valueW + 2, value, valueColor, QM_RIGHT );

		if( link )
			CZM_DrawImage( UI_DIR "icon_chevron.png", r.x + r.w - chevron, r.y + ( r.h - chevron ) / 2, chevron, chevron, PackRGBA( 150, 156, 166, 255 ));
	}

	{
		const CzmRect &last = m_aboutRows[CZM_ABOUT_ROWS - 1];
		HFont small = CZM_Font( CZMF_SMALL );

		CZM_DrawText( small, m_panel.x, last.y + last.h + H * 0.022f, m_panel.w, CZM_T( "about_more" ), PackRGBA( 120, 126, 136, 255 ), QM_CENTER );
	}
}

void CCzmMainMenu::DrawBell( void )
{
	HFont textFont = CZM_Font( CZMF_PANEL_TEXT );
	HFont smallFont = CZM_Font( CZMF_SMALL );
	const unsigned int dim = PackRGBA( 165, 172, 182, 255 );
	const CzmUpdateInfo *update = CZM_Update();
	int pad = H * 0.045f, x = m_panel.x + pad, w = m_panel.w - pad * 2;
	int y = m_iPanelLineY + H * 0.025f, bottom = m_panel.y + m_panel.h - pad * 0.7f;
	int seen = (int)EngFuncs::GetCvarFloat( "czm_news_seen" );

	if( update )
	{
		const CzmRect &c = m_update;
		HFont nameFont = CZM_Font( CZMF_NICK );
		int state;
		double done, total;
		char error[32], text[160], size[32];
		const char *label;
		int starts[4], lines, ty;

		CZM_UpdateState( &state, &done, &total, error, sizeof( error ));
		CZM_DrawSliced( UI_DIR "card_on.png", c.x, c.y, c.w, c.h, 30, (int)( H * 0.022f ));

		snprintf( size, sizeof( size ), "%.0f %s", update->size / 1048576.0, CZM_T( "mb" ));
		snprintf( text, sizeof( text ), "%s %s", CZM_T( "upd_title" ), update->version );
		CZM_DrawText( nameFont, c.x + H * 0.03f, c.y + H * 0.022f, c.w - H * 0.06f - CZM_TextWidth( smallFont, size ) - H * 0.02f, text, CZM_White());
		CZM_DrawText( smallFont, c.x + c.w - H * 0.03f - CZM_TextWidth( smallFont, size ), c.y + H * 0.03f, CZM_TextWidth( smallFont, size ) + 2, size, dim );

		// what is new, up to three lines
		ty = c.y + H * 0.022f + CZM_FontHeight( nameFont ) + H * 0.01f;
		lines = CZM_WrapText( textFont, update->notes, c.w - H * 0.06f, starts, 3 );
		for( int l = 0; l < lines && ty + CZM_FontHeight( textFont ) < m_updateButton.y; l++ )
		{
			char line[512];
			int len = Q_min( starts[l + 1] - starts[l], (int)sizeof( line ) - 1 );

			memcpy( line, update->notes + starts[l], len );
			line[len] = 0;
			CZM_DrawText( textFont, c.x + H * 0.03f, ty, c.w - H * 0.06f, line, PackRGBA( 220, 224, 232, 255 ));
			ty += CZM_FontHeight( textFont ) * 115 / 100;
		}

		// the button follows the download
		switch( state )
		{
		case CZM_UPDATE_DOWNLOADING:
		{
			int barX = c.x + H * 0.03f, barW = m_updateButton.x - H * 0.03f - barX;
			int barY = m_updateButton.y + m_updateButton.h / 2, barH = Q_max( 4, (int)( H * 0.01f ));
			int percent = total > 0 ? (int)( done * 100.0 / total ) : 0;

			UI_FillRect( barX, barY, barW, barH, CZM_White( 40 ));
			UI_FillRect( barX, barY, (int)( barW * bound( 0.0, total > 0 ? done / total : 0.0, 1.0 )), barH, PackRGBA( 239, 68, 68, 255 ));
			snprintf( text, sizeof( text ), CZM_T( "upd_downloading" ), percent );
			CZM_DrawText( smallFont, barX, barY - CZM_FontHeight( smallFont ) - H * 0.008f, barW, text, CZM_White());
			label = CZM_T( "upd_cancel" );
			break;
		}
		case CZM_UPDATE_VERIFYING:
			label = CZM_T( "upd_verifying" );
			break;
		case CZM_UPDATE_READY:
			label = CZM_T( "upd_install" );
			break;
		case CZM_UPDATE_INSTALLING:
			label = CZM_T( "upd_installing" );
			break;
		case CZM_UPDATE_PERMISSION:
			CZM_DrawText( smallFont, c.x + H * 0.03f, m_updateButton.y + ( m_updateButton.h - CZM_FontHeight( smallFont )) / 2,
				m_updateButton.x - c.x - H * 0.05f, CZM_T( "upd_permission" ), PackRGBA( 250, 204, 21, 255 ));
			label = CZM_T( "upd_install" );
			break;
		case CZM_UPDATE_ERROR:
			snprintf( text, sizeof( text ), "upd_error_%s", error[0] ? error : "network" );
			CZM_DrawText( smallFont, c.x + H * 0.03f, m_updateButton.y + ( m_updateButton.h - CZM_FontHeight( smallFont )) / 2,
				m_updateButton.x - c.x - H * 0.05f, CZM_T( text ), PackRGBA( 248, 113, 113, 255 ));
			label = CZM_T( "upd_download" );
			break;
		default:
			label = CZM_T( "upd_download" );
			break;
		}

		CZM_DrawButton( state == CZM_UPDATE_DOWNLOADING ? UI_DIR "button_secondary.png" : UI_DIR "button_primary.png", m_updateButton, label,
			CZM_White(), IsPressed( HIT_UPDATE ));
		y = c.y + c.h + H * 0.03f;
	}

	// news, newest first
	if( !CZM_NewsCount( ))
	{
		const char *text = CZM_NewsLoading() ? CZM_T( "news_loading" ) : update ? "" : CZM_T( "bell_empty" );
		int iconSize = H * 0.10f;
		int middle = ( y + bottom ) / 2;

		if( !update )
			CZM_DrawImage( UI_DIR "icon_bell.png", m_panel.x + ( m_panel.w - iconSize ) / 2, middle - iconSize, iconSize, iconSize, CZM_White( 60 ));
		CZM_DrawText( textFont, x, update ? y : middle + H * 0.025f, w, text, dim, QM_CENTER );
		return;
	}

	for( int i = 0; i < CZM_NewsCount() && y < bottom; i++ )
	{
		const CzmNews *n = CZM_NewsItem( i );
		char date[32], line[1024];
		int starts[7], lines;
		time_t t = n->time;

		strftime( date, sizeof( date ), "%d.%m.%Y", localtime( &t ));
		if( n->id > seen )
			CZM_DrawImage( UI_DIR "dot.png", x, y + ( CZM_FontHeight( smallFont ) - H * 0.014f ) / 2, H * 0.014f, H * 0.014f, PackRGBA( 239, 68, 68, 255 ));
		CZM_DrawText( smallFont, x + ( n->id > seen ? H * 0.025f : 0 ), y, w, date, dim );
		y += CZM_FontHeight( smallFont ) + H * 0.006f;

		lines = CZM_WrapText( textFont, n->text, w, starts, 6 );
		for( int l = 0; l < lines && y + CZM_FontHeight( textFont ) <= bottom; l++ )
		{
			int len = Q_min( starts[l + 1] - starts[l], (int)sizeof( line ) - 1 );

			memcpy( line, n->text + starts[l], len );
			line[len] = 0;
			CZM_DrawText( textFont, x, y, w, line, CZM_White());
			y += CZM_FontHeight( textFont ) * 115 / 100;
		}
		y += H * 0.025f;
	}
}

void CCzmMainMenu::DrawSetup( void )
{
	HFont titleFont = CZM_Font( CZMF_PANEL_TITLE );
	HFont textFont = CZM_Font( CZMF_PANEL_TEXT );
	HFont langFont = CZM_Font( CZMF_LANG );
	bool russian = CZM_IsRussian();
	int inset = m_setupLang.h * 0.09f;
	int segment = m_setupLang.w / 2 - inset;
	int textY;

	UI_FillRect( 0, 0, W, H, PackRGBA( 0, 0, 0, 190 ));
	CZM_DrawPanel( m_panel );

	CZM_DrawText( titleFont, m_panel.x, m_panel.y + m_panel.h * 0.10f, m_panel.w, CZM_T( "setup_title" ), CZM_White(), QM_CENTER );
	CZM_DrawText( textFont, m_panel.x, m_panel.y + m_panel.h * 0.10f + CZM_FontHeight( titleFont ) + H * 0.015f, m_panel.w,
		CZM_T( "setup_text" ), PackRGBA( 165, 172, 182, 255 ), QM_CENTER );

	// RU | EN, the same switch as in the header
	CZM_DrawPill( UI_DIR "chip_lang.png", m_setupLang.x, m_setupLang.y, m_setupLang.w, m_setupLang.h );
	CZM_DrawPill( UI_DIR "pill_lang_on.png", russian ? m_setupLang.x + inset : m_setupLang.x + m_setupLang.w / 2, m_setupLang.y + inset,
		segment, m_setupLang.h - inset * 2 );
	textY = m_setupLang.y + ( m_setupLang.h - CZM_FontHeight( langFont )) / 2;
	CZM_DrawText( langFont, m_setupLang.x + inset, textY, segment, "RU", russian ? CZM_White() : CZM_Grey(), QM_CENTER );
	CZM_DrawText( langFont, m_setupLang.x + m_setupLang.w / 2, textY, segment, "EN", russian ? CZM_Grey() : CZM_White(), QM_CENTER );

	CZM_DrawField( m_setupNick, m_nick, m_nick.text, CZM_T( "nick_placeholder" ), IsPressed( HIT_SETUP_NICK ));
	if( m_bNickWarning )
		CZM_DrawText( CZM_Font( CZMF_SMALL ), m_setupNick.x, m_setupNick.y + m_setupNick.h + H * 0.015f, m_setupNick.w,
			CZM_T( "nick_bad" ), PackRGBA( 248, 113, 113, 255 ), QM_CENTER );

	CZM_DrawButton( UI_DIR "button_primary.png", m_setupGo, CZM_T( "setup_go" ), CZM_White(), IsPressed( HIT_SETUP_GO ));
}

void CCzmMainMenu::DrawOverlay( void )
{
	HFont titleFont = CZM_Font( CZMF_PANEL_TITLE );
	int pad = H * 0.045f;
	int closeIcon;

	if( m_overlay == OV_SETUP )
	{
		DrawSetup();
		return;
	}

	UI_FillRect( 0, 0, W, H, PackRGBA( 0, 0, 0, 170 ));
	CZM_DrawPanel( m_panel );

	if( m_overlay == OV_QUIT )
	{
		int titleY = m_panel.y + ( m_quitNo.y - m_panel.y - CZM_FontHeight( titleFont )) / 2;

		CZM_DrawText( titleFont, m_panel.x + pad, titleY, m_panel.w - pad * 2, CZM_T( "quit_title" ), CZM_White(), QM_CENTER );
		CZM_DrawButton( UI_DIR "button_secondary.png", m_quitNo, CZM_T( "cancel" ), CZM_White(), IsPressed( HIT_QUIT_NO ));
		CZM_DrawButton( UI_DIR "button_primary.png", m_quitYes, CZM_T( "quit_yes" ), CZM_White(), IsPressed( HIT_QUIT_YES ));
		return;
	}

	// title, close button and a thin line under them
	CZM_DrawText( titleFont, m_panel.x + pad, m_panel.y + pad * 0.8f, m_close.x - m_panel.x - pad,
		CZM_T( m_overlay == OV_BELL ? "bell_title" : m_overlay == OV_CHAT ? "chat_title" : "about_title" ), CZM_White());
	closeIcon = m_close.h * 0.42f;
	CZM_DrawImage( UI_DIR "icon_close.png", m_close.x + ( m_close.w - closeIcon ) / 2, m_close.y + ( m_close.h - closeIcon ) / 2,
		closeIcon, closeIcon, IsPressed( HIT_CLOSE ) ? CZM_White() : PackRGBA( 190, 196, 206, 255 ));
	UI_FillRect( m_panel.x + pad, m_iPanelLineY, m_panel.w - pad * 2, CZM_LineWidth(), CZM_White( 30 ));

	if( m_overlay == OV_BELL )
	{
		DrawBell();
		return;
	}

	DrawAbout();
}

void CCzmMainMenu::Draw()
{
	Layout();

	// darker top edge so the header reads on any art
	CZM_DrawImage( UI_DIR "fade_top.png", 0, 0, W, H * 0.22f, CZM_White( 200 ));

	DrawTitle();
	for( int i = 0; i < m_iTiles; i++ )
		DrawTile( i, m_tiles[i] );
	DrawNav();
	DrawHeader();

	if( m_overlay != OV_NONE )
		DrawOverlay();
}

/*
=================
CCzmPauseMenu

Small window over the game: resume, settings, leave to the main screen.
=================
*/
class CCzmPauseMenu : public CMenuFramework
{
public:
	CCzmPauseMenu();

	void Show() override;
	void Draw() override;
	bool KeyDown( int key ) override;
	bool KeyUp( int key ) override;

private:
	enum EHit
	{
		HIT_NONE = 0,
		HIT_RESUME,
		HIT_SETTINGS,
		HIT_LEAVE,
	};

	void Layout( void );
	int HitTest( void ) const;
	bool IsPressed( int hit ) const { return m_iPressed == hit && HitTest() == hit; }
	void Activate( int hit );

	int m_iPressed;
	CzmRect m_panel, m_resume, m_settings, m_leave;
};

CCzmPauseMenu::CCzmPauseMenu() : CMenuFramework( "CzmPause" )
{
	m_iPressed = HIT_NONE;
}

void CCzmPauseMenu::Show()
{
	m_iPressed = HIT_NONE;
	CMenuFramework::Show();
}

void CCzmPauseMenu::Layout( void )
{
	const int W = ScreenWidth, H = ScreenHeight;
	int pw = H * 0.66f, ph = H * 0.60f;
	int bw, bh, y, gap;

	m_panel = CZM_Rect(( W - pw ) / 2, ( H - ph ) / 2, pw, ph );
	bw = pw * 0.78f;
	bh = ph * 0.15f;
	gap = ph * 0.05f;
	y = m_panel.y + ph * 0.34f;

	m_resume = CZM_Rect(( W - bw ) / 2, y, bw, bh );
	m_settings = CZM_Rect( m_resume.x, y + bh + gap, bw, bh );
	m_leave = CZM_Rect( m_resume.x, y + ( bh + gap ) * 2, bw, bh );
}

int CCzmPauseMenu::HitTest( void ) const
{
	if( m_resume.HasCursor( ))
		return HIT_RESUME;
	if( m_settings.HasCursor( ))
		return HIT_SETTINGS;
	if( m_leave.HasCursor( ))
		return HIT_LEAVE;
	return HIT_NONE;
}

bool CCzmPauseMenu::KeyDown( int key )
{
	Layout();

	if( UI::Key::IsLeftMouse( key ))
	{
		m_iPressed = HitTest();
		return true;
	}

	// Back again returns to the game
	if( UI::Key::IsEscape( key ))
	{
		m_iPressed = HIT_NONE;
		UI_CloseMenu();
		return true;
	}

	return CMenuFramework::KeyDown( key );
}

bool CCzmPauseMenu::KeyUp( int key )
{
	if( UI::Key::IsLeftMouse( key ))
	{
		int pressed = m_iPressed;
		int hit;

		Layout();
		hit = HitTest();
		m_iPressed = HIT_NONE;

		if( hit != HIT_NONE && hit == pressed )
			Activate( hit );

		return true;
	}

	return CMenuFramework::KeyUp( key );
}

void CCzmPauseMenu::Draw()
{
	const int H = ScreenHeight;
	HFont titleFont = CZM_Font( CZMF_PANEL_TITLE );
	HFont textFont = CZM_Font( CZMF_PANEL_TEXT );
	const char *mode = EngFuncs::GetCvarString( "czm_connect_name" );
	const char *map = EngFuncs::GetCvarString( "czm_loading_map" );
	char subtitle[160];

	Layout();
	CZM_DrawPanel( m_panel );

	CZM_DrawText( titleFont, m_panel.x, m_panel.y + m_panel.h * 0.13f - CZM_FontHeight( titleFont ) / 2, m_panel.w,
		CZM_T( "pause" ), CZM_White(), QM_CENTER );

	// mode and map
	if( !mode || !mode[0] )
		mode = EngFuncs::GetCvarString( "czm_loading_host" );
	if( !map || !map[0] )
		map = gpGlobals->maptitle;

	if( mode && mode[0] && map && map[0] )
		snprintf( subtitle, sizeof( subtitle ), "%s  ·  %s", mode, map );
	else
		snprintf( subtitle, sizeof( subtitle ), "%s", ( mode && mode[0] ) ? mode : ( map ? map : "" ));

	CZM_DrawText( textFont, m_panel.x + H * 0.03f, m_panel.y + m_panel.h * 0.24f - CZM_FontHeight( textFont ) / 2,
		m_panel.w - H * 0.06f, subtitle, PackRGBA( 165, 172, 182, 255 ), QM_CENTER );

	CZM_DrawButton( UI_DIR "button_primary.png", m_resume, CZM_T( "resume" ), CZM_White(), IsPressed( HIT_RESUME ));
	CZM_DrawButton( UI_DIR "button_secondary.png", m_settings, CZM_T( "settings" ), CZM_White(), IsPressed( HIT_SETTINGS ));
	CZM_DrawButton( UI_DIR "button_danger.png", m_leave, CZM_T( "to_menu" ), PackRGBA( 255, 150, 150, 255 ), IsPressed( HIT_LEAVE ));
}

/*
=================
registration

UI_Main_Menu is what the engine opens: the pause window in the game, the main screen otherwise.
=================
*/
ADD_MENU( menu_czmmain, CCzmMainMenu, UI_CzmMain_Menu );
ADD_MENU( menu_czmpause, CCzmPauseMenu, UI_CzmPause_Menu );
ADD_MENU4( menu_main, NULL, UI_Main_Menu, NULL );

void UI_Main_Menu( void )
{
	if( CL_IsActive( ))
		UI_CzmPause_Menu();
	else
		UI_CzmMain_Menu();
}

void CCzmPauseMenu::Activate( int hit )
{
	CZM_PlayClick();

	switch( hit )
	{
	case HIT_RESUME:
		UI_CloseMenu();
		break;
	case HIT_SETTINGS:
		CZM_OpenSettings( CZM_TAB_LAST );
		break;
	case HIT_LEAVE:
		EngFuncs::ClientCmd( false, "disconnect\n" );
		// the disconnect runs on the next frame, open the main screen directly
		UI_CzmMain_Menu();
		break;
	}
}
