#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <vector>

glm::vec3 PlayMode::get_acceleration(glm::vec3 pos){
	glm::vec3 F = glm::vec3(0.0f);

	for (auto & charge : charges){
		glm::vec3 diff = pos - charge.pos;
		float dist_sq = glm::dot(diff, diff);
        float inv_dist_cube = 1.0f / (dist_sq * std::sqrt(dist_sq));
		diff *= (charge.q * inv_dist_cube);
		F += diff;
	}

	F *= (k_e * q / m);

	return F;
}

glm::vec3 PlayMode::get_pos(){
	return 2.0f * prev_pos - prev_prev_pos + get_acceleration(prev_pos) * std::pow(dt, 2.0f);
}

PlayMode::PlayMode() {
	player_pos = glm::vec3(-1.0f, 0.0f, 0.0f);

	// Initialize verlet integration vars
	prev_pos = player_pos;
	prev_prev_pos = player_pos + 0.5f * get_acceleration(player_pos) * std::pow(dt, 2.0f);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_SPACE) {
			space.downs += 1;
			space.pressed = true;
			playing = !playing;
			return true;
		} else if (evt.key.key == SDLK_Q) {
			Q.downs += 1;
			Q.pressed = true;
			new_charge_q *= -1.0f;
			return true;
		} else if (evt.key.key == SDLK_R) {
			R.downs += 1;
			R.pressed = true;
			// RESET
			playing = false;
			player_pos = glm::vec3(-1.0f, 0.0f, 0.0f);
			prev_pos = player_pos;
			prev_prev_pos = player_pos + 0.5f * get_acceleration(player_pos) * std::pow(dt, 2.0f);
			charges.clear();
			return true;
		} 
	} else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		float mouseX = evt.button.x;
		float mouseY = evt.button.y;
		// Add a charge at position
		float x = (2.0f * mouseX / window_size.x) - 1.0f;
		float y = 1.0f - (2.0f * mouseY / window_size.y);
		float aspect = float(window_size.x) / float(window_size.y);
		if (evt.button.button == SDL_BUTTON_LEFT){
			FixedCharge fc;
			fc.pos = glm::vec3(x * aspect, y, 0.0f);
			fc.q = new_charge_q;
			// std::cout << "(" << fc.pos.x << ","  << fc.pos.y << "," << fc.pos.z << ")" << std::endl;
			charges.emplace_back(fc);
			return true;
		} else if (evt.button.button == SDL_BUTTON_RIGHT){
			for (auto it = charges.begin(); it != charges.end();) {
				if (std::pow(it->pos.x - x * aspect, 2.0f) + std::pow(it->pos.y - y, 2.0f) 
					<= ChargeRadius * ChargeRadius) {
					it = charges.erase(it); // erase() returns the iterator to the next element
				} else {
					++it; 
				}
    		}
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	if (!playing){
		return;
	} 
	// Check if charges have collided -> prevent r = 0
	for (auto &fc : charges){
		glm::vec3 diff = fc.pos - player_pos;
		float dist_sq = glm::dot(diff, diff);
		if (std::sqrt(dist_sq) <= ChargeRadius) return;
	}


	// Update player position
	time_acc += elapsed;
	while (time_acc >= dt){
		player_pos = get_pos();

		// Handle wall collision
		for (auto& wall : walls) {
			glm::vec3 A = glm::vec3 (wall.x, wall.y, 0.0f);
			glm::vec3 B = glm::vec3(wall.z, wall.w, 0.0f);

			glm::vec3 ab = B - A;
			glm::vec3 ap = player_pos - A;
			// Project onto wall
			float t = glm::dot(ap, ab) / glm::dot(ab, ab);
			t = glm::clamp(t, 0.0f, 1.0f);

			glm::vec3 closest_point = A + t * ab;

			glm::vec3 to_particle = player_pos - closest_point;
			float distance = glm::length(to_particle);

			// Prevent division by zero
			if (distance == 0.0f) {
				to_particle = glm::vec3(-ab.y, ab.x, 0.0f); 
				distance = 0.001f;
			}

			if (distance < ChargeRadius) {
				// Calculate the collision surface normal
				glm::vec3 normal = to_particle / distance;
				// Get implicit velocity
				glm::vec3 velocity = player_pos - prev_pos;
				float depth = ChargeRadius - distance;
				player_pos += normal * depth;
				// Flip velocity 
				float normal_vel = glm::dot(velocity, normal);

				// Only bounce if moving toward the wall
				if (normal_vel < 0.0f) {
					glm::vec3 reflected_velocity = velocity - (2.0f) * normal_vel * normal;     
					// New implicit velocity
					prev_pos = player_pos - reflected_velocity;
				}
			}
		}

		prev_prev_pos = prev_pos;
		prev_pos = player_pos;
		time_acc -= dt;
	}
	// std::cout << "(" << player_pos.x << ","  << player_pos.y << "," << player_pos.z << ")" << std::endl;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	// From base5
	static std::array< glm::vec2, 16 > const circle = [](){
		std::array< glm::vec2, 16 > ret;
		for (uint32_t a = 0; a < ret.size(); ++a) {
			float ang = a / float(ret.size()) * 2.0f * float(M_PI);
			ret[a] = glm::vec2(std::cos(ang), std::sin(ang));
		}
		return ret;
	}();

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);

	float aspect = float(drawable_size.x) / float(drawable_size.y);
	DrawLines lines(glm::mat4(
		1.0f / aspect, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	));

	// Draw Player
	glm::u8vec4 col;
	if (q > 0) col = glm::u8vec4(0xff, 0x00, 0x00, 0xff);
	else col = glm::u8vec4(0x00, 0x00, 0xff, 0xff);
	for (uint32_t a = 0; a < circle.size(); ++a) {
		lines.draw(
			player_pos + glm::vec3(ChargeRadius * circle[a], 0.0f),
			player_pos + glm::vec3(ChargeRadius * circle[(a + 1) % circle.size()], 0.0f),
			col
		);
	}
	float H = 0.1f;
	if (q > 0){
		lines.draw_text("+",
		player_pos - glm::vec3(ChargeRadius * 0.25f, ChargeRadius * 0.5f, 0.0f),
		glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
		col);

	} else {
		lines.draw_text("-",
		player_pos - glm::vec3(ChargeRadius * 0.25f, ChargeRadius * 0.5f, 0.0f),
		glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
		col);
	}
	// Draw fixed charges
	for (auto const &fc : charges) {
		glm::u8vec4 col;
		if (fc.q > 0) col = glm::u8vec4(0xff, 0x00, 0x00, 0xff);
		else col = glm::u8vec4(0x00, 0x00, 0xff, 0xff);

		for (uint32_t a = 0; a < circle.size(); ++a) {
			lines.draw(
				fc.pos + glm::vec3(ChargeRadius * circle[a], 0.0f),
				fc.pos + glm::vec3(ChargeRadius * circle[(a + 1) % circle.size()], 0.0f),
				col
			);
		}
		if (fc.q > 0){
			lines.draw_text("+",
			fc.pos - glm::vec3(ChargeRadius * 0.25f, ChargeRadius * 0.5f, 0.0f),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			col);

		} else {
			lines.draw_text("-",
			fc.pos - glm::vec3(ChargeRadius * 0.25f, ChargeRadius * 0.5f, 0.0f),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			col);
		}
		
	}

	// Draw walls
	for (auto &wall : walls){
		lines.draw(glm::vec3(wall[0], wall[1], 0.0f), glm::vec3(wall[2], wall[3], 0.0f), 
				glm::u8vec4(0x00, 0x00, 0x00, 0xff));
	}

}
