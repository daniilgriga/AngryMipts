#pragma once

// ============================================================
// platform_raylib.hpp — Raylib backend (Emscripten/web builds).
// Defines platform:: types matching the SFML backend API
// so that platform-agnostic code compiles on both targets.
// ============================================================

#include <raylib.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace platform
{
struct Texture;
}  // namespace platform

namespace sf
{

enum class PrimitiveType
{
    Points = 0,
    Lines = 1,
    LineStrip = 2,
    Triangles = 3,
    TriangleStrip = 4,
    TriangleFan = 5,
};

struct Text
{
    enum Style
    {
        Regular = 0,
        Bold = 1,
    };
};

struct Vector2i
{
    int x = 0;
    int y = 0;
};

struct IntRect
{
    Vector2i position;
    Vector2i size;
    IntRect() = default;
    IntRect( Vector2i pos, Vector2i sz ) : position( pos ), size( sz ) {}
};

using Texture = platform::Texture;

inline float degrees( float v ) { return v; }

}  // namespace sf

namespace platform
{

// ── Geometry ────────────────────────────────────────────────

struct Vec2u;

struct Vec2f
{
    float x = 0, y = 0;
    Vec2f() = default;
    Vec2f( float x, float y ) : x(x), y(y) {}
    explicit Vec2f( Vec2u v );
    Vec2f  operator+( Vec2f o ) const { return { x + o.x, y + o.y }; }
    Vec2f  operator-( Vec2f o ) const { return { x - o.x, y - o.y }; }
    Vec2f  operator*( float s ) const { return { x * s,   y * s   }; }
    Vec2f  operator/( float s ) const { return { x / s,   y / s   }; }
    Vec2f& operator+=( Vec2f o ) { x += o.x; y += o.y; return *this; }
    Vec2f& operator-=( Vec2f o ) { x -= o.x; y -= o.y; return *this; }
    Vec2f& operator*=( float s ) { x *= s; y *= s; return *this; }
};
struct Vec2u { unsigned x = 0, y = 0; };
struct Vec2i { int x = 0, y = 0; };

inline Vec2f::Vec2f( Vec2u v )
    : x( static_cast<float> ( v.x ) ),
      y( static_cast<float> ( v.y ) )
{
}

struct Rect
{
    float left = 0, top = 0, width = 0, height = 0;
    Rect() = default;
    Rect( Vec2f pos, Vec2f size )
        : left(pos.x), top(pos.y), width(size.x), height(size.y) {}
    Rect( float l, float t, float w, float h )
        : left(l), top(t), width(w), height(h) {}
    bool contains( Vec2f p ) const
    {
        return p.x >= left && p.x < left + width
            && p.y >= top  && p.y < top  + height;
    }
};
using FloatRect = Rect;

struct Transform
{
    // 3×3 affine matrix (row-major, identity by default)
    float m[9] = { 1,0,0, 0,1,0, 0,0,1 };
};

// ── Color ───────────────────────────────────────────────────

struct Color
{
    uint8_t r = 0, g = 0, b = 0, a = 255;
    Color() = default;
    Color( uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255 )
        : r(r), g(g), b(b), a(a) {}

    static const Color White;
    static const Color Black;
    static const Color Transparent;

    ::Color to_rl() const { return { r, g, b, a }; }
};

inline const Color Color::White       { 255, 255, 255, 255 };
inline const Color Color::Black       {   0,   0,   0, 255 };
inline const Color Color::Transparent {   0,   0,   0,   0 };

// ── Time / Clock ────────────────────────────────────────────

struct Time
{
    float seconds = 0.f;
    float asSeconds()      const { return seconds; }
    int   asMilliseconds() const { return static_cast<int>(seconds * 1000.f); }
};

inline Time seconds( float s ) { return { s }; }
inline Time milliseconds( int ms ) { return { ms / 1000.f }; }

struct Clock
{
    double start_ = GetTime();
    Time getElapsedTime() const { return { float(GetTime() - start_) }; }
    Time restart() { float e = float(GetTime() - start_); start_ = GetTime(); return { e }; }
};

// ── Font / Text ─────────────────────────────────────────────

struct Font
{
    ::Font rl {};
    bool   loaded = false;
    bool openFromFile( const std::string& path )
    {
        // Load at high resolution so DrawTextEx can scale down cleanly.
        // LoadFont default is 32 px — scaling up from that causes pixellation.
        rl = ::LoadFontEx( path.c_str(), 128, nullptr, 0 );
        loaded = IsFontValid( rl );
        if ( loaded )
            SetTextureFilter( rl.texture, TEXTURE_FILTER_BILINEAR );
        return loaded;
    }
};

// Text is not a stored object in Raylib — it's drawn immediately.
// We keep a data struct matching sf::Text fields so scenes compile unchanged.
struct Text
{
    const Font*  font_     = nullptr;
    std::string  string_;
    unsigned     char_size_ = 18;
    Color        fill_color_    { 255, 255, 255, 255 };
    Color        outline_color_ { 0, 0, 0, 0 };
    float        outline_thickness_ = 0.f;
    Vec2f        position_;
    Vec2f        origin_;
    bool         bold_ = false;

    Text() = default;
    Text( const Font& f, const std::string& s, unsigned size )
        : font_(&f), string_(s), char_size_(size) {}

    void setString( const std::string& s )  { string_ = s; }
    void setCharacterSize( unsigned s )      { char_size_ = s; }
    void setFillColor( Color c )             { fill_color_ = c; }
    void setOutlineColor( Color c )          { outline_color_ = c; }
    void setOutlineThickness( float t )      { outline_thickness_ = t; }
    void setStyle( int /*flags*/ )           {}  // Bold handled separately
    void setPosition( Vec2f p )              { position_ = p; }
    void setOrigin( Vec2f o )                { origin_ = o; }

    struct Bounds { float left, top, width, height; };
    Bounds getLocalBounds() const
    {
        if ( !font_ || !font_->loaded ) return {};
        ::Vector2 sz = MeasureTextEx( font_->rl, string_.c_str(),
                                       float(char_size_), 1.f );
        return { 0.f, 0.f, sz.x, sz.y };
    }
    Bounds getGlobalBounds() const
    {
        auto lb = getLocalBounds();
        return { position_.x - origin_.x + lb.left,
                 position_.y - origin_.y + lb.top,
                 lb.width, lb.height };
    }
};

// ── Texture / Image ─────────────────────────────────────────

struct Image
{
    ::Image rl {};
    bool    ready = false;

    void create( unsigned w, unsigned h, Color fill = {} )
    {
        rl = GenImageColor( int(w), int(h), fill.to_rl() );
        ready = true;
    }
    void setPixel( unsigned x, unsigned y, Color c )
    {
        ImageDrawPixel( &rl, int(x), int(y), c.to_rl() );
    }
    void saveToFile( const std::string& /*path*/ ) const
    {
        // On web: no disk — skip silently.
    }
    Vec2u getSize() const
    {
        return { unsigned(rl.width), unsigned(rl.height) };
    }
};

struct Texture
{
    ::Texture2D rl {};
    bool        loaded = false;

    void loadFromImage( const Image& img )
    {
        rl     = LoadTextureFromImage( img.rl );
        loaded = IsTextureValid( rl );
    }
    void setSmooth( bool s )
    {
        if ( !loaded )
        {
            return;
        }
        SetTextureFilter( rl, s ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT );
    }
    Vec2u getSize() const { return { unsigned(rl.width), unsigned(rl.height) }; }
};

// ── Shader ──────────────────────────────────────────────────

struct Shader
{
    ::Shader rl {};
    bool     loaded = false;

    enum class Type { Fragment, Vertex };
    static bool isAvailable() { return true; }

    bool loadFromMemory( const std::string& src, Type type )
    {
        const char* vs = ( type == Type::Fragment ) ? nullptr : src.c_str();
        const char* fs = ( type == Type::Fragment ) ? src.c_str() : nullptr;
        rl     = LoadShaderFromMemory( vs, fs );
        loaded = IsShaderValid( rl );
        return loaded;
    }

    void setUniform( const std::string& name, float v )
    {
        int loc = GetShaderLocation( rl, name.c_str() );
        SetShaderValue( rl, loc, &v, SHADER_UNIFORM_FLOAT );
    }
    void setUniform( const std::string& name, Vec2f v )
    {
        int loc = GetShaderLocation( rl, name.c_str() );
        float vals[2] = { v.x, v.y };
        SetShaderValue( rl, loc, vals, SHADER_UNIFORM_VEC2 );
    }
};

// ── View (camera / letterbox) ────────────────────────────────

struct View
{
    Vec2f center_;
    Vec2f size_;
    Rect  viewport_ { 0.f, 0.f, 1.f, 1.f };  // normalized 0..1

    View() = default;
    View( Rect r ) : center_{ r.left + r.width*0.5f, r.top + r.height*0.5f },
                     size_{ r.width, r.height } {}

    void setCenter( Vec2f c )   { center_ = c; }
    void setSize( Vec2f s )     { size_   = s; }
    void setViewport( Rect vp ) { viewport_ = vp; }
    void reset( Rect r )
    {
        center_ = { r.left + r.width*0.5f, r.top + r.height*0.5f };
        size_   = { r.width, r.height };
    }
    Vec2f getSize()   const { return size_; }
    Vec2f getCenter() const { return center_; }
};

// ── Event ────────────────────────────────────────────────────

struct KeyEvent
{
    int key = 0;
    int code = 0;
    bool pressed = true;
    bool alt = false;
    bool shift = false;
    bool ctrl = false;
};
struct MouseBtnEvent  { float x, y; int button; bool pressed = true; };  // button: 0=left,1=right,2=mid
struct MouseMoveEvent { float x, y; };
struct MouseWheelEvent{ float delta; float x, y; };
struct TextEvent  { uint32_t unicode; };
struct ResizedEvent   { unsigned w, h; };
struct ClosedEvent    {};
struct FocusEvent     {};

using Event = std::variant<
    KeyEvent,
    MouseBtnEvent,
    MouseMoveEvent,
    MouseWheelEvent,
    TextEvent,
    ResizedEvent,
    ClosedEvent,
    FocusEvent
>;

// ── Window ──────────────────────────────────────────────────
// Wraps Raylib window + provides sf::RenderWindow-compatible API.

struct RectShape;
struct CircleShape;
struct ConvexShape;
struct Sprite;
struct Vertex;
struct VertexArray;
struct RenderTexture; // forward
struct Window
{
    bool open_ = false;
    int  w_ = 1280, h_ = 720;
    float last_mouse_x_ = 0.f;
    float last_mouse_y_ = 0.f;
    bool mouse_initialized_ = false;
    bool was_focused_ = true;
    View default_view_ { Rect { 0.f, 0.f, 1280.f, 720.f } };
    View current_view_ { Rect { 0.f, 0.f, 1280.f, 720.f } };

    void create( unsigned w, unsigned h, const std::string& title )
    {
        InitWindow( int(w), int(h), title.c_str() );
        open_ = true;
        w_ = int(w); h_ = int(h);
        default_view_.reset ( { 0.f, 0.f, static_cast<float> ( w_ ), static_cast<float> ( h_ ) } );
        default_view_.setViewport ( { 0.f, 0.f, 1.f, 1.f } );
        current_view_ = default_view_;
    }
    bool isOpen() const { return open_ && !WindowShouldClose(); }
    void close()        { open_ = false; CloseWindow(); }
    void display()      { EndDrawing(); }
    void clear( Color c = {} ) { BeginDrawing(); ClearBackground( c.to_rl() ); }
    void setFramerateLimit( unsigned /*fps*/ )
    {
        // On web, emscripten_set_main_loop with fps=0 uses requestAnimationFrame
        // (native monitor rate). Calling SetTargetFPS() here would cap to 30 in
        // some browsers. Leave FPS uncapped; the browser's vsync handles pacing.
        SetTargetFPS( 0 );
    }
    void setVerticalSyncEnabled( bool /*v*/ ) {}
    void setView( const View& view ) { current_view_ = view; }
    const View& getDefaultView() const { return default_view_; }
    Vec2f mapPixelToCoords( Vec2i pixel, const View& view ) const;

    Vec2u getSize() const { return { unsigned(GetScreenWidth()), unsigned(GetScreenHeight()) }; }

    void draw( const RectShape& shape );
    void draw( const CircleShape& shape );
    void draw( const ConvexShape& shape );
    void draw( const Sprite& sprite );
    void draw( const Text& text );
    void draw( const Vertex* vertices, std::size_t count, sf::PrimitiveType type );
    void draw( const VertexArray& vertices );

    // Events are polled separately via poll_events()
    std::vector<Event> pollEvents();
};

// ── Poll events (Raylib input → Event stream) ────────────────

std::vector<Event> poll_events( Window& w );

// ── Angle helper (matches sf::degrees) ──────────────────────
inline float degrees( float v ) { return v; }  // Raylib uses degrees natively

// ── RenderTarget stub ────────────────────────────────────────
// On Raylib there's no separate RenderTarget type at this level —
// drawing goes directly to the screen or a RenderTexture.
// Scene render() takes Window& and we add a RenderTexture variant later.
using RenderTarget = Window;

// ── Primitive types stubs (used by particles / renderer) ─────

struct Vertex
{
    Vec2f    position;
    Color    color { 255,255,255,255 };
};

struct VertexArray
{
    int                prim_type = 0;  // matches Raylib DrawPrimitive types
    std::vector<Vertex> verts;
    VertexArray() = default;
    explicit VertexArray( sf::PrimitiveType type, std::size_t count = 0 )
        : prim_type ( static_cast<int> ( type ) ),
          verts ( count )
    {
    }
    void resize( std::size_t n ) { verts.resize(n); }
    Vertex& operator[]( std::size_t i ) { return verts[i]; }
    const Vertex& operator[]( std::size_t i ) const { return verts[i]; }
    std::size_t getVertexCount() const { return verts.size(); }
};

struct RectShape
{
    Vec2f pos_, size_, origin_;
    Color fill_, outline_;
    float outline_t_ = 0.f;
    float rotation_  = 0.f;
    RectShape() = default;
    explicit RectShape( Vec2f size ) : size_( size ) {}

    void setPosition( Vec2f p )            { pos_       = p; }
    void setSize( Vec2f s )                { size_      = s; }
    void setFillColor( Color c )           { fill_      = c; }
    void setOutlineColor( Color c )        { outline_   = c; }
    void setOutlineThickness( float t )    { outline_t_ = t; }
    void setOrigin( Vec2f o )              { origin_    = o; }
    void setRotation( float deg )          { rotation_  = deg; }
    Rect getGlobalBounds() const
    {
        return { pos_.x - origin_.x, pos_.y - origin_.y, size_.x, size_.y };
    }
};

struct CircleShape
{
    Vec2f  pos_, origin_;
    Color  fill_, outline_;
    float  outline_t_ = 0.f;
    float  radius_    = 0.f;
    int    point_count_ = 32;
    float  rotation_ = 0.f;
    CircleShape() = default;
    explicit CircleShape( float radius ) : radius_( radius ) {}

    void setPosition( Vec2f p )         { pos_         = p; }
    void setRadius( float r )           { radius_      = r; }
    void setFillColor( Color c )        { fill_        = c; }
    void setOutlineColor( Color c )     { outline_     = c; }
    void setOutlineThickness( float t ) { outline_t_   = t; }
    void setOrigin( Vec2f o )           { origin_      = o; }
    void setPointCount( int n )         { point_count_ = n; }
    void setRotation( float deg )       { rotation_    = deg; }
    float getRadius() const             { return radius_; }
};

struct ConvexShape
{
    std::vector<Vec2f> points_;
    Vec2f   pos_, origin_;
    Color   fill_, outline_;
    float   outline_t_ = 0.f;
    float   rotation_ = 0.f;
    const Texture* tex_ = nullptr;
    ConvexShape() = default;
    explicit ConvexShape( int n ) { setPointCount( n ); }

    void setPointCount( int n )             { points_.resize(n); }
    void setPoint( int i, Vec2f p )         { points_[i] = p; }
    void setPosition( Vec2f p )             { pos_       = p; }
    void setOrigin( Vec2f o )               { origin_    = o; }
    void setFillColor( Color c )            { fill_      = c; }
    void setOutlineColor( Color c )         { outline_   = c; }
    void setOutlineThickness( float t )     { outline_t_ = t; }
    void setRotation( float deg )           { rotation_  = deg; }
    void setTextureRect( const sf::IntRect& /*rect*/ ) {}
    void setTexture( const Texture* t )     { tex_       = t; }
};

struct Sprite
{
    const Texture* tex_      = nullptr;
    Vec2f          pos_, origin_, scale_ { 1.f, 1.f };
    float          rotation_ = 0.f;
    Color          color_    { 255,255,255,255 };
    Sprite() = default;
    explicit Sprite( const Texture& tex ) { setTexture( tex ); }

    void setTexture( const Texture& t )     { tex_      = &t; }
    void setPosition( Vec2f p )             { pos_      = p; }
    void setOrigin( Vec2f o )               { origin_   = o; }
    void setScale( Vec2f s )                { scale_    = s; }
    void setRotation( float deg )           { rotation_ = deg; }
    void setColor( Color c )                { color_    = c; }
};

// ── Off-screen render texture ────────────────────────────────

struct RenderTexture
{
    ::RenderTexture2D rl {};
    bool ready = false;
    Texture color_tex;

    bool create( unsigned w, unsigned h )
    {
        rl    = LoadRenderTexture( int(w), int(h) );
        ready = IsRenderTextureValid( rl );
        color_tex.rl     = rl.texture;
        color_tex.loaded = ready;
        return ready;
    }
    void clear( Color c = {} )
    {
        BeginTextureMode( rl );
        ClearBackground( c.to_rl() );
    }
    void display() { EndTextureMode(); }
    const Texture& getTexture() const { return color_tex; }
};

// ── Audio ────────────────────────────────────────────────────

struct SoundBuffer
{
    ::Wave    wave {};
    bool      loaded = false;

    bool loadFromSamples( const int16_t* samples, std::size_t count,
                          unsigned channels, unsigned sampleRate )
    {
        wave.frameCount = unsigned(count / channels);
        wave.sampleRate = sampleRate;
        wave.sampleSize = 16;
        wave.channels   = channels;
        // Copy samples
        std::size_t bytes = count * sizeof(int16_t);
        wave.data = malloc( bytes );
        if ( wave.data ) { memcpy( wave.data, samples, bytes ); loaded = true; }
        return loaded;
    }
    std::size_t getSampleCount() const
    {
        return wave.frameCount * wave.channels;
    }
};

struct Sound
{
    ::Sound rl {};
    bool    loaded = false;

    void load( const SoundBuffer& buf )
    {
        rl     = LoadSoundFromWave( buf.wave );
        loaded = IsSoundValid( rl );
    }

    enum class Status { Stopped, Playing, Paused };
    Status getStatus() const
    {
        return IsSoundPlaying( rl ) ? Status::Playing : Status::Stopped;
    }

    void setVolume( float v )  { SetSoundVolume( rl, v / 100.f ); }
    void setPitch( float p )   { SetSoundPitch( rl, p ); }
    void play()                { PlaySound( rl ); }
    void stop()                { StopSound( rl ); }
};

inline void draw_colored_triangle( const Vertex& a, const Vertex& b, const Vertex& c )
{
    // DrawTriangle requires CCW winding in screen space (Y-down).
    // Average the three vertex colours — WebGL 1 doesn't support per-vertex
    // colour interpolation without a custom shader, and rlBegin/rlEnd nested
    // inside BeginDrawing() causes MAX_VERTEX_ATTRIBS spam on Emscripten.
    const ::Color avg {
        static_cast<uint8_t>( ( int(a.color.r) + b.color.r + c.color.r ) / 3 ),
        static_cast<uint8_t>( ( int(a.color.g) + b.color.g + c.color.g ) / 3 ),
        static_cast<uint8_t>( ( int(a.color.b) + b.color.b + c.color.b ) / 3 ),
        static_cast<uint8_t>( ( int(a.color.a) + b.color.a + c.color.a ) / 3 ),
    };
    const Vector2 pa { a.position.x, a.position.y };
    const Vector2 pb { b.position.x, b.position.y };
    const Vector2 pc { c.position.x, c.position.y };
    // Determine winding; flip if CW so DrawTriangle always gets CCW.
    const float cross = ( pb.x - pa.x ) * ( pc.y - pa.y )
                      - ( pb.y - pa.y ) * ( pc.x - pa.x );
    if ( cross >= 0.f )
        DrawTriangle( pa, pb, pc, avg );
    else
        DrawTriangle( pa, pc, pb, avg );
}

inline Rect viewport_pixels( const Window& window, const View& view )
{
    const float screen_w = static_cast<float> ( std::max ( 1, GetScreenWidth() ) );
    const float screen_h = static_cast<float> ( std::max ( 1, GetScreenHeight() ) );

    const float left = view.viewport_.left * screen_w;
    const float top = view.viewport_.top * screen_h;
    const float width = std::max ( 1.f, view.viewport_.width * screen_w );
    const float height = std::max ( 1.f, view.viewport_.height * screen_h );

    ( void ) window;
    return { left, top, width, height };
}

inline Vec2f world_to_screen( const Window& window, const View& view, Vec2f p )
{
    const Rect vp = viewport_pixels ( window, view );
    const float view_w = std::max ( 1e-4f, view.size_.x );
    const float view_h = std::max ( 1e-4f, view.size_.y );
    const float left = view.center_.x - view_w * 0.5f;
    const float top = view.center_.y - view_h * 0.5f;
    const float nx = ( p.x - left ) / view_w;
    const float ny = ( p.y - top ) / view_h;
    return { vp.left + nx * vp.width, vp.top + ny * vp.height };
}

inline Vec2f screen_to_world( const Window& window, const View& view, Vec2f p )
{
    const Rect vp = viewport_pixels ( window, view );
    const float view_w = std::max ( 1e-4f, view.size_.x );
    const float view_h = std::max ( 1e-4f, view.size_.y );
    const float nx = ( p.x - vp.left ) / vp.width;
    const float ny = ( p.y - vp.top ) / vp.height;
    const float left = view.center_.x - view_w * 0.5f;
    const float top = view.center_.y - view_h * 0.5f;
    return { left + nx * view_w, top + ny * view_h };
}

inline Vec2f world_scale_to_screen( const Window& window, const View& view, Vec2f s )
{
    const Rect vp = viewport_pixels ( window, view );
    const float view_w = std::max ( 1e-4f, view.size_.x );
    const float view_h = std::max ( 1e-4f, view.size_.y );
    return { s.x * ( vp.width / view_w ), s.y * ( vp.height / view_h ) };
}

inline Vec2f Window::mapPixelToCoords( Vec2i pixel, const View& view ) const
{
    return screen_to_world ( *this,
                             view,
                             { static_cast<float> ( pixel.x ),
                               static_cast<float> ( pixel.y ) } );
}

inline void Window::draw( const RectShape& shape )
{
    const Vec2f scale = world_scale_to_screen ( *this, current_view_, { 1.f, 1.f } );
    const Vec2f pos = world_to_screen ( *this, current_view_, shape.pos_ );

    Rectangle rect {
        pos.x - shape.origin_.x * scale.x,
        pos.y - shape.origin_.y * scale.y,
        shape.size_.x * scale.x,
        shape.size_.y * scale.y
    };
    DrawRectanglePro( rect, { shape.origin_.x * scale.x, shape.origin_.y * scale.y },
                      shape.rotation_, shape.fill_.to_rl() );
    if ( shape.outline_t_ > 0.f )
    {
        const float outline = std::max ( 0.5f, shape.outline_t_ * std::min ( scale.x, scale.y ) );
        DrawRectangleLinesEx( rect, outline, shape.outline_.to_rl() );
    }
}

inline void Window::draw( const CircleShape& shape )
{
    const Vec2f center_world {
        shape.pos_.x - shape.origin_.x + shape.radius_,
        shape.pos_.y - shape.origin_.y + shape.radius_
    };
    const Vec2f center_screen = world_to_screen ( *this, current_view_, center_world );
    const Vec2f scale = world_scale_to_screen ( *this, current_view_, { 1.f, 1.f } );
    const float radius = shape.radius_ * 0.5f * ( scale.x + scale.y );

    DrawCircleV( { center_screen.x, center_screen.y }, radius, shape.fill_.to_rl() );
    if ( shape.outline_t_ > 0.f )
    {
        const float outline = std::max ( 0.5f, shape.outline_t_ * std::min ( scale.x, scale.y ) );
        DrawRing( { center_screen.x, center_screen.y },
                  std::max ( 0.f, radius - outline ),
                  radius,
                  0.f, 360.f, std::max( 24, shape.point_count_ ), shape.outline_.to_rl() );
    }
}

inline void Window::draw( const ConvexShape& shape )
{
    if ( shape.points_.size() < 3 )
    {
        return;
    }

    const float rad = shape.rotation_ * 3.14159265358979323846f / 180.f;
    const float cs = std::cos( rad );
    const float sn = std::sin( rad );
    const auto to_world = [&]( Vec2f p )
    {
        const Vec2f shifted { p.x - shape.origin_.x, p.y - shape.origin_.y };
        const Vec2f rotated { shifted.x * cs - shifted.y * sn,
                              shifted.x * sn + shifted.y * cs };
        const Vec2f world { shape.pos_.x + rotated.x, shape.pos_.y + rotated.y };
        const Vec2f screen = world_to_screen ( *this, current_view_, world );
        return Vector2 { screen.x, screen.y };
    };

    for ( std::size_t i = 1; i + 1 < shape.points_.size(); ++i )
    {
        const Vector2 a = to_world( shape.points_[0] );
        const Vector2 b = to_world( shape.points_[i] );
        const Vector2 c = to_world( shape.points_[i + 1] );
        DrawTriangle( a, b, c, shape.fill_.to_rl() );
    }
}

inline void Window::draw( const Sprite& sprite )
{
    if ( sprite.tex_ == nullptr || !sprite.tex_->loaded )
    {
        return;
    }

    const float src_w = static_cast<float> ( sprite.tex_->rl.width );
    const float src_h = static_cast<float> ( sprite.tex_->rl.height );
    Rectangle src { 0.f, 0.f, src_w, src_h };
    const Vec2f pos = world_to_screen ( *this, current_view_, sprite.pos_ );
    const Vec2f scale = world_scale_to_screen ( *this, current_view_, { 1.f, 1.f } );
    Rectangle dst {
        pos.x,
        pos.y,
        src_w * sprite.scale_.x * scale.x,
        src_h * sprite.scale_.y * scale.y
    };
    DrawTexturePro( sprite.tex_->rl, src, dst,
                    { sprite.origin_.x * sprite.scale_.x * scale.x,
                      sprite.origin_.y * sprite.scale_.y * scale.y },
                    sprite.rotation_, sprite.color_.to_rl() );
}

inline void Window::draw( const Text& text )
{
    if ( text.font_ == nullptr || !text.font_->loaded )
    {
        return;
    }

    const Vec2f base_world {
        text.position_.x - text.origin_.x,
        text.position_.y - text.origin_.y
    };
    const Vec2f pos_screen = world_to_screen ( *this, current_view_, base_world );
    const Vec2f scale = world_scale_to_screen ( *this, current_view_, { 1.f, 1.f } );
    const float font_size = std::max ( 4.f, static_cast<float> ( text.char_size_ ) * scale.y );
    const float spacing = font_size / 10.f;  // Raylib recommended: fontSize/10
    const Vector2 size = MeasureTextEx( text.font_->rl, text.string_.c_str(), font_size, spacing );
    const Vector2 pos { pos_screen.x, pos_screen.y };

    if ( text.outline_thickness_ > 0.f && text.outline_color_.a > 0 )
    {
        const float d = std::max ( 1.f, text.outline_thickness_ * std::min ( scale.x, scale.y ) );
        DrawTextEx( text.font_->rl, text.string_.c_str(), { pos.x - d, pos.y }, font_size, spacing,
                    text.outline_color_.to_rl() );
        DrawTextEx( text.font_->rl, text.string_.c_str(), { pos.x + d, pos.y }, font_size, spacing,
                    text.outline_color_.to_rl() );
        DrawTextEx( text.font_->rl, text.string_.c_str(), { pos.x, pos.y - d }, font_size, spacing,
                    text.outline_color_.to_rl() );
        DrawTextEx( text.font_->rl, text.string_.c_str(), { pos.x, pos.y + d }, font_size, spacing,
                    text.outline_color_.to_rl() );
    }

    DrawTextEx( text.font_->rl, text.string_.c_str(), pos, font_size, spacing, text.fill_color_.to_rl() );
}

inline void Window::draw( const Vertex* vertices, std::size_t count, sf::PrimitiveType type )
{
    if ( vertices == nullptr || count == 0 )
    {
        return;
    }

    const Vec2f scale = world_scale_to_screen ( *this, current_view_, { 1.f, 1.f } );
    const float line_thickness = std::max ( 1.f, std::min ( scale.x, scale.y ) );
    auto tx = [&]( Vec2f p ) -> Vector2
    {
        const Vec2f s = world_to_screen ( *this, current_view_, p );
        return { s.x, s.y };
    };

    if ( type == sf::PrimitiveType::Points )
    {
        for ( std::size_t i = 0; i < count; ++i )
        {
            DrawPixelV( tx ( vertices[i].position ),
                        vertices[i].color.to_rl() );
        }
        return;
    }

    if ( type == sf::PrimitiveType::Lines )
    {
        for ( std::size_t i = 0; i + 1 < count; i += 2 )
        {
            DrawLineEx( tx ( vertices[i].position ),
                        tx ( vertices[i + 1].position ),
                        line_thickness, vertices[i].color.to_rl() );
        }
        return;
    }

    if ( type == sf::PrimitiveType::LineStrip )
    {
        for ( std::size_t i = 0; i + 1 < count; ++i )
        {
            DrawLineEx( tx ( vertices[i].position ),
                        tx ( vertices[i + 1].position ),
                        line_thickness, vertices[i].color.to_rl() );
        }
        return;
    }

    if ( type == sf::PrimitiveType::Triangles )
    {
        for ( std::size_t i = 0; i + 2 < count; i += 3 )
        {
            Vertex a = vertices[i];
            Vertex b = vertices[i + 1];
            Vertex c = vertices[i + 2];
            a.position = { tx ( a.position ).x, tx ( a.position ).y };
            b.position = { tx ( b.position ).x, tx ( b.position ).y };
            c.position = { tx ( c.position ).x, tx ( c.position ).y };
            draw_colored_triangle( a, b, c );
        }
        return;
    }

    if ( type == sf::PrimitiveType::TriangleStrip )
    {
        for ( std::size_t i = 0; i + 2 < count; ++i )
        {
            Vertex a = vertices[( i & 1u ) == 0u ? i : i + 1];
            Vertex b = vertices[( i & 1u ) == 0u ? i + 1 : i];
            Vertex c = vertices[i + 2];
            a.position = { tx ( a.position ).x, tx ( a.position ).y };
            b.position = { tx ( b.position ).x, tx ( b.position ).y };
            c.position = { tx ( c.position ).x, tx ( c.position ).y };
            if ( ( i & 1u ) == 0u )
            {
                draw_colored_triangle( a, b, c );
            }
            else
            {
                draw_colored_triangle( a, b, c );
            }
        }
        return;
    }

    if ( type == sf::PrimitiveType::TriangleFan )
    {
        for ( std::size_t i = 1; i + 1 < count; ++i )
        {
            Vertex a = vertices[0];
            Vertex b = vertices[i];
            Vertex c = vertices[i + 1];
            a.position = { tx ( a.position ).x, tx ( a.position ).y };
            b.position = { tx ( b.position ).x, tx ( b.position ).y };
            c.position = { tx ( c.position ).x, tx ( c.position ).y };
            draw_colored_triangle( a, b, c );
        }
    }
}

inline void Window::draw( const VertexArray& vertices )
{
    draw( vertices.verts.data(), vertices.verts.size(),
          static_cast<sf::PrimitiveType> ( vertices.prim_type ) );
}

inline std::vector<Event> poll_events( Window& w )
{
    std::vector<Event> events;

    if ( WindowShouldClose() )
    {
        events.push_back( ClosedEvent {} );
    }

    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    if ( width != w.w_ || height != w.h_ )
    {
        w.w_ = width;
        w.h_ = height;
        w.default_view_.reset ( { 0.f, 0.f, static_cast<float> ( width ), static_cast<float> ( height ) } );
        w.default_view_.setViewport ( { 0.f, 0.f, 1.f, 1.f } );
        w.current_view_ = w.default_view_;
        events.push_back( ResizedEvent { static_cast<unsigned> ( width ),
                                         static_cast<unsigned> ( height ) } );
    }

    const bool focused = IsWindowFocused();
    if ( focused && !w.was_focused_ )
    {
        events.push_back( FocusEvent {} );
    }
    w.was_focused_ = focused;

    while ( const int key = GetKeyPressed() )
    {
        KeyEvent ev;
        ev.key = key;
        ev.code = key;
        ev.pressed = true;
        ev.alt = IsKeyDown( KEY_LEFT_ALT ) || IsKeyDown( KEY_RIGHT_ALT );
        ev.shift = IsKeyDown( KEY_LEFT_SHIFT ) || IsKeyDown( KEY_RIGHT_SHIFT );
        ev.ctrl = IsKeyDown( KEY_LEFT_CONTROL ) || IsKeyDown( KEY_RIGHT_CONTROL );
        events.push_back( ev );
    }

    while ( const int ch = GetCharPressed() )
    {
        events.push_back( TextEvent { static_cast<uint32_t> ( ch ) } );
    }

    const Vector2 mouse = GetMousePosition();
    if ( !w.mouse_initialized_ || mouse.x != w.last_mouse_x_ || mouse.y != w.last_mouse_y_ )
    {
        w.last_mouse_x_ = mouse.x;
        w.last_mouse_y_ = mouse.y;
        w.mouse_initialized_ = true;
        events.push_back( MouseMoveEvent { mouse.x, mouse.y } );
    }

    const auto push_mouse_btn = [&]( int ray_btn, int mapped_btn )
    {
        if ( IsMouseButtonPressed( ray_btn ) )
        {
            events.push_back( MouseBtnEvent { mouse.x, mouse.y, mapped_btn, true } );
        }
        if ( IsMouseButtonReleased( ray_btn ) )
        {
            events.push_back( MouseBtnEvent { mouse.x, mouse.y, mapped_btn, false } );
        }
    };
    push_mouse_btn( MOUSE_BUTTON_LEFT, 0 );
    push_mouse_btn( MOUSE_BUTTON_RIGHT, 1 );
    push_mouse_btn( MOUSE_BUTTON_MIDDLE, 2 );

    const float wheel = GetMouseWheelMove();
    if ( wheel != 0.f )
    {
        events.push_back( MouseWheelEvent { wheel, mouse.x, mouse.y } );
    }

    return events;
}

inline std::vector<Event> Window::pollEvents()
{
    return poll_events( *this );
}

}  // namespace platform
