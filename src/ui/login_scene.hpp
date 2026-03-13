#pragma once
#include "data/account_service.hpp"
#include "scene.hpp"

#include <string>

namespace angry
{

class LoginScene : public Scene
{
public:
    explicit LoginScene ( const platform::Font& font, AccountService& accounts );

    SceneId handle_input ( const platform::Event& event ) override;
    void    update() override;
    void    render ( platform::Window& window ) override;

private:
    enum class FocusField { Username, Password };
    enum class StatusKind { None, Pending, Error, Success };

    void do_login();
    void do_register();

    AccountService& accounts_;

    platform::Font font_;

    std::string username_buf_;
    std::string password_buf_;
    FocusField  focus_       = FocusField::Username;

    std::string  status_msg_;
    StatusKind   status_kind_ = StatusKind::None;
    platform::Clock status_clock_;   // time since last status set
    platform::Clock caret_clock_;    // drives blinking caret
    platform::Clock anim_clock_;     // background animation

    // Hit-test rects updated each render() call
    platform::FloatRect rect_field_username_;
    platform::FloatRect rect_field_password_;
    platform::FloatRect rect_btn_login_;
    platform::FloatRect rect_btn_register_;
    platform::FloatRect rect_btn_guest_;
};

}  // namespace angry
