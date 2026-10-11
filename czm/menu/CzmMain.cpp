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
#include <string.h>

#include "extdll_menu.h"
#include "BaseMenu.h"
#include "Utils.h"
#include "Framework.h"
#include "keydefs.h"
#include "CzmMenu.h"

#define UI_DIR "gfx/czm/ui/"
#define CZM_MAX_TILES 8

namespace
{

struct CzmRect
{
	int x, y, w, h;

	bool HasCursor( void ) const
	{
		return w > 0 && h > 0 && CZM_CursorIn( x, y, w, h );
	}

	// touch targets are a bit larger than what is drawn
	CzmRect Grown( int by ) const
	{
		CzmRect r = { x - by, y - by, w + by * 2, h + by * 2 };
		return r;
	}
};

CzmRect MakeRect( int x, int y, int w, int h )
{
	CzmRect r = { x, y, w, h };
	return r;
}

unsigned int White( int alpha = 255 )
{
	return PackRGBA( 255, 255, 255, alpha );
}

unsigned int Grey( void )
{
	return PackRGBA( 170, 176, 186, 255 );
}

int LineWidth( void )
{
	return Q_max( 1, ScreenHeight / 600 );
}

void DrawPanel( const CzmRect &r )
{
	CZM_DrawSliced( UI_DIR "panel.png", r.x, r.y, r.w, r.h, 40, (int)( ScreenHeight * 0.026f ));
}

void DrawButton( const char *image, const CzmRect &r, const char *label, unsigned int textColor, bool pressed )
{
	HFont font = CZM_Font( CZMF_BUTTON );

	CZM_DrawPill( image, r.x, r.y, r.w, r.h, pressed ? PackRGBA( 185, 185, 185, 255 ) : White());
	CZM_DrawText( font, r.x, r.y + ( r.h - CZM_FontHeight( font )) / 2, r.w, label, textColor, QM_CENTER );
}

void PlayClick( void )
{
	EngFuncs::PlayLocalSound( uiStatic.sounds[SND_LAUNCH] );
}

// notifications come from the kontrafps.ru service
int UnreadNotifications( void )
{
	return 0;
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

private:
	enum EOverlay
	{
		OV_NONE = 0,
		OV_QUIT,
		OV_BELL,
		OV_ABOUT,
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
}

void CCzmMainMenu::Show()
{
	m_overlay = OV_NONE;
	m_iPressed = HIT_NONE;
	CMenuFramework::Show();
	CZM_QueryServers( true );
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
	m_avatar = MakeRect( m_iMargin, top, avatar, avatar );

	CZM_CleanName( EngFuncs::GetCvarString( "name" ), m_szNick, sizeof( m_szNick ));
	if( !m_szNick[0] )
		Q_strncpy( m_szNick, "Player", sizeof( m_szNick ));

	m_iNickX = m_iMargin + avatar + H * 0.022f;
	m_iNickW = Q_min( CZM_TextWidth( nickFont, m_szNick ), (int)( W * 0.34f ));
	pencil = H * 0.028f;
	right = Q_max( m_iNickX + m_iNickW + (int)( H * 0.012f ) + pencil, m_iNickX + CZM_TextWidth( hintFont, CZM_T( "nick_hint" )));
	m_profile = MakeRect( m_iMargin, top, right - m_iMargin, avatar );

	// header right: bell and language switch
	chip = H * 0.066f;
	langW = H * 0.22f;
	m_lang = MakeRect( W - m_iMargin - langW, top, langW, chip );
	m_bell = MakeRect( m_lang.x - H * 0.025f - chip, top, chip, chip );

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
		m_tiles[i] = MakeRect( W - m_iMargin - tileW, y + i * ( tileH + gap ), tileW, tileH );

	// bottom navigation
	y = H * 0.885f;
	for( int i = 0; i < 4; i++ )
		m_nav[i] = MakeRect( W * i / 4, y, W * ( i + 1 ) / 4 - W * i / 4, H - y );

	// panels
	if( m_overlay == OV_QUIT )
	{
		int pad = H * 0.04f;
		int bh = H * 0.085f;
		int pw = H * 0.70f, ph = H * 0.36f;
		int bw;

		m_panel = MakeRect(( W - pw ) / 2, ( H - ph ) / 2, pw, ph );
		bw = ( pw - pad * 3 ) / 2;
		m_quitNo = MakeRect( m_panel.x + pad, m_panel.y + ph - pad - bh, bw, bh );
		m_quitYes = MakeRect( m_panel.x + pad * 2 + bw, m_quitNo.y, bw, bh );
	}
	else
	{
		int pw = Q_min( (int)( H * 1.0f ), (int)( W * 0.62f ));
		int ph = H * 0.68f;
		int pad = H * 0.045f;
		int closeSize = H * 0.09f;
		int lineStep, aboutTop;

		m_panel = MakeRect(( W - pw ) / 2, ( H - ph ) / 2, pw, ph );
		m_close = MakeRect( m_panel.x + pw - closeSize - H * 0.015f, m_panel.y + H * 0.015f, closeSize, closeSize );
		m_iPanelLineY = m_panel.y + pad * 0.8f + CZM_FontHeight( CZM_Font( CZMF_PANEL_TITLE )) + H * 0.02f;

		// About: logo row, then the version line (five taps open the console)
		aboutTop = m_iPanelLineY + H * 0.035f;
		lineStep = CZM_FontHeight( CZM_Font( CZMF_PANEL_TEXT )) * 17 / 10;
		m_version = MakeRect( m_panel.x + pad, aboutTop + H * 0.11f + H * 0.035f, pw - pad * 2, lineStep );
	}
}

int CCzmMainMenu::HitTest( void ) const
{
	const int grow = ScreenHeight * 0.015f;

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
		PlayClick();

	switch( hit )
	{
	case HIT_PROFILE:
	case HIT_NAV + 0:
		UI_PlayerSetup_Menu();
		break;
	case HIT_NAV + 1:
		UI_Touch_Menu();
		break;
	case HIT_NAV + 2:
		UI_Options_Menu();
		break;
	case HIT_NAV + 3:
		m_overlay = OV_ABOUT;
		m_iVersionTaps = 0;
		break;
	case HIT_BELL:
		m_overlay = OV_BELL;
		break;
	case HIT_LANG_RU:
		CZM_SetLanguage( true );
		break;
	case HIT_LANG_EN:
		CZM_SetLanguage( false );
		break;
	case HIT_BACKDROP:
	case HIT_CLOSE:
	case HIT_QUIT_NO:
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

bool CCzmMainMenu::KeyDown( int key )
{
	Layout();

	if( UI::Key::IsLeftMouse( key ))
	{
		m_iPressed = HitTest();
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
	CZM_DrawImage( UI_DIR "avatar.png", m_avatar.x, m_avatar.y, m_avatar.w, m_avatar.h, profilePressed ? PackRGBA( 190, 190, 190, 255 ) : White());
	nickY = m_avatar.y + m_avatar.h * 0.04f;
	CZM_DrawText( nickFont, m_iNickX, nickY, m_iNickW, m_szNick, profilePressed ? Grey() : White());
	pencil = H * 0.028f;
	CZM_DrawImage( UI_DIR "icon_pencil.png", m_iNickX + m_iNickW + H * 0.012f, nickY + ( CZM_FontHeight( nickFont ) - pencil ) / 2,
		pencil, pencil, PackRGBA( 200, 205, 215, 255 ));
	CZM_DrawText( hintFont, m_iNickX, m_avatar.y + m_avatar.h * 0.62f, W * 0.4f, CZM_T( "nick_hint" ), Grey());

	// bell with the unread counter
	CZM_DrawImage( UI_DIR "chip_round.png", m_bell.x, m_bell.y, m_bell.w, m_bell.h, IsPressed( HIT_BELL ) ? PackRGBA( 170, 170, 170, 255 ) : White());
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
		CZM_DrawText( badgeFont, bx, by + ( d - CZM_FontHeight( badgeFont )) / 2, d, count, White(), QM_CENTER );
	}

	// RU | EN
	CZM_DrawPill( UI_DIR "chip_lang.png", m_lang.x, m_lang.y, m_lang.w, m_lang.h );
	inset = m_lang.h * 0.09f;
	segment = m_lang.w / 2 - inset;
	CZM_DrawPill( UI_DIR "pill_lang_on.png", russian ? m_lang.x + inset : m_lang.x + m_lang.w / 2, m_lang.y + inset, segment, m_lang.h - inset * 2 );
	textY = m_lang.y + ( m_lang.h - CZM_FontHeight( langFont )) / 2;
	CZM_DrawText( langFont, m_lang.x + inset, textY, segment, "RU", russian ? White() : Grey(), QM_CENTER );
	CZM_DrawText( langFont, m_lang.x + m_lang.w / 2, textY, segment, "EN", russian ? Grey() : White(), QM_CENTER );
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
		r = MakeRect( r.x + dx, r.y + dy, r.w - dx * 2, r.h - dy * 2 );
	}

	margin = r.h / 10;
	CZM_DrawImage( UI_DIR "tile_shadow.png", r.x - margin, r.y - margin + r.h * 0.05f, r.w + margin * 2, r.h + margin * 2 );
	CZM_DrawImage( server->tile, r.x, r.y, r.w, r.h );

	// text sits right of the slanted edge and left of the play button
	textX = r.x + r.h * 0.40f;
	textW = r.x + r.w - r.h * 0.58f - textX;
	CZM_DrawText( CZM_Font( CZMF_TILE_TITLE ), textX, r.y + r.h * 0.12f, textW, server->title, White());

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
		CZM_DrawText( statusFont, x, statusY, right - x, CZM_T( "querying" ), Grey());
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
		CZM_DrawText( statusFont, x, statusY, right - x, text, White());
		x += CZM_TextWidth( statusFont, text );

		if( server->ping >= 0 && x < right )
		{
			CZM_DrawText( statusFont, x, statusY, right - x, "  ·  ", Grey());
			x += CZM_TextWidth( statusFont, "  ·  " );
			snprintf( text, sizeof( text ), "%d %s", server->ping, CZM_T( "ms" ));
			CZM_DrawText( statusFont, x, statusY, right - x, text, CZM_PingColor( server->ping ));
		}
	}

	CZM_DrawImage( UI_DIR "dot.png", textX, statusY + ( CZM_FontHeight( statusFont ) - dot ) / 2, dot, dot, dotColor );
}

void CCzmMainMenu::DrawNav( void )
{
	static const char *icons[4] = { UI_DIR "icon_user.png", UI_DIR "icon_stick.png", UI_DIR "icon_gear.png", UI_DIR "icon_info.png" };
	static const char *labels[4] = { "nav_profile", "nav_controls", "nav_settings", "nav_about" };
	HFont font = CZM_Font( CZMF_NAV );
	int top = m_nav[0].y;
	int iconSize = H * 0.042f;

	UI_FillRect( 0, top, W, H - top, PackRGBA( 8, 10, 14, 190 ));
	UI_FillRect( 0, top, W, LineWidth(), White( 40 ));

	for( int i = 0; i < 4; i++ )
	{
		const CzmRect &r = m_nav[i];
		bool pressed = IsPressed( HIT_NAV + i );
		unsigned int color = pressed ? White() : PackRGBA( 225, 228, 235, 255 );

		if( pressed )
			UI_FillRect( r.x, r.y, r.w, r.h, White( 20 ));

		CZM_DrawImage( icons[i], r.x + ( r.w - iconSize ) / 2, top + H * 0.014f, iconSize, iconSize, color );
		CZM_DrawText( font, r.x, top + H * 0.072f - CZM_FontHeight( font ) / 2, r.w, CZM_T( labels[i] ), color, QM_CENTER );

		if( i > 0 )
			UI_FillRect( r.x, top + H * 0.025f, LineWidth(), H - top - H * 0.05f, White( 30 ));
	}
}

void CCzmMainMenu::DrawAbout( void )
{
	HFont textFont = CZM_Font( CZMF_PANEL_TEXT );
	const unsigned int textColor = PackRGBA( 200, 205, 215, 255 );
	int pad = H * 0.045f;
	int x = m_panel.x + pad, w = m_panel.w - pad * 2;
	int top = m_iPanelLineY + H * 0.035f;
	int logo = H * 0.11f;
	int textX = x + logo + H * 0.03f;
	int y, step;
	char text[128];

	CZM_DrawImage( UI_DIR "avatar.png", x, top, logo, logo );
	CZM_DrawText( CZM_Font( CZMF_NICK ), textX, top + logo * 0.14f, x + w - textX, "Counter Zombie Mobile", White());
	CZM_DrawText( CZM_Font( CZMF_SMALL ), textX, top + logo * 0.58f, x + w - textX, "32 players", Grey());

	// version: label and value
	y = m_version.y + ( m_version.h - CZM_FontHeight( textFont )) / 2;
	snprintf( text, sizeof( text ), "%s  ", CZM_T( "about_version" ));
	CZM_DrawText( textFont, x, y, w, text, Grey());
	CZM_DrawText( textFont, x + CZM_TextWidth( textFont, text ), y, w - CZM_TextWidth( textFont, text ), CZM_Version(), White());

	step = m_version.h;
	y += step;
	CZM_DrawText( textFont, x, y, w, CZM_T( "about_engine" ), textColor );
	y += step;
	CZM_DrawText( textFont, x, y, w, CZM_T( "about_source" ), textColor );
	y += step;
	CZM_DrawText( textFont, x, y, w, CZM_T( "about_fonts" ), textColor );
}

void CCzmMainMenu::DrawOverlay( void )
{
	HFont titleFont = CZM_Font( CZMF_PANEL_TITLE );
	int pad = H * 0.045f;
	int closeIcon;

	UI_FillRect( 0, 0, W, H, PackRGBA( 0, 0, 0, 170 ));
	DrawPanel( m_panel );

	if( m_overlay == OV_QUIT )
	{
		int titleY = m_panel.y + ( m_quitNo.y - m_panel.y - CZM_FontHeight( titleFont )) / 2;

		CZM_DrawText( titleFont, m_panel.x + pad, titleY, m_panel.w - pad * 2, CZM_T( "quit_title" ), White(), QM_CENTER );
		DrawButton( UI_DIR "button_secondary.png", m_quitNo, CZM_T( "cancel" ), White(), IsPressed( HIT_QUIT_NO ));
		DrawButton( UI_DIR "button_primary.png", m_quitYes, CZM_T( "quit_yes" ), White(), IsPressed( HIT_QUIT_YES ));
		return;
	}

	// title, close button and a thin line under them
	CZM_DrawText( titleFont, m_panel.x + pad, m_panel.y + pad * 0.8f, m_close.x - m_panel.x - pad,
		CZM_T( m_overlay == OV_BELL ? "bell_title" : "about_title" ), White());
	closeIcon = m_close.h * 0.42f;
	CZM_DrawImage( UI_DIR "icon_close.png", m_close.x + ( m_close.w - closeIcon ) / 2, m_close.y + ( m_close.h - closeIcon ) / 2,
		closeIcon, closeIcon, IsPressed( HIT_CLOSE ) ? White() : PackRGBA( 190, 196, 206, 255 ));
	UI_FillRect( m_panel.x + pad, m_iPanelLineY, m_panel.w - pad * 2, LineWidth(), White( 30 ));

	if( m_overlay == OV_BELL )
	{
		HFont textFont = CZM_Font( CZMF_PANEL_TEXT );
		int iconSize = H * 0.10f;
		int middle = ( m_iPanelLineY + m_panel.y + m_panel.h ) / 2;

		CZM_DrawImage( UI_DIR "icon_bell.png", m_panel.x + ( m_panel.w - iconSize ) / 2, middle - iconSize, iconSize, iconSize, White( 60 ));
		CZM_DrawText( textFont, m_panel.x + pad, middle + H * 0.025f, m_panel.w - pad * 2, CZM_T( "bell_empty" ),
			PackRGBA( 165, 172, 182, 255 ), QM_CENTER );
		return;
	}

	DrawAbout();
}

void CCzmMainMenu::Draw()
{
	Layout();

	// darker top edge so the header reads on any art
	CZM_DrawImage( UI_DIR "fade_top.png", 0, 0, W, H * 0.22f, White( 200 ));

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

	m_panel = MakeRect(( W - pw ) / 2, ( H - ph ) / 2, pw, ph );
	bw = pw * 0.78f;
	bh = ph * 0.15f;
	gap = ph * 0.05f;
	y = m_panel.y + ph * 0.34f;

	m_resume = MakeRect(( W - bw ) / 2, y, bw, bh );
	m_settings = MakeRect( m_resume.x, y + bh + gap, bw, bh );
	m_leave = MakeRect( m_resume.x, y + ( bh + gap ) * 2, bw, bh );
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
	DrawPanel( m_panel );

	CZM_DrawText( titleFont, m_panel.x, m_panel.y + m_panel.h * 0.13f - CZM_FontHeight( titleFont ) / 2, m_panel.w,
		CZM_T( "pause" ), White(), QM_CENTER );

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

	DrawButton( UI_DIR "button_primary.png", m_resume, CZM_T( "resume" ), White(), IsPressed( HIT_RESUME ));
	DrawButton( UI_DIR "button_secondary.png", m_settings, CZM_T( "settings" ), White(), IsPressed( HIT_SETTINGS ));
	DrawButton( UI_DIR "button_danger.png", m_leave, CZM_T( "to_menu" ), PackRGBA( 255, 150, 150, 255 ), IsPressed( HIT_LEAVE ));
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
	PlayClick();

	switch( hit )
	{
	case HIT_RESUME:
		UI_CloseMenu();
		break;
	case HIT_SETTINGS:
		UI_Options_Menu();
		break;
	case HIT_LEAVE:
		EngFuncs::ClientCmd( false, "disconnect\n" );
		// the disconnect runs on the next frame, open the main screen directly
		UI_CzmMain_Menu();
		break;
	}
}
