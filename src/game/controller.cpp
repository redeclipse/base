//#include "controller.h"
#include "game.h"
#include "tools.h"
#include "rendertext.h"
#include "engine.h"

#if defined(USE_STEAM)
#include "steam_api_flat.h"
#endif

#include <stdio.h>

#define DEF_ACTION_SET(x) InputActionSetHandle_t x##_handle = 0
#define DEF_ANALOG_ACTION(x) InputAnalogActionHandle_t x##_handle = 0

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
        SIAPI_LAST_WEAPON = -35,
	SIAPI_WHEEL_SELECT = -36,
	SIAPI_CHANGE_LOADOUT = -37,
	SIAPI_SCOREBOARD = -38,
	SIAPI_SUICIDE = -39,
	SIAPI_MENU = -40,
        SIAPI_CLAW = -41,
        SIAPI_PISTOL = -42,
        SIAPI_SWORD = -43,
        SIAPI_SHOTGUN = -44,
        SIAPI_SMG = -45,
        SIAPI_FLAMER = -46,
        SIAPI_PLASMA = -47,
        SIAPI_ZAPPER = -48,
        SIAPI_RIFLE = -49,
        SIAPI_CORRODER = -50,
        SIAPI_GRENADE = -51,
        SIAPI_MINE = -52,
        SIAPI_ROCKET = -53,
        SIAPI_MINIGUN = -54,
        SIAPI_JETSAW = -55,
        SIAPI_MELEE = -56,
	// The menu code does not use the keymap system and doesn't allow you to
	// rebind those controls. Because of this, it is safe to 'pretend' to be
	// the other keys directly with no consequences.
	SIAPI_MENU_SELECT = -1, // left mouse
	SIAPI_MENU_CANCEL = -2, // right mouse
	SIAPI_MENU_SCROLL_UP = -4, // scroll wheel -Y
	SIAPI_MENU_SCROLL_DOWN = -5, // scroll wheel +Y
};

// This current controller implementation depends on Steam Input and is not
// available outside of Steam
#if defined(USE_STEAM)

InputHandle_t *controllers = new InputHandle_t[STEAM_INPUT_MAX_COUNT];
InputHandle_t lastusedcontroller = 0;

bool get_digital_action_state(int controlleridx, int siapi_digital_handle)
{
	InputDigitalActionData_t data = cdpi::steam::input->GetDigitalActionData(controllers[controlleridx], siapi_digital_handle);
	if (data.bState) {
		lastinputwassiapi = true;
		lastusedcontroller = controllers[controlleridx];
	}
	return data.bState;
}

class digital_action_state
{
	bool input_last_frame[STEAM_INPUT_MAX_COUNT];
	bool input_this_frame[STEAM_INPUT_MAX_COUNT];

public:
	InputDigitalActionHandle_t handle = -1;
	int keymap_id = -1;
	// Should be more than one origin eventually!
	EInputActionOrigin origin = k_EInputActionOrigin_None;

	void update(int controlleridx)
	{
		this->input_last_frame[controlleridx] = this->input_this_frame[controlleridx];
		this->input_this_frame[controlleridx] = get_digital_action_state(controlleridx, this->handle);
	}

	bool pressed(int controlleridx)
	{
		return this->input_this_frame[controlleridx];
	}

	bool released(int controlleridx)
	{
		return !this->input_this_frame[controlleridx];
	}

	bool just_pressed(int controlleridx)
	{
		return this->input_this_frame[controlleridx] && !this->input_last_frame[controlleridx];
	}

	bool just_released(int controlleridx)
	{
		return !this->input_this_frame[controlleridx] && this->input_last_frame[controlleridx];
	}

	void ingame_process(int controlleridx)
	{
		this->update(controlleridx);

		if (this->just_pressed(controlleridx))
			processkey(this->keymap_id, true);
		else if (this->just_released(controlleridx))
			processkey(this->keymap_id, false);
	}

	void menu_process(int controlleridx)
	{
		this->update(controlleridx);

		if (this->just_pressed(controlleridx))
			UI::keypress(this->keymap_id, true);
		else if (this->just_released(controlleridx))
			UI::keypress(this->keymap_id, false);
	}
};

DEF_ACTION_SET(InGameControls);

DEF_ANALOG_ACTION(move);
DEF_ANALOG_ACTION(camera);

class digital_action_state primary;
class digital_action_state secondary;
class digital_action_state reload;
class digital_action_state use;
class digital_action_state jump;
class digital_action_state walk;
class digital_action_state crouch;
class digital_action_state special;
class digital_action_state drop;
class digital_action_state affinity;
class digital_action_state dash;
class digital_action_state next_weapon;
class digital_action_state previous_weapon;
class digital_action_state primary_weapon;
class digital_action_state secondary_weapon;
class digital_action_state last_weapon;
class digital_action_state claw;
class digital_action_state pistol;
class digital_action_state sword;
class digital_action_state shotgun;
class digital_action_state smg;
class digital_action_state flamer;
class digital_action_state plasma;
class digital_action_state zapper;
class digital_action_state rifle;
class digital_action_state corroder;
class digital_action_state grenade;
class digital_action_state mine;
class digital_action_state rocket;
class digital_action_state minigun;
class digital_action_state jetsaw;
class digital_action_state melee;
class digital_action_state wheel_select;
class digital_action_state change_loadout;
class digital_action_state scoreboard;
class digital_action_state suicide;
class digital_action_state menu;

class digital_action_state recenter_camera;

DEF_ACTION_SET(MenuControls);
DEF_ANALOG_ACTION(menu_cursor);
class digital_action_state menu_select;
class digital_action_state menu_cancel;
class digital_action_state menu_scroll_up;
class digital_action_state menu_scroll_down;

DEF_ACTION_SET(EditingControls);

void init_siapi_handles()
{
	SET_ACTION_SET(InGameControls);

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

        SET_DIGITAL_ACTION(last_weapon);
        last_weapon.keymap_id = SIAPI_LAST_WEAPON;

        SET_DIGITAL_ACTION(claw);
	claw.keymap_id = SIAPI_CLAW;

        SET_DIGITAL_ACTION(pistol);
	pistol.keymap_id = SIAPI_PISTOL;

        SET_DIGITAL_ACTION(sword);
	sword.keymap_id = SIAPI_SWORD;

        SET_DIGITAL_ACTION(shotgun);
	shotgun.keymap_id = SIAPI_SHOTGUN;

        SET_DIGITAL_ACTION(smg);
	smg.keymap_id = SIAPI_SMG;

        SET_DIGITAL_ACTION(flamer);
	flamer.keymap_id = SIAPI_FLAMER;

        SET_DIGITAL_ACTION(plasma);
	plasma.keymap_id = SIAPI_PLASMA;

        SET_DIGITAL_ACTION(zapper);
	zapper.keymap_id = SIAPI_ZAPPER;

        SET_DIGITAL_ACTION(rifle);
	rifle.keymap_id = SIAPI_RIFLE;

        SET_DIGITAL_ACTION(corroder);
	corroder.keymap_id = SIAPI_CORRODER;

        SET_DIGITAL_ACTION(grenade);
	grenade.keymap_id = SIAPI_GRENADE;

        SET_DIGITAL_ACTION(mine);
	mine.keymap_id = SIAPI_MINE;

        SET_DIGITAL_ACTION(rocket);
	rocket.keymap_id = SIAPI_ROCKET;

        SET_DIGITAL_ACTION(minigun);
	minigun.keymap_id = SIAPI_MINIGUN;

        SET_DIGITAL_ACTION(jetsaw);
	jetsaw.keymap_id = SIAPI_JETSAW;

        SET_DIGITAL_ACTION(melee);
	melee.keymap_id = SIAPI_MELEE;

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

	SET_ACTION_SET(MenuControls);
	SET_ANALOG_ACTION(menu_cursor);

	SET_DIGITAL_ACTION(menu_select);
	menu_select.keymap_id = SIAPI_MENU_SELECT;

	SET_DIGITAL_ACTION(menu_cancel);
	menu_cancel.keymap_id = SIAPI_MENU_CANCEL;

	SET_DIGITAL_ACTION(menu_scroll_up);
	menu_scroll_up.keymap_id = SIAPI_MENU_SCROLL_UP;

	SET_DIGITAL_ACTION(menu_scroll_down);
	menu_scroll_down.keymap_id = SIAPI_MENU_SCROLL_DOWN;

	SET_ACTION_SET(EditingControls);
}

void update_ingame_actions(int controlleridx);
void update_menu_actions(int controlleridx);

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

	if (connected_count == 0) return;

        // Initialize handles if needed
        if(!(InGameControls_handle && MenuControls_handle && EditingControls_handle)) controller::init_siapi_handles();

	for (int i = 0; i < connected_count; i++) {
		if (editmode) {
			// TODO: We currently don't have SIAPI actions for
			// editing mode, but we do provide an action set for
			// convenience
			cdpi::steam::input->ActivateActionSet(STEAM_INPUT_HANDLE_ALL_CONTROLLERS, EditingControls_handle);
			continue;
		}

		if (UI::hasinput() && !UI::menuisgameplay()) {
			cdpi::steam::input->ActivateActionSet(STEAM_INPUT_HANDLE_ALL_CONTROLLERS, MenuControls_handle);
			update_menu_actions(i);
			continue;
		}

		cdpi::steam::input->ActivateActionSet(STEAM_INPUT_HANDLE_ALL_CONTROLLERS, InGameControls_handle);
		update_ingame_actions(i);
	}
}

void update_ingame_actions(int controlleridx)
{
	InputAnalogActionData_t move_data = cdpi::steam::input->GetAnalogActionData(
	    controllers[controlleridx],
	    move_handle
	);

	if (lastusedcontroller == controllers[controlleridx]) {
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
	}

	if (move_data.x != 0 || move_data.y != 0) {
		lastinputwassiapi = true;
		lastusedcontroller = controllers[controlleridx];
		lastmovementwaskeyboard = false;
	}

	// We have to read the camera delta every frame even if we don't intend
	// on doing anything with it, otherwise it will 'build up', which is not
	// what we want in the cases where we are going to deliberately ignore
	// it.
	InputAnalogActionData_t camera_delta = cdpi::steam::input->GetAnalogActionData(
		controllers[controlleridx],
		camera_handle
	);

	recenter_camera.update(controlleridx);
	if (recenter_camera.pressed(controlleridx)) {
		game::resetplayerpitch();
	} else {
		game::mousemove(camera_delta.x, camera_delta.y, 0, 0, screenw, screenh, true);
		if (camera_delta.x != 0 || camera_delta.y != 0) {
			lastinputwassiapi = true;
			lastusedcontroller = controllers[controlleridx];
		}
	}

	primary.ingame_process(controlleridx);
	secondary.ingame_process(controlleridx);
	reload.ingame_process(controlleridx);
	use.ingame_process(controlleridx);
	jump.ingame_process(controlleridx);
	walk.ingame_process(controlleridx);
	crouch.ingame_process(controlleridx);
	special.ingame_process(controlleridx);
	drop.ingame_process(controlleridx);
	affinity.ingame_process(controlleridx);
	dash.ingame_process(controlleridx);
	next_weapon.ingame_process(controlleridx);
	previous_weapon.ingame_process(controlleridx);
	primary_weapon.ingame_process(controlleridx);
	secondary_weapon.ingame_process(controlleridx);
        last_weapon.ingame_process(controlleridx);
        claw.ingame_process(controlleridx);
        pistol.ingame_process(controlleridx);
        sword.ingame_process(controlleridx);
        shotgun.ingame_process(controlleridx);
        smg.ingame_process(controlleridx);
        flamer.ingame_process(controlleridx);
        plasma.ingame_process(controlleridx);
        zapper.ingame_process(controlleridx);
        rifle.ingame_process(controlleridx);
        corroder.ingame_process(controlleridx);
        grenade.ingame_process(controlleridx);
        mine.ingame_process(controlleridx);
        rocket.ingame_process(controlleridx);
        minigun.ingame_process(controlleridx);
        jetsaw.ingame_process(controlleridx);
        melee.ingame_process(controlleridx);
	wheel_select.ingame_process(controlleridx);
	change_loadout.ingame_process(controlleridx);
	scoreboard.ingame_process(controlleridx);
	suicide.ingame_process(controlleridx);
	menu.ingame_process(controlleridx);
}

void update_menu_actions(int controlleridx)
{
	InputAnalogActionData_t cursor_data = cdpi::steam::input->GetAnalogActionData(
	    controllers[controlleridx],
	    menu_cursor_handle
	);

	game::mousemove(cursor_data.x, cursor_data.y, 0, 0, screenw, screenh, true);

	if (cursor_data.x != 0 || cursor_data.y != 0)
		lastinputwassiapi = true;

	menu_select.menu_process(controlleridx);
	menu_cancel.menu_process(controlleridx);
	menu_scroll_up.menu_process(controlleridx);
	menu_scroll_down.menu_process(controlleridx);
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

vector<textkey *> textkeys;

textkey *get_siapi_textkey(const char *str)
{
	digital_action_state *das = get_das_for_keymap_name(str);

	if(!origins) origins = new EInputActionOrigin[STEAM_INPUT_MAX_ORIGINS];
	// Have to clear the origins ourselves
	origins[0] = k_EInputActionOrigin_None;
	cdpi::steam::input->GetDigitalActionOrigins(
		lastusedcontroller,
		hud::hasinput(true) ? MenuControls_handle : InGameControls_handle,
		das->handle,
		origins
	);

        const char *siapi_origin_glyph = cdpi::steam::input->GetGlyphPNGForActionOrigin(
                origins[0],
                k_ESteamInputGlyphSize_Medium,
                ESteamInputGlyphStyle_Dark
        );

        char origin_enum_string[13];
        snprintf(origin_enum_string, 13, "origin_%d", origins[0]);

        return findtextkey_common(origin_enum_string, textkeys, siapi_origin_glyph);
}

ICOMMAND(0, showsiapibindpanel, "", (), { cdpi::steam::input->ShowBindingPanel(lastusedcontroller); });
#else /* defined(USE_STEAM) */
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

ICOMMAND(0, showsiapibindpanel, "", (), { return; });
#endif /* defined(USE_STEAM) */
}
