#include "controller.h"
#include "game.h"
#include "engine.h"

#if defined(USE_STEAM)
#include "steam_api_flat.h"
#endif

#include <stdio.h>

#define DEF_ACTION_SET(x) InputActionSetHandle_t x##_handle = -1
#define DEF_ANALOG_ACTION(x) InputAnalogActionHandle_t x##_handle = -1
#define DEF_DIGITAL_ACTION(x) class digital_action_state x

#define SET_ACTION_SET(x) x##_handle = cdpi::steam::input->GetActionSetHandle(#x)
#define SET_ANALOG_ACTION(x) x##_handle =cdpi::steam::input->GetAnalogActionHandle(#x)
#define SET_DIGITAL_ACTION(x) x.set_action_handle(cdpi::steam::input->GetDigitalActionHandle(#x))

namespace controller
{
// If we are not using the SIAPI move action, then do not overwrite strafing
// data set by the regular keyboard bindings
bool lastmovementwaskeyboard = true;

// This current controller implementation depends on Steam Input and is not
// available outside of Steam
#if defined(USE_STEAM)

InputHandle_t *controllers = new InputHandle_t[STEAM_INPUT_MAX_COUNT];

class digital_action_state
{
	bool input_last_frame = false;
	bool input_this_frame = false;
	InputDigitalActionHandle_t handle = -1;

public:
	void set_action_handle(InputDigitalActionHandle_t handle)
	{
		this->handle = handle;
	}

	bool get_digital_action_state()
	{
		InputDigitalActionData_t data = cdpi::steam::input->GetDigitalActionData(controllers[0], this->handle);
		return data.bState;
	}

	void update()
	{
		this->input_last_frame = this->input_this_frame;
		this->input_this_frame = this->get_digital_action_state();
	}

	bool pressed()
	{
		return this->input_this_frame;
	}

	bool released()
	{
		return !this->input_this_frame;
	}

	bool just_pressed()
	{
		return this->input_this_frame && !this->input_last_frame;
	}

	bool just_released()
	{
		return !this->input_this_frame && this->input_last_frame;
	}
};

DEF_ACTION_SET(InGameControls);
DEF_ACTION_SET(MenuControls);

DEF_ANALOG_ACTION(move);
DEF_ANALOG_ACTION(camera);

DEF_DIGITAL_ACTION(primary);
DEF_DIGITAL_ACTION(secondary);
DEF_DIGITAL_ACTION(reload);
DEF_DIGITAL_ACTION(use);
DEF_DIGITAL_ACTION(jump);
DEF_DIGITAL_ACTION(walk);
DEF_DIGITAL_ACTION(crouch);
DEF_DIGITAL_ACTION(special);
DEF_DIGITAL_ACTION(drop);
DEF_DIGITAL_ACTION(affinity);
DEF_DIGITAL_ACTION(dash);

DEF_DIGITAL_ACTION(next_weapon);
DEF_DIGITAL_ACTION(previous_weapon);
DEF_DIGITAL_ACTION(primary_weapon);
DEF_DIGITAL_ACTION(secondary_weapon);
DEF_DIGITAL_ACTION(wheel_select);
DEF_DIGITAL_ACTION(change_loadout);

DEF_DIGITAL_ACTION(scoreboard);

DEF_DIGITAL_ACTION(recenter_camera);

bool get_digital_action_state(int siapi_digital_handle)
{
	InputDigitalActionData_t data = cdpi::steam::input->GetDigitalActionData(controllers[0], siapi_digital_handle);
	return data.bState;
}

void init_action_handles()
{
	SET_ACTION_SET(InGameControls);
	SET_ACTION_SET(MenuControls);

	SET_ANALOG_ACTION(move);
	SET_ANALOG_ACTION(camera);

	SET_DIGITAL_ACTION(primary);
	SET_DIGITAL_ACTION(secondary);
	SET_DIGITAL_ACTION(reload);
	SET_DIGITAL_ACTION(use);
	SET_DIGITAL_ACTION(jump);
	SET_DIGITAL_ACTION(walk);
	SET_DIGITAL_ACTION(crouch);
	SET_DIGITAL_ACTION(special);
	SET_DIGITAL_ACTION(drop);
	SET_DIGITAL_ACTION(affinity);
	//SET_DIGITAL_ACTION(dash);
	//SET_DIGITAL_ACTION(next_weapon);
	//SET_DIGITAL_ACTION(previous_weapon);
	//SET_DIGITAL_ACTION(primary_weapon);
	//SET_DIGITAL_ACTION(secondary_weapon);
	//SET_DIGITAL_ACTION(wheel_select);
	//SET_DIGITAL_ACTION(change_loadout);
	SET_DIGITAL_ACTION(scoreboard); // showscores

	SET_DIGITAL_ACTION(recenter_camera);
}

void handle_digital_action_ac(class digital_action_state *das, int ac)
{
	das->update();

	if (das->just_pressed())
		physics::doaction(ac, true);
	else if (das->just_released())
		physics::doaction(ac, false);
}

void update_from_controller()
{
	// Steamworks ( https://partner.steamgames.com/doc/api/ISteamInput#RunFrame ) says that
	// > Synchronize API state with the latest Steam Controller inputs
	// > available. This is performed automatically by
	// > SteamAPI_RunCallbacks, but for the absolute lowest possible
	// > latency, you can call this directly before reading controller
	// > state.
	// which appears to be necessary here, otherwise we seem to drop some
	// gamepad inputs
	cdpi::steam::input->RunFrame();

	int connected_count = cdpi::steam::input->GetConnectedControllers(controllers);
	InputActionSetHandle_t current_set = hud::hasinput(true) ? MenuControls_handle : InGameControls_handle;
	cdpi::steam::input->ActivateActionSet(STEAM_INPUT_HANDLE_ALL_CONTROLLERS, current_set);
	InputAnalogActionData_t move_data = cdpi::steam::input->GetAnalogActionData(
	    controllers[0],
	    move_handle
	);

	//game::player1->move = move_data.y;

	if (move_data.y < -0.5f)
		game::player1->move = -1;
	else if (move_data.y > 0.5f)
		game::player1->move = 1;
	else if (!lastmovementwaskeyboard)
		game::player1->move = 0;

	//game::player1->strafe = -move_data.x;

	if (move_data.x < -0.5f)
		game::player1->strafe = 1;
	else if (move_data.x > 0.5f)
		game::player1->strafe = -1;
	else if (!lastmovementwaskeyboard)
		game::player1->strafe = 0;

	if (move_data.x != 0 && move_data.y != 0)
		lastmovementwaskeyboard = false;

	// We have to read the camera delta every frame even if we don't intend
	// on doing anything with it, otherwise it will 'build up', which is not
	// what we want in the cases where we are going to deliberately ignore
	// it.
	InputAnalogActionData_t camera_delta = cdpi::steam::input->GetAnalogActionData(
		controllers[0],
		camera_handle
	);

	recenter_camera.update();
	if (recenter_camera.pressed()) {
		game::player1->pitch = 0.0f;
	} else {
		// We deliberately *do not* respect the mouse sensitivity
		// settings here.  The Steamworks page 'getting started' page (
		// https://partner.steamgames.com/doc/features/steam_controller/getting_started_for_devs
		// ) explicitly states that:

		// > You should either rely on the configurator to provide
		// > sensitivity (ie you don't filter incoming Steam Input
		// > data), or you should use a dedicated sensitivity option for
		// > Steam Input that's distinct from the system mouse.

		// We opt to take Option A - make gamepad aim sensitivity
		// entirely the responsibility of Steam Input. This reduces the
		// amount of additional support code needed in-engine and also
		// has the added benefit that the 'Dots per 360' setting for
		// flick stick and RWS gyro configuration is always a fixed
		// value in every configuration. (This value is 3600, for the
		// record).  We choose to make the universal base sensitivity
		// the value exactly the same as the default sensitivity for
		// mouse, so that Steam Input 100% sensitivity matches the
		// game's default setting.

		game::player1->yaw += mousesens(camera_delta.x, 100.f, 10.f * game::zoomsens());
		game::player1->pitch -= mousesens(camera_delta.y, 100.f, 10.f * game::zoomsens());
		fixrange(game::player1->yaw, game::player1->pitch);
	}

	// WIP: these things all work fine because I am not trying to call
	// commands to make them go
	handle_digital_action_ac(&primary, AC_PRIMARY);
	handle_digital_action_ac(&secondary, AC_SECONDARY);
	handle_digital_action_ac(&reload, AC_RELOAD);
	handle_digital_action_ac(&use, AC_USE);
	handle_digital_action_ac(&jump, AC_JUMP);
	handle_digital_action_ac(&walk, AC_WALK);
	handle_digital_action_ac(&crouch, AC_CROUCH);
	handle_digital_action_ac(&special, AC_SPECIAL);
	handle_digital_action_ac(&drop, AC_DROP);
	handle_digital_action_ac(&affinity, AC_AFFINITY);

	// WIP: this thing does not work; I don't know how to call commands
	// directly
	scoreboard.update();
	tagval tv;
	if (scoreboard.just_pressed()) {
		printf("just pressed scoreboard\n");
		tv.setint(1);
	 	execute(getident("showscores"), &tv, 1);
	} else if (scoreboard.just_released()) {
		printf("just released scoreboard\n");
		tv.setint(0);
	 	execute(getident("showscores"), &tv, 1);
	}
}
#else /* defined(USE_STEAM) */
void init_action_handles()
{
	return;
}

void update_from_controller()
{
	return;
}
#endif /* defined(USE_STEAM) */
}
