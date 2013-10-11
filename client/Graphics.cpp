//////////////////////////////////////////////////////////////////////////////
// NoLifeClient - Part of the NoLifeStory project                           //
// Copyright (C) 2013 Peter Atashian                                        //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "NoLifeClient.hpp"
namespace NL {
    namespace Graphics {
        sf::Window * Window;
        string Title = "NoLifeStory";
        const sf::ContextSettings Context(0, 0, 0, 1, 5);
        GLuint VBO;
        void Create(bool fullscreen) {
            Config::Fullscreen = fullscreen;
            if (Config::Fullscreen) Window->create(sf::VideoMode(Config::FullscreenWidth, Config::FullscreenHeight, 32), Title, sf::Style::Default | sf::Style::Fullscreen, Context);
            else Window->create(sf::VideoMode(Config::WindowWidth, Config::WindowHeight, 32), Title, sf::Style::Titlebar | sf::Style::Resize, Context);
            View::Resize(Window->getSize().x, Window->getSize().y);
            if (Config::Vsync) Window->setVerticalSyncEnabled(true);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_TEXTURE_2D);
            glColor4f(1, 1, 1, 1);
            glClearColor(0, 0, 0, 0);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glEnableClientState(GL_VERTEX_ARRAY);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glVertexPointer(2, GL_FLOAT, 0, nullptr);
            glTexCoordPointer(2, GL_FLOAT, 0, nullptr);
        }
        void Init() {
            Window = new sf::Window();
            GLenum err = glewInit();
            switch (err) {
            case GLEW_OK:
                break;
            case GLEW_ERROR_NO_GL_VERSION:
                throw "You need OpenGL";
            case GLEW_ERROR_GL_VERSION_10_ONLY:
                throw "You need something newer than OpenGL 1.0";
            case GLEW_ERROR_GLX_VERSION_11_ONLY:
                throw "You need something newer than GLX 1.1";
            default:
                throw "ERROR: Unknown GLEW error code " + to_string(err);
            }
            if (!GLEW_ARB_texture_non_power_of_two || !GLEW_VERSION_1_5) {
                throw "Your OpenGL is out of date. Please update your drivers and/or buy a new GPU";
            }
            float a[] = {0, 0, 1, 0, 1, 1, 0, 1};
            glGenBuffers(1, &VBO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(a), a, GL_STATIC_DRAW);
            Create(Config::Fullscreen);
        }
        void Update() {
            if (!Config::Fullscreen) {
                //Meant to slow down title updates because apparently this causes horrible performance on X
                static steady_clock::time_point lasttitle = steady_clock::now();
                if (steady_clock::now() - lasttitle > milliseconds {250}) {
                    lasttitle = steady_clock::now();
                    Window->setTitle(Title + " drawing map " + Map::Current.Name().substr(0, 9) + " at " + to_string(Time::FPS) + " FPS");
                }
            }
            Window->display();
            GLenum err = glGetError();
            switch (err) {
            case GL_NO_ERROR:
                break;
            case GL_INVALID_ENUM:
                throw "Invalid enum";
            case GL_INVALID_VALUE:
                throw "Invalid value";
            case GL_INVALID_OPERATION:
                throw "Invalid operation";
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                throw "Invalid framebuffer operation";
            case GL_OUT_OF_MEMORY:
                throw "Out of memory";
            default:
                throw "Unknown OpenGL error code " + to_string(err);
            }
            sf::Event e;
            while (Window->pollEvent(e)) switch (e.type) {
            case sf::Event::Closed:
                Game::Over = true;
                break;
            case sf::Event::GainedFocus: break;
            case sf::Event::LostFocus: break;
            case sf::Event::JoystickButtonPressed: break;
            case sf::Event::JoystickButtonReleased: break;
            case sf::Event::JoystickConnected: break;
            case sf::Event::JoystickDisconnected: break;
            case sf::Event::JoystickMoved: break;
            case sf::Event::KeyPressed:
                switch (e.key.code) {
                case sf::Keyboard::M:
                    BGM.setVolume(BGM.getVolume() > 0 ? 0 : 100);
                    break;
                case sf::Keyboard::R:
                    Config::Rave = !Config::Rave;
                    BGM.PlayMusic();
                    break;
                case sf::Keyboard::F11:
                    Create(!Config::Fullscreen);
                    break;
                case sf::Keyboard::Return:
                    Map::Random();
                    break;
                case sf::Keyboard::Escape:
                    Game::Over = true;
                    break;
                case sf::Keyboard::LAlt:
                    Player::Pos.Jump();
                    break;
                case sf::Keyboard::Up:
                    Player::Pos.up = true;
                    break;
                case sf::Keyboard::Down:
                    Player::Pos.down = true;
                    break;
                case sf::Keyboard::Left:
                    Player::Pos.left = true;
                    break;
                case sf::Keyboard::Right:
                    Player::Pos.right = true;
                    break;
                }
                break;
            case sf::Event::KeyReleased: break;
            case sf::Event::MouseButtonPressed: break;
            case sf::Event::MouseButtonReleased: break;
            case sf::Event::MouseEntered: break;
            case sf::Event::MouseLeft: break;
            case sf::Event::MouseMoved: break;
            case sf::Event::MouseWheelMoved: break;
            case sf::Event::Resized:
                View::Resize(e.size.width, e.size.height);
                break;
            case sf::Event::TextEntered: break;
            }
            glClear(GL_COLOR_BUFFER_BIT);
        }
        void Unload() {
            Window->close();
            delete Window;
        }
        void DrawRect(int32_t x1, int32_t y1, int32_t x2, int32_t y2, bool view) {
            Sprite::Unbind();
            if (view) glTranslated(View::Width / 2 - View::X, View::Height / 2 - View::Y, 0);
            glTranslated(x1, y1, 0);
            glScaled(x2 - x1, y2 - y1, 1);
            glDrawArrays(GL_QUADS, 0, 4);
            glLoadIdentity();
        }
    }
}