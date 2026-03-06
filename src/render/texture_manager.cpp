#include "render/texture_manager.hpp"

#include <cmath>
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

    textures_["proj_dasher"] = render_texture (
        "proj_dasher",
        [] ( sf::RenderTexture& canvas )
        {
            sf::CircleShape body ( 104.f );
            body.setOrigin ( {104.f, 104.f} );
            body.setPosition ( {128.f, 128.f} );
            body.setFillColor ( sf::Color ( 242, 156, 66 ) );
            body.setOutlineThickness ( 10.f );
            body.setOutlineColor ( sf::Color ( 154, 82, 28 ) );
            canvas.draw ( body );

            sf::ConvexShape chevron ( 3 );
            chevron.setPoint ( 0, {0.f, 0.f} );
            chevron.setPoint ( 1, {62.f, 18.f} );
            chevron.setPoint ( 2, {0.f, 36.f} );
            chevron.setFillColor ( sf::Color ( 255, 233, 178, 225 ) );
            chevron.setPosition ( {102.f, 110.f} );
            canvas.draw ( chevron );
            chevron.setPosition ( {126.f, 110.f} );
            canvas.draw ( chevron );
        } );

    textures_["proj_bomber"] = render_texture (
        "proj_bomber",
        [] ( sf::RenderTexture& canvas )
        {
            sf::CircleShape body ( 104.f );
            body.setOrigin ( {104.f, 104.f} );
            body.setPosition ( {128.f, 128.f} );
            body.setFillColor ( sf::Color ( 60, 64, 76 ) );
            body.setOutlineThickness ( 13.f );
            body.setOutlineColor ( sf::Color ( 30, 32, 40 ) );
            canvas.draw ( body );

            sf::CircleShape body_shadow ( 92.f );
            body_shadow.setOrigin ( {92.f, 92.f} );
            body_shadow.setPosition ( {132.f, 136.f} );
            body_shadow.setFillColor ( sf::Color ( 10, 12, 16, 85 ) );
            canvas.draw ( body_shadow );

            sf::CircleShape shine ( 38.f );
            shine.setOrigin ( {38.f, 38.f} );
            shine.setPosition ( {92.f, 90.f} );
            shine.setFillColor ( sf::Color ( 188, 194, 214, 76 ) );
            canvas.draw ( shine );

            sf::CircleShape seam_ring ( 72.f );
            seam_ring.setOrigin ( {72.f, 72.f} );
            seam_ring.setPosition ( {128.f, 128.f} );
            seam_ring.setFillColor ( sf::Color::Transparent );
            seam_ring.setOutlineThickness ( 4.f );
            seam_ring.setOutlineColor ( sf::Color ( 24, 26, 34, 178 ) );
            canvas.draw ( seam_ring );

            sf::CircleShape warning_ring ( 62.f );
            warning_ring.setOrigin ( {62.f, 62.f} );
            warning_ring.setPosition ( {128.f, 136.f} );
            warning_ring.setFillColor ( sf::Color::Transparent );
            warning_ring.setOutlineThickness ( 11.f );
            warning_ring.setOutlineColor ( sf::Color ( 246, 146, 62, 226 ) );
            canvas.draw ( warning_ring );

            for ( int i = 0; i < 8; ++i )
            {
                const float angle = 6.2831853f * static_cast<float> ( i ) / 8.f;
                const float x = 128.f + std::cos ( angle ) * 80.f;
                const float y = 128.f + std::sin ( angle ) * 80.f;

                sf::CircleShape rivet ( 7.f );
                rivet.setOrigin ( {7.f, 7.f} );
                rivet.setPosition ( {x, y} );
                rivet.setFillColor ( sf::Color ( 124, 128, 142 ) );
                rivet.setOutlineThickness ( 2.f );
                rivet.setOutlineColor ( sf::Color ( 56, 58, 70 ) );
                canvas.draw ( rivet );
            }

            sf::RectangleShape fuse_base ( {20.f, 58.f} );
            fuse_base.setOrigin ( {10.f, 54.f} );
            fuse_base.setPosition ( {178.f, 66.f} );
            fuse_base.setRotation ( sf::degrees ( 21.f ) );
            fuse_base.setFillColor ( sf::Color ( 228, 208, 172 ) );
            canvas.draw ( fuse_base );

            sf::RectangleShape fuse_thread ( {14.f, 52.f} );
            fuse_thread.setOrigin ( {7.f, 48.f} );
            fuse_thread.setPosition ( {177.f, 67.f} );
            fuse_thread.setRotation ( sf::degrees ( 21.f ) );
            fuse_thread.setFillColor ( sf::Color ( 174, 122, 82, 165 ) );
            canvas.draw ( fuse_thread );

            sf::CircleShape spark_glow ( 14.f );
            spark_glow.setOrigin ( {14.f, 14.f} );
            spark_glow.setPosition ( {194.f, 30.f} );
            spark_glow.setFillColor ( sf::Color ( 255, 182, 92, 168 ) );
            canvas.draw ( spark_glow );

            sf::CircleShape spark_core ( 8.f );
            spark_core.setOrigin ( {8.f, 8.f} );
            spark_core.setPosition ( {194.f, 30.f} );
            spark_core.setFillColor ( sf::Color ( 255, 234, 160, 228 ) );
            canvas.draw ( spark_core );

            sf::CircleShape cap ( 12.f );
            cap.setOrigin ( {12.f, 12.f} );
            cap.setPosition ( {171.f, 74.f} );
            cap.setFillColor ( sf::Color ( 52, 54, 66 ) );
            cap.setOutlineThickness ( 2.f );
            cap.setOutlineColor ( sf::Color ( 102, 108, 128, 200 ) );
            canvas.draw ( cap );
        } );

    textures_["proj_dropper"] = render_texture (
        "proj_dropper",
        [] ( sf::RenderTexture& canvas )
        {
            sf::CircleShape body ( 104.f );
            body.setOrigin ( {104.f, 104.f} );
            body.setPosition ( {128.f, 128.f} );
            body.setFillColor ( sf::Color ( 74, 176, 140 ) );
            body.setOutlineThickness ( 11.f );
            body.setOutlineColor ( sf::Color ( 38, 102, 82 ) );
            canvas.draw ( body );

            sf::CircleShape shine ( 36.f );
            shine.setOrigin ( {36.f, 36.f} );
            shine.setPosition ( {92.f, 88.f} );
            shine.setFillColor ( sf::Color ( 198, 244, 224, 70 ) );
            canvas.draw ( shine );

            sf::CircleShape cargo_ring ( 64.f );
            cargo_ring.setOrigin ( {64.f, 64.f} );
            cargo_ring.setPosition ( {128.f, 136.f} );
            cargo_ring.setFillColor ( sf::Color::Transparent );
            cargo_ring.setOutlineThickness ( 10.f );
            cargo_ring.setOutlineColor ( sf::Color ( 186, 248, 226, 220 ) );
            canvas.draw ( cargo_ring );

            sf::RectangleShape hatch ( {72.f, 26.f} );
            hatch.setOrigin ( {36.f, 13.f} );
            hatch.setPosition ( {128.f, 166.f} );
            hatch.setFillColor ( sf::Color ( 42, 126, 98, 220 ) );
            hatch.setOutlineThickness ( 3.f );
            hatch.setOutlineColor ( sf::Color ( 204, 252, 236, 184 ) );
            canvas.draw ( hatch );

            sf::RectangleShape latch ( {16.f, 12.f} );
            latch.setOrigin ( {8.f, 6.f} );
            latch.setPosition ( {128.f, 166.f} );
            latch.setFillColor ( sf::Color ( 242, 252, 246, 220 ) );
            canvas.draw ( latch );

            sf::RectangleShape fin_left ( {42.f, 16.f} );
            fin_left.setOrigin ( {42.f, 8.f} );
            fin_left.setPosition ( {110.f, 132.f} );
            fin_left.setRotation ( sf::degrees ( -26.f ) );
            fin_left.setFillColor ( sf::Color ( 206, 250, 232, 168 ) );
            canvas.draw ( fin_left );

            sf::RectangleShape fin_right ( {42.f, 16.f} );
            fin_right.setOrigin ( {0.f, 8.f} );
            fin_right.setPosition ( {146.f, 132.f} );
            fin_right.setRotation ( sf::degrees ( 26.f ) );
            fin_right.setFillColor ( sf::Color ( 206, 250, 232, 168 ) );
            canvas.draw ( fin_right );

            sf::RectangleShape chain ( {8.f, 32.f} );
            chain.setOrigin ( {4.f, 0.f} );
            chain.setPosition ( {128.f, 178.f} );
            chain.setFillColor ( sf::Color ( 196, 236, 220, 188 ) );
            canvas.draw ( chain );

            sf::CircleShape payload ( 18.f );
            payload.setOrigin ( {18.f, 18.f} );
            payload.setPosition ( {128.f, 214.f} );
            payload.setFillColor ( sf::Color ( 124, 220, 184 ) );
            payload.setOutlineThickness ( 3.f );
            payload.setOutlineColor ( sf::Color ( 216, 252, 238, 210 ) );
            canvas.draw ( payload );

            sf::CircleShape payload_core ( 7.f );
            payload_core.setOrigin ( {7.f, 7.f} );
            payload_core.setPosition ( {128.f, 214.f} );
            payload_core.setFillColor ( sf::Color ( 238, 255, 248, 220 ) );
            canvas.draw ( payload_core );
        } );

    textures_["proj_boomerang"] = render_texture (
        "proj_boomerang",
        [] ( sf::RenderTexture& canvas )
        {
            sf::CircleShape body ( 104.f );
            body.setOrigin ( {104.f, 104.f} );
            body.setPosition ( {128.f, 128.f} );
            body.setFillColor ( sf::Color ( 150, 190, 76 ) );
            body.setOutlineThickness ( 10.f );
            body.setOutlineColor ( sf::Color ( 88, 122, 42 ) );
            canvas.draw ( body );

            sf::RectangleShape arm_a ( {70.f, 18.f} );
            arm_a.setOrigin ( {8.f, 9.f} );
            arm_a.setPosition ( {106.f, 116.f} );
            arm_a.setRotation ( sf::degrees ( -28.f ) );
            arm_a.setFillColor ( sf::Color ( 240, 252, 196, 214 ) );
            canvas.draw ( arm_a );

            sf::RectangleShape arm_b ( {70.f, 18.f} );
            arm_b.setOrigin ( {8.f, 9.f} );
            arm_b.setPosition ( {106.f, 144.f} );
            arm_b.setRotation ( sf::degrees ( 30.f ) );
            arm_b.setFillColor ( sf::Color ( 240, 252, 196, 214 ) );
            canvas.draw ( arm_b );
        } );

    textures_["proj_bubbler"] = render_texture (
        "proj_bubbler",
        [] ( sf::RenderTexture& canvas )
        {
            sf::CircleShape body ( 104.f );
            body.setOrigin ( {104.f, 104.f} );
            body.setPosition ( {128.f, 128.f} );
            body.setFillColor ( sf::Color ( 86, 190, 236 ) );
            body.setOutlineThickness ( 10.f );
            body.setOutlineColor ( sf::Color ( 44, 116, 152 ) );
            canvas.draw ( body );

            for ( int i = 0; i < 4; ++i )
            {
                const float radius = 13.f + i * 4.f;
                sf::CircleShape bubble ( radius );
                bubble.setOrigin ( {radius, radius} );
                bubble.setPosition ( {90.f + i * 28.f, 86.f + ( i % 2 ) * 22.f} );
                bubble.setFillColor ( sf::Color ( 230, 248, 255, 74 ) );
                bubble.setOutlineThickness ( 2.f );
                bubble.setOutlineColor ( sf::Color ( 235, 251, 255, 178 ) );
                canvas.draw ( bubble );
            }
        } );

    textures_["proj_inflater"] = render_texture (
        "proj_inflater",
        [] ( sf::RenderTexture& canvas )
        {
            sf::CircleShape body ( 104.f );
            body.setOrigin ( {104.f, 104.f} );
            body.setPosition ( {128.f, 128.f} );
            body.setFillColor ( sf::Color ( 230, 112, 168 ) );
            body.setOutlineThickness ( 10.f );
            body.setOutlineColor ( sf::Color ( 146, 56, 104 ) );
            canvas.draw ( body );

            sf::CircleShape ring ( 58.f );
            ring.setOrigin ( {58.f, 58.f} );
            ring.setPosition ( {128.f, 128.f} );
            ring.setFillColor ( sf::Color::Transparent );
            ring.setOutlineThickness ( 7.f );
            ring.setOutlineColor ( sf::Color ( 255, 214, 232, 195 ) );
            canvas.draw ( ring );

            sf::RectangleShape plus_h ( {62.f, 14.f} );
            plus_h.setOrigin ( {31.f, 7.f} );
            plus_h.setPosition ( {128.f, 128.f} );
            plus_h.setFillColor ( sf::Color ( 255, 230, 242, 220 ) );
            canvas.draw ( plus_h );

            sf::RectangleShape plus_v ( {14.f, 62.f} );
            plus_v.setOrigin ( {7.f, 31.f} );
            plus_v.setPosition ( {128.f, 128.f} );
            plus_v.setFillColor ( sf::Color ( 255, 230, 242, 220 ) );
            canvas.draw ( plus_v );
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
    case ProjectileType::Dasher:
        return get ( "proj_dasher" );
    case ProjectileType::Bomber:
        return get ( "proj_bomber" );
    case ProjectileType::Dropper:
        return get ( "proj_dropper" );
    case ProjectileType::Boomerang:
        return get ( "proj_boomerang" );
    case ProjectileType::Bubbler:
        return get ( "proj_bubbler" );
    case ProjectileType::Inflater:
        return get ( "proj_inflater" );
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
