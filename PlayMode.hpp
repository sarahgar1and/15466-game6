#include "Mode.hpp"

#include "Scene.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----
	struct FixedCharge {
		glm::vec3 pos;
		double q; 
	};
	std::vector<FixedCharge> charges;
	const double k_e = 8.99e9; // Coulomb's constant
	double q = 1.602e-19; // Player charge
	double m = 1.673e-27; // Player mass
	glm::vec3 prev_pos;
	glm::vec3 prev_prev_pos;
	float dt = 1.0f / 60.0f; // Verlet integration time step 
	float time_acc = 0.0f;
	glm::vec3 get_acceleration(glm::vec3 pos);
	glm::vec3 get_pos();

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//hexapod leg to wobble:
	Scene::Transform *player = nullptr;
	Scene::Transform *fixed_charge = nullptr;
	Scene::Transform *fixed_charge1 = nullptr;
	
	//camera:
	Scene::Camera *camera = nullptr;

};
