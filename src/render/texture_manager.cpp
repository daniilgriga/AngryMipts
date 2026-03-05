#include "render/texture_manager.hpp"

#include <filesystem>
#include <functional>
#include <string>

namespace angry
{
namespace
{

constexpr unsigned kTexSize = 256;

std::string resolveProjectPath( const std::filesystem::path& relativePath )
{
    if ( std::filesystem::exists( relativePath ) )
    {
        return relativePath.string();
    }

#ifdef ANGRY_MIPTS_SOURCE_DIR
    const std::filesystem::path fromSourceDir =
        std::filesystem::path( ANGRY_MIPTS_SOURCE_DIR ) / relativePath;
    if ( std::filesystem::exists( fromSourceDir ) )
    {
        return fromSourceDir.string();
    }
#endif

    return relativePath.string();
}

sf::Texture render_texture ( const std::string& name,
                             const std::function<void ( sf::RenderTexture& )>& draw_fn )
{
    sf::RenderTexture canvas ( {kTexSize, kTexSize} );
    canvas.clear ( sf::Color::Transparent );
    draw_fn ( canvas );
    canvas.display();

    const sf::Image image = canvas.getTexture().copyToImage();
    const std::filesystem::path outputDir =
        std::filesystem::path ( resolveProjectPath ( "assets/textures/generated" ) );
    std::error_code ec;
    std::filesystem::create_directories ( outputDir, ec );
    ( void ) image.saveToFile ( outputDir / ( name + ".png" ) );

    sf::Texture texture ( image );
    texture.setSmooth ( true );
    return texture;
}

void draw_base_rect ( sf::RenderTexture& canvas, sf::Color color )
{
    sf::RectangleShape rect ( {static_cast<float> ( kTexSize ), static_cast<float> ( kTexSize )} );
    rect.setFillColor ( color );
    canvas.draw ( rect );
}

}  // namespace

void TextureManager::generate_all()
{
    if ( generated_ )
        return;

    textures_.clear();

    textures_["block_wood"] = render_texture (
        "block_wood",
        [] ( sf::RenderTexture& canvas )
        {
            draw_base_rect ( canvas, sf::Color ( 156, 103, 56 ) );

            for ( int i = 0; i < 11; ++i )
            {
                sf::RectangleShape grain ( {static_cast<float> ( kTexSize ), 4.f} );
                grain.setPosition ( {0.f, 14.f + i * 23.f} );
                grain.setFillColor ( i % 2 == 0 ? sf::Color ( 182, 122, 68, 180 )
                                                : sf::Color ( 124, 82, 44, 170 ) );
                canvas.draw ( grain );
            }

            for ( int i = 0; i < 5; ++i )
            {
                sf::CircleShape knot ( 18.f + i * 1.5f );
                knot.setOrigin ( {knot.getRadius(), knot.getRadius()} );
                knot.setPosition ( {44.f + i * 48.f, 70.f + ( i % 2 ) * 94.f} );
                knot.setFillColor ( sf::Color ( 116, 74, 40, 160 ) );
                canvas.draw ( knot );
            }
        } );

    textures_["block_stone"] = render_texture (
        "block_stone",
        [] ( sf::RenderTexture& canvas )
        {
            draw_base_rect ( canvas, sf::Color ( 146, 151, 158 ) );

            for ( int y = 0; y <= static_cast<int> ( kTexSize ); y += 42 )
            {
                sf::RectangleShape mortar ( {static_cast<float> ( kTexSize ), 3.f} );
                mortar.setPosition ( {0.f, static_cast<float> ( y )} );
                mortar.setFillColor ( sf::Color ( 96, 101, 108, 200 ) );
                canvas.draw ( mortar );
            }

            for ( int x = 0; x <= static_cast<int> ( kTexSize ); x += 48 )
            {
                sf::RectangleShape crack ( {2.f, static_cast<float> ( kTexSize )} );
                crack.setPosition ( {static_cast<float> ( x + ( ( x / 48 ) % 2 ) * 18 ), 0.f} );
                crack.setFillColor ( sf::Color ( 108, 113, 120, 170 ) );
                canvas.draw ( crack );
            }
        } );

    textures_["block_glass"] = render_texture (
        "block_glass",
        [] ( sf::RenderTexture& canvas )
        {
            draw_base_rect ( canvas, sf::Color ( 170, 220, 245, 135 ) );

            sf::RectangleShape shine ( {220.f, 34.f} );
            shine.setPosition ( {18.f, 26.f} );
            shine.setRotation ( sf::degrees ( -12.f ) );
            shine.setFillColor ( sf::Color ( 255, 255, 255, 120 ) );
            canvas.draw ( shine );

            for ( int i = 0; i < 7; ++i )
            {
                sf::Vertex segment[] = {
                    {{22.f + i * 33.f, 0.f}, sf::Color ( 220, 245, 255, 110 )},
                    {{4.f + i * 33.f, 256.f}, sf::Color ( 220, 245, 255, 30 )},
                };
                canvas.draw ( segment, 2, sf::PrimitiveType::Lines );
            }
        } );

    textures_["block_ice"] = render_texture (
        "block_ice",
        [] ( sf::RenderTexture& canvas )
        {
            draw_base_rect ( canvas, sf::Color ( 198, 231, 255, 170 ) );

            for ( int i = 0; i < 10; ++i )
            {
                sf::Vertex crack[] = {
                    {{18.f + i * 24.f, 30.f + ( i % 3 ) * 20.f}, sf::Color ( 238, 248, 255, 120 )},
                    {{6.f + i * 24.f, 220.f - ( i % 4 ) * 26.f}, sf::Color ( 170, 210, 245, 110 )},
                };
                canvas.draw ( crack, 2, sf::PrimitiveType::Lines );
            }
        } );

    textures_["proj_standard"] = render_texture (
        "proj_standard",
        [] ( sf::RenderTexture& canvas )
        {
            sf::CircleShape body ( 104.f );
            body.setOrigin ( {104.f, 104.f} );
            body.setPosition ( {128.f, 128.f} );
            body.setFillColor ( sf::Color ( 204, 72, 68 ) );
            body.setOutlineThickness ( 10.f );
            body.setOutlineColor ( sf::Color ( 132, 34, 32 ) );
            canvas.draw ( body );

            sf::CircleShape highlight ( 34.f );
            highlight.setOrigin ( {34.f, 34.f} );
            highlight.setPosition ( {95.f, 90.f} );
            highlight.setFillColor ( sf::Color ( 255, 178, 165, 160 ) );
            canvas.draw ( highlight );
        } );

    textures_["proj_heavy"] = render_texture (
        "proj_heavy",
        [] ( sf::RenderTexture& canvas )
        {
            sf::CircleShape body ( 104.f );
            body.setOrigin ( {104.f, 104.f} );
            body.setPosition ( {128.f, 128.f} );
            body.setFillColor ( sf::Color ( 86, 58, 124 ) );
            body.setOutlineThickness ( 12.f );
            body.setOutlineColor ( sf::Color ( 52, 34, 78 ) );
            canvas.draw ( body );

            sf::CircleShape ring ( 58.f );
            ring.setOrigin ( {58.f, 58.f} );
            ring.setPosition ( {128.f, 128.f} );
            ring.setFillColor ( sf::Color::Transparent );
            ring.setOutlineThickness ( 10.f );
            ring.setOutlineColor ( sf::Color ( 170, 130, 214, 180 ) );
            canvas.draw ( ring );
        } );

    textures_["proj_splitter"] = render_texture (
        "proj_splitter",
        [] ( sf::RenderTexture& canvas )
        {
            sf::CircleShape body ( 104.f );
            body.setOrigin ( {104.f, 104.f} );
            body.setPosition ( {128.f, 128.f} );
            body.setFillColor ( sf::Color ( 65, 150, 214 ) );
            body.setOutlineThickness ( 10.f );
            body.setOutlineColor ( sf::Color ( 38, 98, 148 ) );
            canvas.draw ( body );

            sf::RectangleShape bar_h ( {94.f, 16.f} );
            bar_h.setOrigin ( {47.f, 8.f} );
            bar_h.setPosition ( {128.f, 128.f} );
            bar_h.setFillColor ( sf::Color ( 212, 240, 255, 180 ) );
            canvas.draw ( bar_h );

            sf::RectangleShape bar_v ( {16.f, 94.f} );
            bar_v.setOrigin ( {8.f, 47.f} );
            bar_v.setPosition ( {128.f, 128.f} );
            bar_v.setFillColor ( sf::Color ( 212, 240, 255, 180 ) );
            canvas.draw ( bar_v );
        } );

    textures_["target"] = render_texture (
        "target",
        [] ( sf::RenderTexture& canvas )
        {
            sf::CircleShape body ( 102.f );
            body.setOrigin ( {102.f, 102.f} );
            body.setPosition ( {128.f, 128.f} );
            body.setFillColor ( sf::Color ( 120, 210, 88 ) );
            body.setOutlineThickness ( 10.f );
            body.setOutlineColor ( sf::Color ( 64, 140, 52 ) );
            canvas.draw ( body );

            sf::CircleShape eye_white ( 18.f );
            eye_white.setOrigin ( {18.f, 18.f} );
            eye_white.setFillColor ( sf::Color ( 245, 252, 245 ) );
            eye_white.setPosition ( {97.f, 108.f} );
            canvas.draw ( eye_white );
            eye_white.setPosition ( {157.f, 108.f} );
            canvas.draw ( eye_white );

            sf::CircleShape pupil ( 7.f );
            pupil.setOrigin ( {7.f, 7.f} );
            pupil.setFillColor ( sf::Color ( 40, 58, 36 ) );
            pupil.setPosition ( {97.f, 108.f} );
            canvas.draw ( pupil );
            pupil.setPosition ( {157.f, 108.f} );
            canvas.draw ( pupil );

            sf::CircleShape snout ( 26.f );
            snout.setOrigin ( {26.f, 26.f} );
            snout.setPosition ( {128.f, 150.f} );
            snout.setFillColor ( sf::Color ( 156, 235, 125 ) );
            canvas.draw ( snout );
        } );

    textures_["slingshot_wood"] = render_texture (
        "slingshot_wood",
        [] ( sf::RenderTexture& canvas )
        {
            draw_base_rect ( canvas, sf::Color ( 118, 80, 46 ) );

            for ( int i = 0; i < 12; ++i )
            {
                sf::RectangleShape grain ( {static_cast<float> ( kTexSize ), 3.f} );
                grain.setPosition ( {0.f, 10.f + i * 21.f} );
                grain.setFillColor ( i % 2 == 0 ? sf::Color ( 138, 92, 56, 180 )
                                                : sf::Color ( 98, 65, 36, 170 ) );
                canvas.draw ( grain );
            }
        } );

    generated_ = true;
}

const sf::Texture& TextureManager::get ( const std::string& key )
{
    generate_all();
    return textures_.at ( key );
}

const sf::Texture& TextureManager::block ( Material material )
{
    switch ( material )
    {
    case Material::Stone:
        return get ( "block_stone" );
    case Material::Glass:
        return get ( "block_glass" );
    case Material::Ice:
        return get ( "block_ice" );
    case Material::Wood:
    default:
        return get ( "block_wood" );
    }
}

const sf::Texture& TextureManager::projectile ( ProjectileType type )
{
    switch ( type )
    {
    case ProjectileType::Heavy:
        return get ( "proj_heavy" );
    case ProjectileType::Splitter:
        return get ( "proj_splitter" );
    case ProjectileType::Standard:
    default:
        return get ( "proj_standard" );
    }
}

const sf::Texture& TextureManager::target()
{
    return get ( "target" );
}

const sf::Texture& TextureManager::slingshot_wood()
{
    return get ( "slingshot_wood" );
}

}  // namespace angry
