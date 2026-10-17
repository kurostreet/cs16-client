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

void CZM_TabAvatarsVidInit()
{
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
