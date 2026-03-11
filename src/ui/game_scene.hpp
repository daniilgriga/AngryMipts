#pragma once
#include "data/level_loader.hpp"
#include "data/score_saver.hpp"
#include "physics/physics_engine.hpp"
#include "render/particles.hpp"
#include "render/renderer.hpp"
#include "render/sfx_player.hpp"
#include "scene.hpp"
#include "shared/level_data.hpp"
#include "shared/thread_safe_queue.hpp"
#include "shared/world_snapshot.hpp"
#include "ui/result_scene.hpp"
#include "ui/slingshot.hpp"

#include <random>
#include <vector>

namespace angry
{

class GameScene : public Scene
{
private:
    struct DropperPayloadGhost
    {
        sf::Vector2f position;
        sf::Vector2f velocity;
        float age = 0.f;
        float lifetime = 0.62f;
        float radius = 9.f;
    };

    struct InflaterExpandRing
    {
        sf::Vector2f position;
        float age = 0.f;
        float lifetime = 0.55f;  // total expand duration
        float maxRadius = 80.f;  // final radius of the ring
    };

    struct BubbleFloat
    {
        sf::Vector2f position;
        float age = 0.f;
        float lifetime = 1.4f;
        float radius = 0.f;
    };

    // Tracks active Bubbler capture zone so we can overlay bubbles on lifted objects
    struct BubbleCaptureZone
    {
        sf::Vector2f center;
        float captureRadius = 140.f;  // matches physics capture radius
        float age = 0.f;
        float lifetime = 1.1f;        // matches physics bubble duration
    };

    Renderer renderer_;
    SfxPlayer sfx_;
    Slingshot slingshot_;
    ParticleSystem particles_;
    PhysicsEngine physics_;
    ThreadSafeQueue<Command> command_queue_;
    LevelLoader level_loader_;
    ScoreSaver score_saver_;
    WorldSnapshot snapshot_;
    sf::Font font_;
    sf::Text hud_text_;
    sf::Clock frame_clock_;
    LevelResult last_result_;
    SceneId pending_scene_ = SceneId::None;
    float end_delay_ = 0.f;
    int level_id_ = -1;
    std::string scores_path_;
    sf::View game_view_;
    sf::RenderWindow* window_ptr_ = nullptr;
    sf::RenderTexture world_pass_;
    sf::RenderTexture bloom_extract_pass_;
    sf::RenderTexture bloom_ping_pass_;
    sf::RenderTexture bloom_pong_pass_;
    sf::Shader post_shader_;
    sf::Shader bloom_extract_shader_;
    sf::Shader bloom_blur_shader_;
    bool post_shader_ready_ = false;
    bool bloom_ready_ = false;
    float impact_flash_ = 0.f;
    sf::Clock visual_clock_;
    float shake_time_ = 0.f;
    float shake_strength_ = 0.f;
    std::mt19937 rng_ {std::random_device {} ()};
    std::uniform_real_distribution<float> shake_dist_ {-1.f, 1.f};
    std::vector<DropperPayloadGhost> dropper_payload_ghosts_;
    std::vector<InflaterExpandRing>   inflater_rings_;
    std::vector<BubbleFloat>          bubble_floats_;
    std::vector<BubbleCaptureZone>    bubble_capture_zones_;
    bool render_targets_dirty_ = true;

    static WorldSnapshot make_mock_snapshot();
    void finish_level();
    void process_events();
    void rebuild_render_targets ( sf::Vector2u size );

public:
    explicit GameScene ( const sf::Font& font );

    const LevelResult& get_last_result() const { return last_result_; }

    void load_level ( int level_id, const std::string& scores_path = "" );
    void retry();
    void notify_window_recreated();

    SceneId handle_input ( const sf::Event& event ) override;
    void update() override;
    void render ( sf::RenderWindow& window ) override;

    // Returns and clears pending_scene_ — called by SceneManager::update()
    // so level-end transitions happen without waiting for an input event.
    SceneId poll_pending_scene();

};

}  // namespace angry
