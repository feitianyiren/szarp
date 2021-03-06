/* 
  SZARP: SCADA software 
  

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

#include "conversion.h"

#include "szarp_base_common/lua_utils.h"
#include "sz4/types.h"
#include "sz4/exceptions.h"
#include "sz4/base.h"
#include "sz4/util.h"

#ifndef luaL_reg
#define luaL_reg luaL_Reg
#endif


namespace sz4 {

template<class base> struct base_ipk_pair : public std::pair<base*, typename base::ipk_container_type*> {};

template<class base> base_ipk_pair<base>* get_base_ipk_pair(lua_State *lua) {
	base_ipk_pair<base>* ret;
	lua_pushlightuserdata(lua, (void*)&lua_interpreter<base>::lua_base_ipk_pair_key);
	lua_gettable(lua, LUA_REGISTRYINDEX);
	ret = (base_ipk_pair<base>*)lua_touserdata(lua, -1);
	lua_pop(lua, 1);
	return ret;
}

template<class base> void push_base_ipk_pair(lua_State *lua, base* _base, typename base::ipk_container_type* container) {
	lua_pushlightuserdata(lua, (void*)&lua_interpreter<base>::lua_base_ipk_pair_key);

	base_ipk_pair<base>* pair = (base_ipk_pair<base>*)lua_newuserdata(lua, sizeof(base_ipk_pair<base>));
	pair->first = _base;
	pair->second = container;

	lua_settable(lua, LUA_REGISTRYINDEX);
}

template<class base> int lua_sz4(lua_State *lua) {
	const unsigned char* param_name = (unsigned char*) luaL_checkstring(lua, 1);
	if (param_name == NULL) 
                 luaL_error(lua, "Invalid param name");

	TParam* param = NULL;
	try {
		nanosecond_time_t time(lua_tonumber(lua, 2));
		SZARP_PROBE_TYPE probe_type(static_cast<SZARP_PROBE_TYPE>((int)lua_tonumber(lua, 3)));

		base_ipk_pair<base>* base_ipk = get_base_ipk_pair<base>(lua);
		param = base_ipk->second->GetParam(std::basic_string<unsigned char>(param_name));

		if (param)
		{
			weighted_sum<double, nanosecond_time_t> sum;
			base_ipk->first->get_weighted_sum(param, time, szb_move_time(time, 1, SZARP_PROBE_TYPE(probe_type), 0), probe_type, sum);

			base_ipk->first->fixed_stack().back() = base_ipk->first->fixed_stack().back() & sum.fixed();

			lua_pushnumber(lua, scale_value(sum.avg(), param));

			return 1;
		}

	} catch (exception &e) {
		luaL_where(lua, 1);
		lua_pushfstring(lua, "%s", e.what());
		lua_concat(lua, 2);
	}

	if (param)
	{
		return lua_error(lua);
	}
	else
	{
		return luaL_error(lua, "Param %s not found", param_name);
	}
}

int lua_sz4_move_time(lua_State* lua);

int lua_sz4_round_time(lua_State* lua);

template<class base> int lua_sz4_in_season(lua_State *lua) {
	const unsigned char* prefix = reinterpret_cast<const unsigned char*>(luaL_checkstring(lua, 1));
	TSzarpConfig* cfg = NULL;
	{
		time_t time(lua_tonumber(lua, 2));
		base_ipk_pair<base>* base_ipk = get_base_ipk_pair<base>(lua);

		cfg = base_ipk->second->GetConfig(SC::U2S(prefix));
		if (cfg) {
			lua_pushboolean(lua, cfg->GetSeasons()->IsSummerSeason(time));
			return 1;
		}
	}

	return luaL_error(lua, "Config %s not found", (char*)prefix);
}

int lua_sz4_isnan(lua_State* lua);

int lua_sz4_nan(lua_State* lua);

template<class base> lua_interpreter<base>::lua_interpreter() {
	m_lua = lua_open();
	if (m_lua == NULL) {
		throw exception("Failed to initialize lua interpreter");	
	}

	luaL_openlibs(m_lua);
	lua::set_probe_types_globals(m_lua);

	const struct luaL_reg sz4_funcs_lib[] = {
		{ "szbase", lua_sz4<base> },
		{ "szb_move_time", lua_sz4_move_time },
		{ "szb_round_time", lua_sz4_round_time },
//		{ "szb_search_first", lua_sz4_search_first },
//		{ "szb_search_last", lua_sz4_search_last },
		{ "isnan", lua_sz4_isnan },
		{ "nan", lua_sz4_nan },
		{ "in_season", lua_sz4_in_season<base> },
		{ NULL, NULL }
	};

	const luaL_Reg *lib = sz4_funcs_lib;

	for (; lib->func; lib++) 
		lua_register(m_lua, lib->name, lib->func);

};

template<class base> void lua_interpreter<base>::initialize(base* _base, typename base::ipk_container_type* container) {
	push_base_ipk_pair(m_lua, _base, container);
}

template<class base> bool lua_interpreter<base>::prepare_param(TParam* param) {
	return lua::prepare_param(m_lua, param);
}

template<class base> double lua_interpreter<base>::calculate_value(nanosecond_time_t start, SZARP_PROBE_TYPE probe_type, int custom_length) {
	double result;

	lua_pushvalue(m_lua, -1);
	lua_pushnumber(m_lua, start);
	lua_pushnumber(m_lua, probe_type);
	lua_pushnumber(m_lua, custom_length);

	int ret = lua_pcall(m_lua, 3, 1, 0);
	if (ret != 0)
		throw exception(std::string("Lua param execution error: ") + lua_tostring(m_lua, -1));

	if (lua_isnumber(m_lua, -1))
		result = lua_tonumber(m_lua, -1);
	else if (lua_isboolean(m_lua, -1))
		result = lua_toboolean(m_lua, -1);
	else
		result = nan("");
	lua_pop(m_lua, 1);

	return result;
}

template<class base> void lua_interpreter<base>::pop_prepared_param() {
	lua_pop(m_lua, 1);
}

template<class base> lua_interpreter<base>::~lua_interpreter() {
	if (m_lua)
		lua_close(m_lua);
}

template<class base> lua_State* lua_interpreter<base>::lua() {
	return m_lua;
}

template<class base> const int lua_interpreter<base>::lua_base_ipk_pair_key = 0;
}
