/* Asynchronous menu avatar cache, drawn by the game HUD. GPL-3.0-or-later. */
#include "hud.h"
#include "cl_util.h"
#include "draw_util.h"
#include "triangleapi.h"
#include "czm_avatars.h"
#include "czm_tab.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

namespace {
struct Texture { char path[64]; int handle; unsigned used; double retry; } s_textures[64];
unsigned s_used;

const char *AvatarPath( int slot, bool self )
{
	if( self )
	{
		const char *avatar = gEngfuncs.pfnGetCvarString( "czm_avatar" );
		if( !std::strcmp( avatar, "photo" ))
		{
			const char *photo = gEngfuncs.pfnGetCvarString( "czm_avatar_photo" );
			if( CzmTab::AvatarPath( photo )) return photo;
		}
		int index = std::atoi( avatar );
		if( index >= 1 && index <= 10 )
		{
			static char path[64]; std::snprintf( path, sizeof( path ), "gfx/czm/avatars/avatar_%02d.png", index );
			return path;
		}
		return "gfx/czm/ui/avatar.png";
	}
	CzmTab::State state;
	if( slot >= 1 && slot <= 32 && CzmTab::Parse( gEngfuncs.pfnGetCvarString( "czm_tab_state" ), state )
		&& state.players[slot].userid > 0 && state.players[slot].tag[0] )
	{
		char prefix[120], name[40]; CzmTab::Prefix( prefix, sizeof( prefix ), state, slot );
		std::snprintf( name, sizeof( name ), "czm_tab_avatar_%d", slot );
		const char *value = gEngfuncs.pfnGetCvarString( name );
		if( !std::strncmp( value, prefix, std::strlen( prefix )) && CzmTab::AvatarPath( value + std::strlen( prefix )))
			return value + std::strlen( prefix );
	}
	return "gfx/czm/ui/icon_user.png";
}

int LoadAvatar( const char *path )
{
	Texture *entry = &s_textures[0];
	for( Texture &candidate : s_textures )
	{
		if( !std::strcmp( candidate.path, path )) { entry = &candidate; break; }
		if( candidate.used < entry->used ) entry = &candidate;
	}
	if( std::strcmp( entry->path, path ))
	{
		if( entry->handle ) gRenderAPI.GL_FreeTexture( entry->handle );
		std::memset( entry, 0, sizeof( *entry ));
		std::snprintf( entry->path, sizeof( entry->path ), "%s", path );
	}
	entry->used = ++s_used;
	double now = gEngfuncs.GetClientTime();
	if( entry->handle || now < entry->retry ) return entry->handle;
	entry->retry = now + 10;
	int length = 0; byte *data = gEngfuncs.COM_LoadFile( path, 5, &length );
	if( data )
	{
		// Separate texture names prevent a TAB eviction from freeing a menu image.
		char name[96]; std::snprintf( name, sizeof( name ), "czm_tab/%s", path );
		if( length > 0 && length <= 1024 * 1024 ) entry->handle = gRenderAPI.GL_LoadTexture( name, data, length, TF_NOMIPMAP|TF_CLAMP );
		gEngfuncs.COM_FreeFile( data );
	}
	return entry->handle;
}
}

namespace {
struct ProfileHit { int x, y, width, height; bool self; } s_profileHits[33];
int s_profileFinger = -1, s_profileSlot, s_profileId;
float s_profileDownX, s_profileDownY;
double s_profileDrawTime;
char s_profilePrefix[120];

int ProfileId( int slot, bool self, char *prefix )
{
	prefix[0] = 0;
	if( self ) return std::atoi( gEngfuncs.pfnGetCvarString( "czm_profile_id" ));
	CzmTab::State state;
	if( !CzmTab::Parse( gEngfuncs.pfnGetCvarString( "czm_tab_state" ), state )) return 0;
	char name[40]; std::snprintf( name, sizeof( name ), "czm_tab_profile_%d", slot );
	CzmTab::Prefix( prefix, 120, state, slot );
	return CzmTab::ProfileId( gEngfuncs.pfnGetCvarString( name ), state, slot );
}
}

void CZM_TabProfilesBeginDraw()
{
	std::memset( s_profileHits, 0, sizeof( s_profileHits ));
	s_profileDrawTime = gEngfuncs.GetClientTime();
}

void CZM_TabProfileRow( int slot, bool self, int x, int y, int width, int height )
{
	if( slot >= 1 && slot <= 32 ) s_profileHits[slot] = { x, y, width, height, self };
}

void CZM_TabConnectionFrame()
{
	const char *level = gEngfuncs.pfnGetLevelName();
	const char *base = level ? std::strrchr( level, '/' ) : NULL;
	base = base ? base + 1 : level;
	char map[65] = "";
	if( base && std::strlen( base ) < sizeof( map ))
	{
		std::strcpy( map, base );
		char *suffix = std::strstr( map, ".bsp" ); if( suffix ) *suffix = 0;
		if( std::strspn( map, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_+.-" ) != std::strlen( map )) map[0] = 0;
	}
	if( std::strcmp( gEngfuncs.pfnGetCvarString( "czm_current_map" ), map )) gEngfuncs.Cvar_Set( "czm_current_map", map );
}

int CZM_TabProfileTouch( int type, int finger, float x, float y )
{
	float aspect = gEngfuncs.pfnGetCvarFloat( "czm_touch_profile_aspect" );
	if( aspect < .25f ) aspect = (float)ScreenHeight / ScreenWidth;
	float px = x * ScreenWidth, py = y * ScreenWidth * aspect;
	if( s_profileFinger == finger )
	{
		if( type == 1 )
		{
			s_profileFinger = -1;
			char prefix[120];
			int player = ProfileId( s_profileSlot, s_profileHits[s_profileSlot].self, prefix );
			if( player == s_profileId && !std::strcmp( prefix, s_profilePrefix ) && std::abs( px - s_profileDownX ) < ScreenHeight * .025f
				&& std::abs( py - s_profileDownY ) < ScreenHeight * .025f && gHUD.m_Scoreboard.ShouldDrawScoreboard() )
			{
				char id[24]; std::snprintf( id, sizeof( id ), "%d", player );
				gEngfuncs.Cvar_Set( "czm_open_profile", id );
				gHUD.m_Scoreboard.m_bForceDraw = gHUD.m_Scoreboard.m_bShowscoresHeld = false;
				gEngfuncs.pfnClientCmd( "menu_czmsocial\n" );
			}
		}
		return 1;
	}
	if( s_profileFinger >= 0 ) return 0;
	if( type != 0 || !gHUD.m_Scoreboard.ShouldDrawScoreboard() || gEngfuncs.GetClientTime() - s_profileDrawTime > .5 ) return 0;
	for( int slot = 1; slot <= 32; slot++ )
	{
		const ProfileHit &h = s_profileHits[slot];
		if( h.width <= 0 || px < h.x || px >= h.x+h.width || py < h.y || py >= h.y+h.height ) continue;
		char prefix[120]; int player = ProfileId( slot, h.self, prefix );
		if( player <= 0 ) return 0;
		s_profileFinger = finger; s_profileSlot = slot; s_profileId = player;
		s_profileDownX = px; s_profileDownY = py; std::strcpy( s_profilePrefix, prefix );
		return 1;
	}
	return 0;
}


void CZM_TabAvatarsVidInit()
{
	std::memset( s_profileHits, 0, sizeof( s_profileHits )); s_profileFinger = -1;
	if( gRenderAPI.GL_FreeTexture )
		for( Texture &entry : s_textures ) if( entry.handle ) gRenderAPI.GL_FreeTexture( entry.handle );
	std::memset( s_textures, 0, sizeof( s_textures )); s_used = 0;
}

bool CZM_DrawTabAvatar( int slot, bool self, int x, int y, int size )
{
	if( size <= 0 || !g_iXash || !gRenderAPI.GL_LoadTexture || !gRenderAPI.GL_FreeTexture ) return false;
	int texture = LoadAvatar( AvatarPath( slot, self ));
	if( !texture ) texture = LoadAvatar( self ? "gfx/czm/ui/avatar.png" : "gfx/czm/ui/icon_user.png" );
	if( !texture ) return false;
	gRenderAPI.GL_SelectTexture( 0 ); gRenderAPI.GL_Bind( 0, texture );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
	gEngfuncs.pTriAPI->Color4f( 1, 1, 1, 1 );
	float scale = gHUD.m_flScale;
	DrawUtils::Draw2DQuad( x * scale, y * scale, ( x + size ) * scale, ( y + size ) * scale );
	return true;
}
