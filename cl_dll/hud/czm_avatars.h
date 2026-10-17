#pragma once
void CZM_TabAvatarsVidInit();
bool CZM_DrawTabAvatar( int slot, bool self, int x, int y, int size );
void CZM_TabProfilesBeginDraw();
void CZM_TabProfileRow( int slot, bool self, int x, int y, int width, int height );
void CZM_TabConnectionFrame();
int CZM_TabProfileTouch( int type, int finger, float x, float y );
