//#include "controller.h"
#include "game.h"
#include "tools.h"
#include "rendertext.h"
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
#define SET_DIGITAL_ACTION(x) x.handle = cdpi::steam::input->GetDigitalActionHandle(#x)

namespace controller
{
// Track if last input was keyboard or mouse buttons.
// Set to true when using any SIAPI actions
// Set to false when using any other type of action
// Note that mouse motion does _not_ reset the state of this variable.
bool lastinputwassiapi = false;

// If we are not using the SIAPI move action, then do not overwrite strafing
// data set by the regular keyboard bindings
bool lastmovementwaskeyboard = true;

// keymap codes to nice names - keep this in sync with keymaps.cfg
enum siapi_keycodes {
	SIAPI_PRIMARY = -20,
	SIAPI_SECONDARY = -21,
	SIAPI_RELOAD = -22,
	SIAPI_USE = -23,
	SIAPI_JUMP = -24,
	SIAPI_WALK = -25,
	SIAPI_CROUCH = -26,
	SIAPI_SPECIAL = -27,
	SIAPI_DROP = -28,
	SIAPI_AFFINITY = -29,
	SIAPI_DASH = -30,
	SIAPI_NEXT_WEAPON = -31,
	SIAPI_PREVIOUS_WEAPON = -32,
	SIAPI_PRIMARY_WEAPON = -33,
	SIAPI_SECONDARY_WEAPON = -34,
	SIAPI_WHEEL_SELECT = -35,
	SIAPI_CHANGE_LOADOUT = -36,
	SIAPI_SCOREBOARD = -37,
	SIAPI_SUICIDE = -38,
	SIAPI_MENU = -39,
};

// This current controller implementation depends on Steam Input and is not
// available outside of Steam
#if defined(USE_STEAM)

InputHandle_t *controllers = new InputHandle_t[STEAM_INPUT_MAX_COUNT];

bool get_digital_action_state(int siapi_digital_handle)
{
	InputDigitalActionData_t data = cdpi::steam::input->GetDigitalActionData(controllers[0], siapi_digital_handle);
	if (data.bState) lastinputwassiapi = true;
	return data.bState;
}

class digital_action_state
{
	bool input_last_frame = false;
	bool input_this_frame = false;

public:
	InputDigitalActionHandle_t handle = -1;
	int keymap_id = -1;
	// Should be more than one origin eventually!
	EInputActionOrigin origin = k_EInputActionOrigin_None;
	textkey *tk = NULL;

	void update()
	{
		this->input_last_frame = this->input_this_frame;
		this->input_this_frame = get_digital_action_state(this->handle);
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

	void process()
	{
		this->update();

		if (this->just_pressed())
			processkey(this->keymap_id, true);
		else if (this->just_released())
			processkey(this->keymap_id, false);
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
DEF_DIGITAL_ACTION(suicide);
DEF_DIGITAL_ACTION(menu);

DEF_DIGITAL_ACTION(recenter_camera);

void init_action_handles()
{
	SET_ACTION_SET(InGameControls);
	SET_ACTION_SET(MenuControls);

	SET_ANALOG_ACTION(move);
	SET_ANALOG_ACTION(camera);

	SET_DIGITAL_ACTION(primary);
	primary.keymap_id = SIAPI_PRIMARY;

	SET_DIGITAL_ACTION(secondary);
	secondary.keymap_id = SIAPI_SECONDARY;

	SET_DIGITAL_ACTION(reload);
	reload.keymap_id = SIAPI_RELOAD;

	SET_DIGITAL_ACTION(use);
	use.keymap_id = SIAPI_USE;

	SET_DIGITAL_ACTION(jump);
	jump.keymap_id = SIAPI_JUMP;

	SET_DIGITAL_ACTION(walk);
	walk.keymap_id = SIAPI_WALK;

	SET_DIGITAL_ACTION(crouch);
	crouch.keymap_id = SIAPI_CROUCH;

	SET_DIGITAL_ACTION(special);
	special.keymap_id = SIAPI_SPECIAL;

	SET_DIGITAL_ACTION(drop);
	drop.keymap_id = SIAPI_DROP;

	SET_DIGITAL_ACTION(affinity);
	affinity.keymap_id = SIAPI_AFFINITY;

	SET_DIGITAL_ACTION(dash);
	dash.keymap_id = SIAPI_DASH;

	SET_DIGITAL_ACTION(next_weapon);
	next_weapon.keymap_id = SIAPI_NEXT_WEAPON;

	SET_DIGITAL_ACTION(previous_weapon);
	previous_weapon.keymap_id = SIAPI_PREVIOUS_WEAPON;

	SET_DIGITAL_ACTION(primary_weapon);
	primary_weapon.keymap_id = SIAPI_PRIMARY_WEAPON;

	SET_DIGITAL_ACTION(secondary_weapon);
	secondary_weapon.keymap_id = SIAPI_SECONDARY_WEAPON;

	SET_DIGITAL_ACTION(wheel_select);
	wheel_select.keymap_id = SIAPI_WHEEL_SELECT;

	SET_DIGITAL_ACTION(change_loadout);
	change_loadout.keymap_id = SIAPI_CHANGE_LOADOUT;

	SET_DIGITAL_ACTION(scoreboard);
	scoreboard.keymap_id = SIAPI_SCOREBOARD;

	SET_DIGITAL_ACTION(suicide);
	suicide.keymap_id = SIAPI_SUICIDE;

	SET_DIGITAL_ACTION(menu);
	menu.keymap_id = SIAPI_MENU;

	SET_DIGITAL_ACTION(recenter_camera);
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

	if (move_data.x != 0 || move_data.y != 0) {
		lastinputwassiapi = true;
		lastmovementwaskeyboard = false;
	}

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

		if (camera_delta.x != 0 || camera_delta.y != 0) lastinputwassiapi = true;
		game::player1->yaw += mousesens(camera_delta.x, 100.f, 10.f * game::zoomsens());
		game::player1->pitch -= mousesens(camera_delta.y, 100.f, 10.f * game::zoomsens());
		fixrange(game::player1->yaw, game::player1->pitch);
	}

	primary.process();
	secondary.process();
	reload.process();
	use.process();
	jump.process();
	walk.process();
	crouch.process();
	special.process();
	drop.process();
	affinity.process();
	dash.process();
	next_weapon.process();
	previous_weapon.process();
	primary_weapon.process();
	secondary_weapon.process();
	wheel_select.process();
	change_loadout.process();
	scoreboard.process();
	suicide.process();
	menu.process();
}

bool is_siapi_textkey(const char *str)
{
	return !strncmp(str, "SIAPI_", 6);
}

digital_action_state *get_das_for_keymap_name(const char *str)
{
	// This function is awful, redo to be smarter
	if (!strcmp(str, "SIAPI_PRIMARY")) return &primary;
	if (!strcmp(str, "SIAPI_SECONDARY")) return &secondary;
	if (!strcmp(str, "SIAPI_RELOAD")) return &reload;
	if (!strcmp(str, "SIAPI_USE")) return &use;
	if (!strcmp(str, "SIAPI_JUMP")) return &jump;
	if (!strcmp(str, "SIAPI_WALK")) return &walk;
	if (!strcmp(str, "SIAPI_CROUCH")) return &crouch;
	if (!strcmp(str, "SIAPI_SPECIAL")) return &special;
	if (!strcmp(str, "SIAPI_DROP")) return &drop;
	if (!strcmp(str, "SIAPI_AFFINITY")) return &affinity;
	if (!strcmp(str, "SIAPI_DASH")) return &dash;
	if (!strcmp(str, "SIAPI_NEXT_WEAPON")) return &next_weapon;
	if (!strcmp(str, "SIAPI_PREVIOUS_WEAPON")) return &previous_weapon;
	if (!strcmp(str, "SIAPI_PRIMARY_WEAPON")) return &primary_weapon;
	if (!strcmp(str, "SIAPI_SECONDARY_WEAPON")) return &secondary_weapon;
	if (!strcmp(str, "SIAPI_WHEEL_SELECT")) return &wheel_select;
	if (!strcmp(str, "SIAPI_CHANGE_LOADOUT")) return &change_loadout;
	if (!strcmp(str, "SIAPI_SCOREBOARD")) return &scoreboard;
	if (!strcmp(str, "SIAPI_SUICIDE")) return &suicide;
	if (!strcmp(str, "SIAPI_MENU")) return &menu;
	return NULL; // Should never happen
}

EInputActionOrigin *origins = NULL;

textkey *get_siapi_textkey(const char *str)
{
	digital_action_state *das = get_das_for_keymap_name(str);

	if (!das->tk) {
		das->tk = new textkey;
		das->tk->file = NULL; // we don't use this here
		das->tk->name = newstring(str);
	}
	// We have to check if the origin has changed, and if so, reload the texture
	if(!origins) origins = new EInputActionOrigin[STEAM_INPUT_MAX_ORIGINS];
	// Have to clear the origins ourselves
	origins[0] = k_EInputActionOrigin_None;
	cdpi::steam::input->GetDigitalActionOrigins(
		controllers[0],
		hud::hasinput(true) ? MenuControls_handle : InGameControls_handle,
		das->handle,
		origins
	);
	if (origins[0] == k_EInputActionOrigin_None || origins[0] > k_EInputActionOrigin_MaximumPossibleValue) {
		das->tk->tex = NULL;
	} else if (origins[0] != das->origin) {
		const char *siapi_origin_glyph = cdpi::steam::input->GetGlyphPNGForActionOrigin(
			origins[0],
			k_ESteamInputGlyphSize_Medium,
			ESteamInputGlyphStyle_Dark
		);
		das->tk->tex = textureload(siapi_origin_glyph, 3, true, false);
		if(das->tk->tex == notexture) das->tk->tex = NULL;
	}
	das->origin = origins[0];
	return das->tk;
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

bool is_siapi_textkey(const char *str)
{
	return false;
}

textkey *get_siapi_textkey(const char *str)
{
	return NULL;
}
#endif /* defined(USE_STEAM) */
}
