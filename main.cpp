// ADAPTED FROM JIM MCCANN'S BASE1 CODE FOR 15-466 COMPUTER GAME PROGRAMMING

#include "load_save_png.hpp"
#include "GL.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <chrono>
#include <iostream>
#include <stdexcept>

const float PI = 3.1415f;
static GLuint compile_shader(GLenum type, std::string const &source);
static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader);

int main(int argc, char **argv) {
	//Configuration:
	struct {
		std::string title = "Game1: Text/Tiles";
		glm::uvec2 size = glm::uvec2(1200, 700);
	} config;

	//------------  initialization ------------

	//Initialize SDL library:
	SDL_Init(SDL_INIT_VIDEO);

	//Ask for an OpenGL context version 3.3, core profile, enable debug:
	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	//create window:
	SDL_Window *window = SDL_CreateWindow(
		config.title.c_str(),
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		config.size.x, config.size.y,
		SDL_WINDOW_OPENGL /*| SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI*/
	);

	if (!window) {
		std::cerr << "Error creating SDL window: " << SDL_GetError() << std::endl;
		return 1;
	}

	//Create OpenGL context:
	SDL_GLContext context = SDL_GL_CreateContext(window);

	if (!context) {
		SDL_DestroyWindow(window);
		std::cerr << "Error creating OpenGL context: " << SDL_GetError() << std::endl;
		return 1;
	}

	#ifdef _WIN32
	//On windows, load OpenGL extensions:
	if (!init_gl_shims()) {
		std::cerr << "ERROR: failed to initialize shims." << std::endl;
		return 1;
	}
	#endif

	//Set VSYNC + Late Swap (prevents crazy FPS):
	if (SDL_GL_SetSwapInterval(-1) != 0) {
		std::cerr << "NOTE: couldn't set vsync + late swap tearing (" << SDL_GetError() << ")." << std::endl;
		if (SDL_GL_SetSwapInterval(1) != 0) {
			std::cerr << "NOTE: couldn't set vsync (" << SDL_GetError() << ")." << std::endl;
		}
	}

	//Hide mouse cursor (note: showing can be useful for debugging):
	SDL_ShowCursor(SDL_DISABLE);

	//------------ opengl objects / game assets ------------

	//texture:
	GLuint tex = 0;
	glm::uvec2 tex_size = glm::uvec2(0,0);

	{ //load texture 'tex':
		std::vector< uint32_t > data;
		if (!load_png("elements.png", &tex_size.x, &tex_size.y, &data, LowerLeftOrigin)) {
			std::cerr << "Failed to load texture." << std::endl;
			exit(1);
		}
		//create a texture object:
		glGenTextures(1, &tex);
		//bind texture object to GL_TEXTURE_2D:
		glBindTexture(GL_TEXTURE_2D, tex);
		//upload texture data from data:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_size.x, tex_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data[0]);
		//set texture sampling parameters:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	//shader program:
	GLuint program = 0;
	GLuint program_Position = 0;
	GLuint program_TexCoord = 0;
	GLuint program_Color = 0;
	GLuint program_mvp = 0;
	GLuint program_tex = 0;
	{ //compile shader program:
		GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER,
			"#version 330\n"
			"uniform mat4 mvp;\n"
			"in vec4 Position;\n"
			"in vec2 TexCoord;\n"
			"in vec4 Color;\n"
			"out vec2 texCoord;\n"
			"out vec4 color;\n"
			"void main() {\n"
			"	gl_Position = mvp * Position;\n"
			"	color = Color;\n"
			"	texCoord = TexCoord;\n"
			"}\n"
		);

		GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER,
			"#version 330\n"
			"uniform sampler2D tex;\n"
			"in vec4 color;\n"
			"in vec2 texCoord;\n"
			"out vec4 fragColor;\n"
			"void main() {\n"
			"	fragColor = texture(tex, texCoord) * color;\n"
			"}\n"
		);

		program = link_program(fragment_shader, vertex_shader);

		//look up attribute locations:
		program_Position = glGetAttribLocation(program, "Position");
		if (program_Position == -1U) throw std::runtime_error("no attribute named Position");
		program_TexCoord = glGetAttribLocation(program, "TexCoord");
		if (program_TexCoord == -1U) throw std::runtime_error("no attribute named TexCoord");
		program_Color = glGetAttribLocation(program, "Color");
		if (program_Color == -1U) throw std::runtime_error("no attribute named Color");

		//look up uniform locations:
		program_mvp = glGetUniformLocation(program, "mvp");
		if (program_mvp == -1U) throw std::runtime_error("no uniform named mvp");
		program_tex = glGetUniformLocation(program, "tex");
		if (program_tex == -1U) throw std::runtime_error("no uniform named tex");
	}

	//vertex buffer:
	GLuint buffer = 0;
	{ //create vertex buffer
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
	}

	struct Vertex {
		Vertex(glm::vec2 const &Position_, glm::vec2 const &TexCoord_, glm::u8vec4 const &Color_) :
			Position(Position_), TexCoord(TexCoord_), Color(Color_) { }
		glm::vec2 Position;
		glm::vec2 TexCoord;
		glm::u8vec4 Color;
	};
	static_assert(sizeof(Vertex) == 20, "Vertex is nicely packed.");

	//vertex array object:
	GLuint vao = 0;
	{ //create vao and set up binding:
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glVertexAttribPointer(program_Position, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0);
		glVertexAttribPointer(program_TexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0 + sizeof(glm::vec2));
		glVertexAttribPointer(program_Color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (GLbyte *)0 + sizeof(glm::vec2) + sizeof(glm::vec2));
		glEnableVertexAttribArray(program_Position);
		glEnableVertexAttribArray(program_TexCoord);
		glEnableVertexAttribArray(program_Color);
	}

	//------------ structs and variables ------------
  struct {
    glm::vec2 pos = glm::vec2(6.0f, 3.5f);
    glm::vec2 size = glm::vec2(12.0f, 7.0f);
  } camera;
  //adjust for aspect ratio
  camera.size.x = camera.size.y * (float(config.size.x) / float(config.size.y));

  struct SpriteInfo {
		glm::vec2 min_uv;
		glm::vec2 max_uv;
	};

  struct {
    glm::vec2 pos = glm::vec2(0.0f);
    glm::vec2 size = glm::vec2(0.5f);
    
    SpriteInfo sprite_basic = {
      glm::vec2(0.0f),
      glm::vec2(0.2f),
    };
    SpriteInfo sprite_aim_throw = {
      glm::vec2(0.2f),
      glm::vec2(0.4f),
    };
    SpriteInfo sprite_aim_shoot = {
      glm::vec2(0.4f),
      glm::vec2(0.6f),
    };
  } mouse;
  
  struct {
    glm::vec2 pos = glm::vec2(0.25f, 1.0f);
    glm::vec2 size = glm::vec2(0.5f, 1.0f);
    glm::vec2 vel = glm::vec2(0.0f);
    
    SpriteInfo sprite_stand = {
      glm::vec2(0.0f),
      glm::vec2(0.2f),
    };
    SpriteInfo sprite_walk = {
      glm::vec2(0.0f),
      glm::vec2(0.2f),
    };
    SpriteInfo sprite_run = {
      glm::vec2(0.0f),
      glm::vec2(0.2f),
    };
    SpriteInfo sprite_jump = {
      glm::vec2(0.0f),
      glm::vec2(0.2f),
    };
    SpriteInfo sprite_throw = {
      glm::vec2(0.0f),
      glm::vec2(0.2f),
    };
    SpriteInfo sprite_shoot = {
      glm::vec2(0.0f),
      glm::vec2(0.2f),
    };
    
    // 0: throwing
    // 1: shooting
    int ability_mode = 0;

    bool jumping = false;
    bool shifting = false;
    bool behind_door = false;
    bool aiming = false;
    bool visible = false;  

    int num_projectiles = 0;
  } player;

  struct Enemy {
    glm::vec2 pos = glm::vec2(10.0f, 1.0f);
    glm::vec2 vel = glm::vec2(0.0f);
    glm::vec2 size = glm::vec2(0.5, 1.0f);

    SpriteInfo sprite_stand = {
      glm::vec2(0.0f, 0.5f),
      glm::vec2(0.5f, 1.0f),
    };
    SpriteInfo sprite_walk = {
      glm::vec2(0.2f),
      glm::vec2(0.4f),
    };
    SpriteInfo sprite_alert = {
      glm::vec2(0.4f),
      glm::vec2(0.6f),
    };
 
    bool face_right = true;
    bool alerted = false;
    bool walking = false;

    glm::vec2 waypoints [2] = { glm::vec2(10.0f, 1.0f), glm::vec2(4.0f, 1.0f) };
    float wait_timers [2] = { 5.0f, 5.0f };
    int curr_index  = 0;
    float remaining_wait = 5.0f;
  };

  struct Light {
    glm::vec2 pos = glm::vec2(0.0f);
    glm::vec2 size = glm::vec2(1.0f, 3.0f);
    float dir = PI * 1.5f;
    float angle = PI * 0.25f;
    float range = 3.0f;

    SpriteInfo sprite_on = {
      glm::vec2(0.0f),
      glm::vec2(0.2f),
    };
    SpriteInfo sprite_off = {
      glm::vec2(0.2f),
      glm::vec2(0.4f),
    };

    bool light_on = true;
  };

  struct Door {
    glm::vec2 pos = glm::vec2(0.0f);
    glm::vec2 size = glm::vec2(1.0f);

    SpriteInfo sprite_empty = {
      glm::vec2(0.0f),
      glm::vec2(0.2f),
    };
    SpriteInfo sprite_used = {
      glm::vec2(0.0f),
      glm::vec2(0.2f),
    };

    bool in_use = false;
  };

  struct Platform {
    glm::vec2 pos = glm::vec2(15.0f, 0.25f);
    glm::vec2 size = glm::vec2(30.0f, 0.5f);

    SpriteInfo sprite = {
      glm::vec2(0.5f),
      glm::vec2(0.75f),
    };
  };

  Light light;
  Platform platform;
  Enemy enemy;
  Door door;

	//------------ game loop ------------

	bool should_quit = false;
	while (true) {
		static SDL_Event evt;
		while (SDL_PollEvent(&evt) == 1) {
			//handle input:
			if (evt.type == SDL_MOUSEMOTION) {
				mouse.pos.x = (evt.motion.x + 0.5f) / float(config.size.x) * 2.0f - 1.0f;
				mouse.pos.y = (evt.motion.y + 0.5f) / float(config.size.y) *-2.0f + 1.0f;
			} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
			} else if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_ESCAPE) {
				should_quit = true;
			} else if (evt.type == SDL_KEYDOWN || evt.type == SDL_KEYUP) {
        if (evt.key.keysym.sym == SDLK_w) {
          if (!player.jumping && evt.key.state == SDL_PRESSED) {
            player.jumping = true;
            player.vel.y = 6.0f;
          }
        } else if (evt.key.keysym.sym == SDLK_a) {
          if (evt.key.state == SDL_PRESSED) {
            if (player.shifting) {
              player.vel.x = -2.5f;
            } else {
              player.vel.x = -1.0f;
            }
          } else {
            if (player.vel.x == -1.0f || player.vel.x == -2.5f) {
              player.vel.x = 0.0f;
            }
          }
        } else if (evt.key.keysym.sym == SDLK_d) {
          if (evt.key.state == SDL_PRESSED) {
            if (player.shifting) {
              player.vel.x = 2.5f;
            } else {
              player.vel.x = 1.0f;
            }
          } else {
            if (player.vel.x == 1.0f || player.vel.x == 2.5f) {
              player.vel.x = 0.0f;
            }
          }
        } else if (evt.key.keysym.sym == SDLK_q) {
          if (evt.key.state == SDL_PRESSED) {
            player.ability_mode = 0;
          }
        } else if (evt.key.keysym.sym == SDLK_e) {
          if (evt.key.state == SDL_PRESSED) {
            player.ability_mode = 1;
          }
        } else if (evt.key.keysym.sym == SDLK_LSHIFT) {
          if (evt.key.state == SDL_PRESSED) {
            if (player.vel.x == 1.0f) {
              player.vel.x = 2.5f;
            } else if (player.vel.x == -1.0f) {
              player.vel.x = -2.5f;
            }
            player.shifting = true;
          } else {
            if (player.vel.x == 2.5f) {
              player.vel.x = 1.0f;
            } else if (player.vel.x == -2.5f) {
              player.vel.x = -1.0f;
            }
            player.shifting = false;
          }
        } 
      } else if (evt.type == SDL_QUIT) {
				should_quit = true;
				break;
			}
		}
		if (should_quit) break;

		auto current_time = std::chrono::high_resolution_clock::now();
		static auto previous_time = current_time;
		float elapsed = std::chrono::duration< float >(current_time - previous_time).count();
		previous_time = current_time;

		{ //update game state:
      
      // player update
      if (player.jumping) {
        player.vel.y -= elapsed * 9.0f;
      }

      player.pos += player.vel * elapsed;
      if (player.pos.x < 0.25f) {
        player.pos.x = 0.25f;
      } else if (player.pos.x > 29.75f) {
        player.pos.x = 29.75f;
      }
      if (player.pos.y < 1.0f) {
        player.jumping = false;
        player.pos.y = 1.0f;
        player.vel.y = 0.0f;
      }

      //camera update
      camera.pos.x += player.vel.x * elapsed;
      if (player.pos.x < 6.0f) {
        camera.pos.x = 6.0f;
      } else if (player.pos.x > 24.0f) {
        camera.pos.x = 24.0f;
      }

      //enemy update
      if (!enemy.walking) {
        enemy.remaining_wait -= elapsed;
        if (enemy.remaining_wait <= 0.0f) {
          enemy.walking = true;
          enemy.face_right = !enemy.face_right;
          enemy.curr_index = (enemy.curr_index + 1) % 2;
          if (enemy.face_right) {
            enemy.vel.x = 1.0f;
          } else {
            enemy.vel.x = -1.0f;
          }
        }
      } else {
        enemy.pos += enemy.vel * elapsed;
        if ((enemy.face_right && enemy.pos.x > enemy.waypoints[enemy.curr_index].x) ||
            (!enemy.face_right && enemy.pos.x < enemy.waypoints[enemy.curr_index].x)) {
          enemy.pos = enemy.waypoints[enemy.curr_index];
          enemy.remaining_wait = enemy.wait_timers[enemy.curr_index];
          enemy.walking = false;
        }
      }

    }

		//draw output:
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		{ //draw game state:
			std::vector< Vertex > verts;

			auto draw_sprite = [&verts](SpriteInfo const &sprite, glm::vec2 const &at, glm::vec2 size, glm::u8vec4 tint = glm::u8vec4(0xff, 0xff, 0xff, 0xff), float angle = 0.0f) {
				glm::vec2 min_uv = sprite.min_uv;
				glm::vec2 max_uv = sprite.max_uv;
				glm::vec2 right = glm::vec2(std::cos(angle), std::sin(angle));
				glm::vec2 up = glm::vec2(-right.y, right.x);

				verts.emplace_back(at + right * -size.x/2.0f + up * -size.y/2.0f, glm::vec2(min_uv.x, min_uv.y), tint);
				verts.emplace_back(verts.back());
				verts.emplace_back(at + right * -size.x/2.0f + up * size.y/2.0f, glm::vec2(min_uv.x, max_uv.y), tint);
				verts.emplace_back(at + right *  size.x/2.0f + up * -size.y/2.0f, glm::vec2(max_uv.x, min_uv.y), tint);
				verts.emplace_back(at + right *  size.x/2.0f + up *  size.y/2.0f, glm::vec2(max_uv.x, max_uv.y), tint);
				verts.emplace_back(verts.back());
			};

			draw_sprite(player.sprite_stand, player.pos, player.size);
			draw_sprite(enemy.sprite_stand, enemy.pos, enemy.size);
			draw_sprite(platform.sprite, platform.pos, platform.size);

			glBindBuffer(GL_ARRAY_BUFFER, buffer);
			glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * verts.size(), &verts[0], GL_STREAM_DRAW);

			glUseProgram(program);
			glUniform1i(program_tex, 0);
			glm::vec2 scale = 2.0f / camera.size;
			glm::vec2 offset = scale * -camera.pos;
			glm::mat4 mvp = glm::mat4(
				glm::vec4(scale.x, 0.0f, 0.0f, 0.0f),
				glm::vec4(0.0f, scale.y, 0.0f, 0.0f),
				glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
				glm::vec4(offset.x, offset.y, 0.0f, 1.0f)
			);
			glUniformMatrix4fv(program_mvp, 1, GL_FALSE, glm::value_ptr(mvp));

			glBindTexture(GL_TEXTURE_2D, tex);
			glBindVertexArray(vao);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, verts.size());
		}

		SDL_GL_SwapWindow(window);
	}


	//------------  teardown ------------

	SDL_GL_DeleteContext(context);
	context = 0;

	SDL_DestroyWindow(window);
	window = NULL;

	return 0;
}



static GLuint compile_shader(GLenum type, std::string const &source) {
	GLuint shader = glCreateShader(type);
	GLchar const *str = source.c_str();
	GLint length = source.size();
	glShaderSource(shader, 1, &str, &length);
	glCompileShader(shader);
	GLint compile_status = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
	if (compile_status != GL_TRUE) {
		std::cerr << "Failed to compile shader." << std::endl;
		GLint info_log_length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector< GLchar > info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetShaderInfoLog(shader, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		glDeleteShader(shader);
		throw std::runtime_error("Failed to compile shader.");
	}
	return shader;
}

static GLuint link_program(GLuint fragment_shader, GLuint vertex_shader) {
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);
	GLint link_status = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &link_status);
	if (link_status != GL_TRUE) {
		std::cerr << "Failed to link shader program." << std::endl;
		GLint info_log_length = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector< GLchar > info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetProgramInfoLog(program, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		throw std::runtime_error("Failed to link program");
	}
	return program;
}
