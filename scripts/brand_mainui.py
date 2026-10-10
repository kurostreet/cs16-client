#!/usr/bin/env python3
"""Prepare branded menu sources without modifying the pinned upstream submodule."""

import argparse
from pathlib import Path


def replace_once(source, old, new, filename):
    if source.count(old) != 1:
        raise RuntimeError(f"Upstream menu changed: expected one branding anchor in {filename}")
    return source.replace(old, new, 1)


def brand_background(source):
    filename = "controls/BackgroundBitmap.cpp"
    source = replace_once(
        source,
        "bool CMenuBackgroundBitmap::s_bEnableLogoMovie = false;",
        "static HIMAGE s_CzmLogo = 0;\n\nbool CMenuBackgroundBitmap::s_bEnableLogoMovie = false;",
        filename,
    )
    source = replace_once(
        source,
        "void CMenuBackgroundBitmap::LoadBackground()\n{",
        'void CMenuBackgroundBitmap::LoadBackground()\n{\n\ts_CzmLogo = EngFuncs::PIC_Load( "gfx/vgui/czm_logo.tga", PIC_NOFLIP_TGA );',
        filename,
    )
    source = replace_once(
        source,
        "\tif( FBitSet( ui_prefer_won_background->flags, FCVAR_CHANGED ))",
        '''\t// Keep the supplied square logo intact, leaving room for menu controls.
\tif( s_CzmLogo )
\t{
\t\tUI_FillRect( m_scPos, m_scSize, 0xFF000000 );
\t\tconst int logoSize = (int)Q_min( ScreenHeight * 0.92f, ScreenWidth * 0.58f );
\t\tconst int logoX = (int)( ScreenWidth - logoSize - ScreenWidth * 0.04f );
\t\tconst int logoY = (int)( ( ScreenHeight - logoSize ) * 0.5f );
\t\tEngFuncs::PIC_Set( s_CzmLogo, 255, 255, 255, 255 );
\t\tEngFuncs::PIC_Draw( logoX, logoY, logoSize, logoSize );

\t\tchar version[128];
\t\tint stringLen, charH;
\t\tsnprintf( version, sizeof( version ), "Counter Zombie Mobile 32 players build %s", EngFuncs::GetCvarString( "cscl_ver" ) );
\t\tEngFuncs::DrawSetTextColor( 160, 170, 180 );
\t\tEngFuncs::engfuncs.pfnDrawConsoleStringLen( version, &stringLen, &charH );
\t\tEngFuncs::engfuncs.pfnDrawConsoleString( 12, ScreenHeight - charH * 1.5f, version );
\t\treturn;
\t}

\tif( FBitSet( ui_prefer_won_background->flags, FCVAR_CHANGED ))''',
        filename,
    )
    source = replace_once(source, "char version[32];", "char version[128];", filename)
    source = replace_once(
        source,
        'snprintf( version, 32, "cs16-client build %s",',
        'snprintf( version, sizeof( version ), "Counter Zombie Mobile 32 players build %s",',
        filename,
    )
    return source.replace("// print CS16Client version", "// print application version")


def brand_base_menu(source):
    return replace_once(
        source,
        "uiStatic.renderPicbuttonText = gMenu.m_gameinfo.flags & GFL_RENDER_PICBUTTON_TEXT;",
        "uiStatic.renderPicbuttonText = true; // Avoid branded text baked into external cache bitmaps.",
        "BaseMenu.cpp",
    )


def brand_main_menu(source):
    filename = "menus/Main.cpp"
    source = replace_once(
        source,
        'multiPlayer.SetNameAndStatus( L( "GameUI_Multiplayer" ), L( "StringsList_198" ) );',
        'multiPlayer.SetNameAndStatus( "Play online", L( "StringsList_198" ) );',
        filename,
    )
    source = replace_once(
        source,
        'previews.SetNameAndStatus( L( "Previews" ), L( "StringsList_400" ) );',
        'previews.SetNameAndStatus( "Project", L( "StringsList_400" ) );',
        filename,
    )
    old_banner = '''\tif( FBitSet( gMenu.m_gameinfo.flags, GFL_ANIMATED_TITLE ))
\t{
\t\tif( animatedBanner.TryLoad())
\t\t\tAddItem( animatedBanner );
\t}
\telse
\t{
\t\tAddItem( movieBanner );
\t}'''
    source = replace_once(source, old_banner, "\t// The background draws our logo instead of the external cache title movie.", filename)
    source = replace_once(source, "\tAddItem( readme );", "\t// Upstream readme button has no action; project information is available below.", filename)
    # The upstream menu draws all help text over the logo when QMF_NOTIFY is set.
    return source.replace(".iFlags |= QMF_NOTIFY;", ".iFlags &= ~QMF_NOTIFY;")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("upstream", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    transforms = {
        "BaseMenu.cpp": brand_base_menu,
        "controls/BackgroundBitmap.cpp": brand_background,
        "menus/Main.cpp": brand_main_menu,
    }
    for filename, transform in transforms.items():
        content = transform((args.upstream / filename).read_text())
        target = args.output / filename
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists() or target.read_text() != content:
            target.write_text(content)
    print("Prepared Counter Zombie Mobile menu sources")


if __name__ == "__main__":
    main()
