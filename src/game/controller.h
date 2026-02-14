#ifndef CPP_CONTROLLER_HEADER
#define CPP_CONTROLLER_HEADER

#include "rendertext.h"

namespace controller
{
	extern bool lastinputwassiapi;
	extern bool lastmovementwaskeyboard;
	extern void update_from_controller();
	extern bool is_siapi_textkey(const char *str);
	extern vector<textkey *> get_siapi_textkeys(const char *str);
}
#endif
