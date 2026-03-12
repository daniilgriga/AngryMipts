#pragma once
#include "data/level_loader.hpp"
#include "data/OnlineScoreClient.hpp"
#include "data/score_saver.hpp"
#include "physics/physics_runtime.hpp"
#include "render/particles.hpp"
#include "render/renderer.hpp"
#include "render/sfx_player.hpp"
#include "scene.hpp"
#include "shared/level_data.hpp"
#include "shared/thread_safe_queue.hpp"
#include "shared/event.hpp"
#include "shared/world_snapshot.hpp"
#include "ui/result_scene.hpp"
#include "ui/slingshot.hpp"

#include <deque>
#include <cstdint>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
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
    PhysicsRuntime physics_;
    ThreadSafeQueue<Command> command_queue_;
    LevelLoader level_loader_;
    OnlineScoreClient online_score_client_;
    ScoreSaver score_saver_;
    WorldSnapshot snapshot_;
    sf::Font font_;
    sf::Text hud_text_;
    sf::Text perf_text_;
    sf::Clock frame_clock_;
    LevelResult last_result_;
    SceneId pending_scene_ = SceneId::None;
    float end_delay_ = 0.f;
    int level_id_ = -1;
    std::string scores_path_;
    std::string player_name_ = "Player";

    struct LeaderboardAsyncState
    {
        std::mutex mutex;
        std::uint64_t ready_token = 0;
        std::vector<LeaderboardEntry> ready_entries;
        bool ready = false;
    };
    std::shared_ptr<LeaderboardAsyncState> leaderboard_async_state_ =
        std::make_shared<LeaderboardAsyncState>();
    std::uint64_t leaderboard_request_token_ = 0;
    std::uint64_t pending_result_token_ = 0;
    bool leaderboard_applied_ = true;
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
    std::deque<Event> pending_events_;
    bool show_perf_overlay_ = true;
    float smoothed_dt_sec_ = 1.0f / 60.0f;
    float smoothed_fps_ = 60.0f;
    float vfx_load_factor_ = 1.0f;
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
    bool poll_result_update();

    SceneId handle_input ( const sf::Event& event ) override;
    void update() override;
    void render ( sf::RenderWindow& window ) override;

    // Returns and clears pending_scene_ — called by SceneManager::update()
    // so level-end transitions happen without waiting for an input event.
    SceneId poll_pending_scene();

};

}  // namespace angry
