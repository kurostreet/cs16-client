/* TAB avatars use public tags bound to a server-assigned userid. GPL-3.0-or-later. */
#include <stdio.h>
#include <string.h>
#include "extdll_menu.h"
#include "BaseMenu.h"
#include "CzmMenu.h"
#include "CzmJson.h"
#include "../../common/czm_tab.h"

namespace {
struct Pending { int id, generation; } s_bind, s_resolve;
struct Avatar { char prefix[120], ref[32]; int player; double until; } s_slots[33];
char s_context[96], s_tag[33];
int s_generation;
double s_nextBind, s_nextResolve, s_tagUntil;
CzmTab::State s_requested, s_seen;

void Publish( int slot, const char *value )
{
	char name[40]; snprintf( name, sizeof( name ), "czm_tab_avatar_%d", slot );
	if( strcmp( EngFuncs::GetCvarString( name ), value )) EngFuncs::CvarSetString( name, value );
}

void PublishProfile( int slot, const char *value )
{
	char name[40]; snprintf( name, sizeof( name ), "czm_tab_profile_%d", slot );
	if( strcmp( EngFuncs::GetCvarString( name ), value )) EngFuncs::CvarSetString( name, value );
}

void Discard( Pending &request )
{
	int status; CUtlString body;
	if( request.id && CZM_NetPoll( request.id, &status, body )) request.id = 0;
}
}

void CZM_TabReset( void )
{
	s_generation++;
	s_context[0] = s_tag[0] = 0;
	s_nextBind = s_nextResolve = s_tagUntil = 0;
	memset( s_slots, 0, sizeof( s_slots )); memset( &s_seen, 0, sizeof( s_seen ));
	EngFuncs::CvarSetString( "czm_tag", "" );
	for( int slot = 1; slot <= 32; slot++ ) { Publish( slot, "" ); PublishProfile( slot, "" ); }
	// In-flight HTTPS results are still polled, then discarded by generation.
}

void CZM_TabFrame( void )
{
	CzmTab::State state;
	double now = EngFuncs::DoubleTime();
	bool active = CZM_OnlineReady() && CzmTab::Parse( EngFuncs::GetCvarString( "czm_tab_state" ), state );
	char context[96] = "";
	if( active ) snprintf( context, sizeof( context ), "%s %d %d", state.server, state.self, CZM_OnlinePlayer() );
	if( strcmp( context, s_context )) { CZM_TabReset(); Q_strncpy( s_context, context, sizeof( s_context )); }
	if( !active ) { Discard( s_bind ); Discard( s_resolve ); return; }
	if( memcmp( &state, &s_seen, sizeof( state ))) { s_nextResolve = now; s_seen = state; }

	int status; CUtlString body;
	if( s_bind.id && CZM_NetPoll( s_bind.id, &status, body ))
	{
		s_bind.id = 0;
		CzmJson j;
		if( s_bind.generation == s_generation && status == 200 && j.Parse( body.Get() )
			&& CzmTab::Tag( j.String( j.Get( 0, "tag" ))) && j.Int( j.Get( 0, "ttl" )) > 0 )
		{
			Q_strncpy( s_tag, j.String( j.Get( 0, "tag" )), sizeof( s_tag ));
			s_tagUntil = now + Q_min( 180, j.Int( j.Get( 0, "ttl" )));
			s_nextBind = now + Q_min( 60.0, ( s_tagUntil - now ) / 2 );
			EngFuncs::CvarSetString( "czm_tag", s_tag );
		}
	}
	if( s_tag[0] && now >= s_tagUntil ) { s_tag[0] = 0; EngFuncs::CvarSetString( "czm_tag", "" ); }
	if( !s_bind.id && now >= s_nextBind )
	{
		CUtlString request; char userid[24]; snprintf( userid, sizeof( userid ), "%d", state.self );
		request.Append( "{\"server\":" ); CZM_JsonQuote( request, state.server );
		request.Append( ",\"userid\":" ); request.Append( userid );
		request.Append( ",\"tag\":" ); CZM_JsonQuote( request, s_tag ); request.Append( "}" );
		s_bind.id = CZM_NetRequest( "POST", CZM_OnlineUrl( "/api/v1/tab/bind" ), CZM_OnlineKey(), request.Get() );
		s_bind.generation = s_generation; s_nextBind = now + 30;
	}

	if( s_resolve.id && CZM_NetPoll( s_resolve.id, &status, body ))
	{
		s_resolve.id = 0; CzmJson j;
		if( s_resolve.generation == s_generation && status == 200 && j.Parse( body.Get() ))
		{
			int list = j.Get( 0, "players" );
			for( int n = 0; n < Q_min( 32, j.Count( list )); n++ )
			{
				int item = j.At( list, n ), userid = j.Int( j.Get( item, "userid" ));
				const char *tag = j.String( j.Get( item, "tag" ));
				int player = j.Int( j.Get( item, "id" )), ttl = j.Int( j.Get( item, "ttl" ));
				if( !CzmTab::Tag( tag ) || player <= 0 || ttl <= 0 ) continue;
				for( int slot = 1; slot <= 32; slot++ )
				{
					if( s_requested.players[slot].userid != userid || strcmp( s_requested.players[slot].tag, tag )
						|| state.players[slot].userid != userid || strcmp( state.players[slot].tag, tag )) continue;
					Avatar &a = s_slots[slot]; CzmTab::Prefix( a.prefix, sizeof( a.prefix ), state, slot );
					a.player = player; a.until = now + Q_min( 180, ttl );
					Q_strncpy( a.ref, j.String( j.Get( item, "avatar" )), sizeof( a.ref ));
				}
			}
		}
	}
	if( !s_resolve.id && now >= s_nextResolve )
	{
		CUtlString request; int count = 0;
		request.Append( "{\"server\":" ); CZM_JsonQuote( request, state.server ); request.Append( ",\"players\":[" );
		for( int slot = 1; slot <= 32; slot++ )
		{
			const CzmTab::Player &p = state.players[slot];
			if( !p.userid || p.userid == state.self || !p.tag[0] ) continue;
			char item[100]; snprintf( item, sizeof( item ), "%s{\"userid\":%d,\"tag\":\"%s\"}", count++ ? "," : "", p.userid, p.tag );
			request.Append( item );
		}
		request.Append( "]}" ); s_nextResolve = now + 15;
		if( count )
		{
			s_requested = state; s_resolve.generation = s_generation;
			s_resolve.id = CZM_NetRequest( "POST", CZM_OnlineUrl( "/api/v1/tab/resolve" ), CZM_OnlineKey(), request.Get() );
		}
	}
	for( int slot = 1; slot <= 32; slot++ )
	{
		char prefix[120], value[192] = "", profile[160] = ""; CzmTab::Prefix( prefix, sizeof( prefix ), state, slot );
		Avatar &a = s_slots[slot];
		if( a.until > now && !strcmp( prefix, a.prefix ))
		{
			snprintf( profile, sizeof( profile ), "%s%d", prefix, a.player );
			const char *path = CZM_AvatarFor( a.ref, a.player );
			if( CzmTab::AvatarPath( path )) snprintf( value, sizeof( value ), "%s%s", prefix, path );
		}
		else memset( &a, 0, sizeof( a ));
		Publish( slot, value );
		PublishProfile( slot, profile );
	}
}
