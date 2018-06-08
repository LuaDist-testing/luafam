/*
** LuaFAM
** Copyright DarkGod 2007
**
*/

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fam.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#define MT_FAM_CONN	"FAM Connection"
#define MT_FAM_REQUEST	"FAM Request"
#define MT_FAM_EVENT	"FAM Event"

#define REG_FAMLUA	"famlualib"

/* Make a new FAM connection */
static int fam_new(lua_State *L)
{
	FAMConnection conn;

	if (!FAMOpen(&conn))
	{
		FAMConnection *alloc_conn = (FAMConnection *)lua_newuserdata(L, sizeof(FAMConnection));
		luaL_getmetatable (L, MT_FAM_CONN);
		lua_setmetatable (L, -2);
		*alloc_conn = conn;
		return 1;
	}
	else
	{
		lua_pushnil(L);
		return 1;
	}
}

static int fam_close(lua_State *L)
{
	FAMConnection *conn = (FAMConnection *)luaL_checkudata (L, 1, MT_FAM_CONN);
	FAMClose(conn);
	return 0;
}

static int fam_monitor_dir(lua_State *L)
{
	FAMConnection *conn = (FAMConnection *)luaL_checkudata (L, 1, MT_FAM_CONN);
	const char *path = luaL_checkstring (L, 2);
	FAMRequest req;

	if (!FAMMonitorDirectory(conn, path, &req, NULL))
	{
		FAMRequest *alloc_req;

		alloc_req = (FAMRequest *)lua_newuserdata(L, sizeof(FAMRequest));
		luaL_getmetatable (L, MT_FAM_REQUEST);
		lua_setmetatable (L, -2);
		*alloc_req = req;

		/* Store the path in the registry so we can retrieve it later */
		lua_pushliteral(L, REG_FAMLUA);
		lua_gettable(L, LUA_REGISTRYINDEX);
		lua_pushnumber(L, FAMREQUEST_GETREQNUM(&req));
		lua_pushstring(L, path);
		lua_settable(L, -3);
		lua_pop(L, 1);

		return 1;
	}
	else
	{
		lua_pushnil(L);
		return 1;
	}
}

static int fam_monitor_cancel(lua_State *L)
{
	FAMConnection *conn = (FAMConnection *)luaL_checkudata (L, 1, MT_FAM_CONN);
	FAMRequest *req = (FAMRequest *)luaL_checkudata (L, 2, MT_FAM_REQUEST);

	FAMCancelMonitor(conn, req);
	return 0;
}

static const char *event_name(int code)
{
	static const char *famevent[] = {
		"",
		"Changed",
		"Deleted",
		"StartExecuting",
		"StopExecuting",
		"Created",
		"Moved",
		"Acknowledge",
		"Exists",
		"EndExist"
	};
	static char unknown_event[10];

	if (code < FAMChanged || code > FAMEndExist)
	{
		sprintf(unknown_event, "unknown (%d)", code);
		return unknown_event;
	}
	return famevent[code];
}
static int fam_next_event(lua_State *L)
{
	FAMConnection *conn = (FAMConnection *)luaL_checkudata (L, 1, MT_FAM_CONN);
	FAMEvent evt;
	if (FAMNextEvent(conn, &evt) > 0)
	{
		lua_newtable(L);

		lua_pushliteral(L, "filename");
		lua_pushstring(L, evt.filename);
		lua_settable (L, -3);

		lua_pushliteral(L, "code");
		lua_pushstring(L, event_name(evt.code));
		lua_settable (L, -3);

		/* Get the path from the registry */
		lua_pushliteral(L, REG_FAMLUA); lua_gettable(L, LUA_REGISTRYINDEX);
		lua_pushliteral(L, "path");
		lua_pushnumber(L, FAMREQUEST_GETREQNUM((&evt.fr)));
		lua_gettable(L, -3);
		lua_settable(L, -4);
		lua_pop(L, 1);

		return 1;
	}
	else
	{
		lua_pushnil(L);
		return 1;
	}
	return 0;
}

static int fam_pending_event(lua_State *L)
{
	FAMConnection *conn = (FAMConnection *)luaL_checkudata (L, 1, MT_FAM_CONN);
	lua_pushnumber(L, FAMPending(conn));
	return 1;
}

static int fam_getFD(lua_State *L)
{
	FAMConnection *conn = (FAMConnection *)luaL_checkudata (L, 1, MT_FAM_CONN);
	lua_pushnumber(L, FAMCONNECTION_GETFD(conn));
	return 1;
}

static int fam_select(lua_State *L)
{
	FAMConnection *conn = (FAMConnection *)luaL_checkudata (L, 1, MT_FAM_CONN);
	int wait = luaL_checknumber(L, 2);
	struct timeval tv;
	fd_set rfds;
	int retval;

	/* Wait up to five seconds. */
	tv.tv_sec = wait;
	tv.tv_usec = 0;
	FD_ZERO(&rfds);
	FD_SET(FAMCONNECTION_GETFD(conn), &rfds);
	retval = select(1 + FAMCONNECTION_GETFD(conn), &rfds, NULL, NULL, &tv);
	if (retval == 1)
	{
		lua_pushboolean(L, 1);
	}
	else
	{
		lua_pushnil(L);
	}
	return 1;
}

/*
** Assumes the table is on top of the stack.
*/
static void set_info (lua_State *L)
{
	lua_pushliteral (L, "_COPYRIGHT");
	lua_pushliteral (L, "Copyright (C) 2007 DarkGod");
	lua_settable (L, -3);
	lua_pushliteral (L, "_DESCRIPTION");
	lua_pushliteral (L, "LuaFAM allows lua applications to monitor filesystem changes");
	lua_settable (L, -3);
	lua_pushliteral (L, "_VERSION");
	lua_pushliteral (L, "LuaFAM 1.0.0");
	lua_settable (L, -3);
}

static const struct luaL_reg famlib[] =
{
	{"open", fam_new},
	{"close", fam_new},
	{"monitorDirectory", fam_monitor_dir},
	{"monitorCancel", fam_monitor_cancel},
	{"nextEvent", fam_next_event},
	{"pendingEvent", fam_pending_event},
	{"waitEvent", fam_select},
	{"getFD", fam_getFD},
	{NULL, NULL},
};

int luaopen_fam (lua_State *L)
{
	luaL_newmetatable (L, MT_FAM_CONN);
	luaL_newmetatable (L, MT_FAM_REQUEST);
	luaL_newmetatable (L, MT_FAM_EVENT);

	lua_pushliteral(L, REG_FAMLUA);
	lua_newtable(L);
	lua_settable(L, LUA_REGISTRYINDEX);

	luaL_openlib(L, "fam", famlib, 0);
	set_info(L);
	return 1;
}
