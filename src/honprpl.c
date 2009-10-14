#include "honprpl.h"
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>


#include "account.h"
#include "accountopt.h"
#include "blist.h"
#include "cmds.h"
#include "conversation.h"
#include "connection.h"
#include "debug.h"
#include "notify.h"
#include "privacy.h"
#include "prpl.h"
#include "roomlist.h"
#include "status.h"
#include "util.h"
#include "version.h"

#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#endif


static PurplePlugin *_HON_protocol = NULL;
/*

	Utility functions

*/
/* normalize a username (e.g. remove whitespace, add default domain, etc.)
* for honprpl, this is a noop.
*/
static const char *honprpl_normalize(const PurpleAccount *acct,
									 const char *input)
{
	if (input[0] == '[')
	{
		while (input[0] != ']')
			input++;
		input++;
	}
	return input;
}


static void update_buddies(PurpleConnection* gc){
	hon_account* hon = gc->proto_data;
	PurpleGroup* buddies = purple_find_group(HON_BUDDIES_GROUP);
	deserialized_element* buddy_data;

	GHashTableIter iter;
	
	if (!hon->buddies)
		return;

	if (!buddies){
		buddies = purple_group_new(HON_BUDDIES_GROUP);
		purple_blist_add_group(buddies, NULL);
	}
	g_hash_table_iter_init(&iter,hon->buddies);
	while (g_hash_table_iter_next(&iter,NULL,&buddy_data))
	{
		deserialized_element* buddyname = g_hash_table_lookup(buddy_data->array,"nickname");
		if (buddyname)
		{
			PurpleBuddy* buddy;
			guint32 id = atoi(((deserialized_element*)(g_hash_table_lookup(buddy_data->array,"buddy_id")))->string->str);
			if (!g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(id)))
			{
				g_hash_table_insert(hon->id2nick,GINT_TO_POINTER(id),g_strdup(buddyname->string->str));
			}
			
			buddy = purple_find_buddy(gc->account,buddyname->string->str);
			if (!buddy)
			{
				deserialized_element* clan_tag = g_hash_table_lookup(buddy_data->array,"clan_tag");
				if (!clan_tag || clan_tag->type != PHP_STRING)
				{
					buddy = purple_buddy_new(gc->account,buddyname->string->str,NULL);
				}
				else
				{
					clan_tag->string = g_string_prepend_c(clan_tag->string,'[');
					clan_tag->string = g_string_append_c(clan_tag->string,']');
					clan_tag->string = g_string_append(clan_tag->string,buddyname->string->str);
					buddy = purple_buddy_new(gc->account,buddyname->string->str,clan_tag->string->str);
				}
				purple_blist_add_buddy(buddy,NULL,buddies,NULL);
			}
		}
		
						
	}
}
static void update_clanmates(PurpleConnection* gc){
 	hon_account* hon = gc->proto_data;
	PurpleGroup* clanmates;
	deserialized_element* buddy_data;
	GHashTableIter iter;
	gchar* clanname;
	gchar* key;
	deserialized_element* clan_tag = g_hash_table_lookup(hon->clan_info,"tag");

	if (!hon->clanmates || !hon->clan_info)
		return;

	clanname =  ((deserialized_element*)(g_hash_table_lookup(hon->clan_info,"name")))->string->str;

	clanmates = purple_find_group(clanname);

	if (!clanmates){
		clanmates = purple_group_new(clanname);
		purple_blist_add_group(clanmates, NULL);
	}
	g_hash_table_iter_init(&iter,hon->clanmates);
	while (g_hash_table_iter_next(&iter,&key,&buddy_data))
	{
		deserialized_element* buddyname = g_hash_table_lookup(buddy_data->array,"nickname");
		if (buddyname)
		{
			PurpleBuddy* buddy;
			guint32 id = atoi(key);
			if (!g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(id)))
			{
				g_hash_table_insert(hon->id2nick,GINT_TO_POINTER(id),g_strdup(buddyname->string->str));
			}

			buddy = purple_find_buddy(gc->account,buddyname->string->str);
			if (!buddy)
			{
				if (!clan_tag || clan_tag->type != PHP_STRING)
				{
					buddy = purple_buddy_new(gc->account,buddyname->string->str,NULL);
				}
				else
				{
					GString* alias = g_string_new(clan_tag->string->str);
					alias = g_string_prepend_c(alias,'[');
					alias = g_string_append_c(alias,']');
					alias = g_string_append(alias,buddyname->string->str);
					buddy = purple_buddy_new(gc->account,buddyname->string->str,alias->str);
					g_string_free(alias,TRUE);
				}
				purple_blist_add_buddy(buddy,NULL,clanmates,NULL);
			}
		}


	}

}
/* 
* UI callbacks
*/
static void honprpl_input_user_info(PurplePluginAction *action)
{

}

/* this is set to the actions member of the PurplePluginInfo struct at the
* bottom.
*/
static GList *honprpl_actions(PurplePlugin *plugin, gpointer context)
{
	PurplePluginAction *action = purple_plugin_action_new(
		_("Set User Info..."), honprpl_input_user_info);
	return g_list_append(NULL, action);
}


/*
* prpl functions
*/
static const char *honprpl_list_icon(PurpleAccount *acct, PurpleBuddy *buddy)
{
	PurplePresence *presence;
	PurpleStatus *purple_status;
	guint32 status;
	guint32 flags;
	
	if (!buddy)
		return HON_PROTOCOL_ICON;
	presence = purple_buddy_get_presence(buddy);
	purple_status = purple_presence_get_active_status(presence);

	status = purple_status_get_attr_int(purple_status,HON_STATUS_ATTR);
	flags = purple_status_get_attr_int(purple_status,HON_FLAGS_ATTR);
	
	if (status == HON_STATUS_INGAME || status == HON_STATUS_INLOBBY)
	{
		purple_debug_info(HON_DEBUG_PREFIX, "%s icon: %s\n",
		buddy->name,HON_INGAME_EMBLEM);
		return HON_INGAME_EMBLEM;
	}

	return HON_PROTOCOL_ICON;
	
}

static char *honprpl_status_text(PurpleBuddy *buddy) {
	PurplePresence *presence = purple_buddy_get_presence(buddy);
	PurpleStatus *status = purple_presence_get_active_status(presence);
	const gchar* server = purple_status_get_attr_string( status, HON_SERVER_ATTR);
	const gchar* gamename = purple_status_get_attr_string(  status, HON_GAME_ATTR);
	GString* info = g_string_new(NULL);

	if (gamename)
	{
		/*info = g_string_append(info,"game: ");*/
		info = g_string_append(info,gamename);
		/*info = g_string_append_c(info,'\n');*/
	}
	/*
	if (server)
	{
		info = g_string_append(info,"server: ");
		info = g_string_append(info,server);
		info = g_string_append_c(info,'\n');
	}
	*/



	return g_string_free(info,FALSE);	
}
static const char* honprpl_list_emblem(PurpleBuddy *b)
{
	PurplePresence *presence = purple_buddy_get_presence(b);
	PurpleStatus *purple_status = purple_presence_get_active_status(presence);

	PurpleConnection *gc = purple_account_get_connection(purple_buddy_get_account(b));
	guint32 status = purple_status_get_attr_int(purple_status,HON_STATUS_ATTR);
	guint32 flags = purple_status_get_attr_int(purple_status,HON_FLAGS_ATTR);
	
	/*if(status == HON_STATUS_INGAME || status == HON_STATUS_INLOBBY)
		return HON_INGAME_EMBLEM;
	else*/ if (flags & HON_FLAGS_PREPURCHASED)
	{
		return HON_PREMIUM_EMBLEM;
	}



	return NULL;
}
static void honprpl_tooltip_text(PurpleBuddy *buddy,
								 PurpleNotifyUserInfo *info,
								 gboolean full) 
{
	PurplePresence *presence = purple_buddy_get_presence(buddy);
	PurpleConnection* gc = buddy->account->gc;
	hon_account* hon = NULL;
	PurpleStatus *status = purple_presence_get_active_status(presence);
	const gchar* server = purple_status_get_attr_string(status, HON_SERVER_ATTR);
	const gchar* gamename = purple_status_get_attr_string(status, HON_GAME_ATTR);
	guint32 matchid = purple_status_get_attr_int(status, HON_MATCHID_ATTR);
	guint32 buddy_id = purple_status_get_attr_int(status, HON_BUDDYID_ATTR);
	if (gc)
		hon = gc->proto_data;
	
	if (gamename)
	{
		purple_notify_user_info_add_pair(info, _("Game"), gamename);
	}
	if (server)
	{
		purple_notify_user_info_add_pair(info, _("Server"), server);
	}
	if (matchid > 0)
	{
		gchar* matchstring = g_strdup_printf("%d",matchid);
		purple_notify_user_info_add_pair(info, _("Match ID"), matchstring);
		g_free(matchstring);
	}
	if (hon && buddy_id)
	{
		deserialized_element* data;
		gchar* buddy_string_id = g_strdup_printf("%d",buddy_id);
		deserialized_element* clanmate = g_hash_table_lookup(hon->clanmates,buddy_string_id);
		if (clanmate && ((data = g_hash_table_lookup(clanmate->array,"rank")) != 0))
		{
			purple_notify_user_info_add_pair(info, _("Rank"),data->string->str);			
		}
		if (clanmate && ((data = g_hash_table_lookup(clanmate->array,"join_date")) != 0))
		{
			purple_notify_user_info_add_pair(info, _("Join date"),data->string->str);			
		}
		if (clanmate && ((data = g_hash_table_lookup(clanmate->array,"message")) != 0) && data->type != PHP_NULL)
		{
			purple_notify_user_info_add_pair(info, _("Message"),data->string->str);			
		}
		g_free(buddy_string_id);
	}


	purple_debug_info(HON_DEBUG_PREFIX, "game status: %s\n",
		gamename);

}






static GList *honprpl_status_types(PurpleAccount *acct)
{
	GList *types = NULL;
	PurpleStatusType *type;

	purple_debug_info(HON_DEBUG_PREFIX, "returning status types for %s: %s, %s,%s\n",
		acct->username,
		HON_STATUS_ONLINE_S, HON_STATUS_INGAME_S,HON_STATUS_OFFLINE_S);

	type = purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE,
		HON_STATUS_ONLINE_S, NULL, TRUE, TRUE, FALSE,
		HON_STATUS_ATTR, _("Status"), purple_value_new(PURPLE_TYPE_INT),
		HON_FLAGS_ATTR, _("Flags"), purple_value_new(PURPLE_TYPE_INT),
		HON_BUDDYID_ATTR, _("Buddy ID"), purple_value_new(PURPLE_TYPE_INT),
		NULL);
	types = g_list_prepend(types, type);

	type = purple_status_type_new_with_attrs(PURPLE_STATUS_UNAVAILABLE,
		HON_STATUS_INGAME_S, NULL, TRUE, FALSE, FALSE,
		HON_GAME_ATTR, _("Game"), purple_value_new(PURPLE_TYPE_STRING),
		HON_SERVER_ATTR, _("Server"), purple_value_new(PURPLE_TYPE_STRING),
		HON_MATCHID_ATTR, _("Match ID"), purple_value_new(PURPLE_TYPE_INT),
		HON_STATUS_ATTR, _("Status"), purple_value_new(PURPLE_TYPE_INT),
		HON_FLAGS_ATTR, _("Flags"), purple_value_new(PURPLE_TYPE_INT),
		HON_BUDDYID_ATTR, _("Buddy ID"), purple_value_new(PURPLE_TYPE_INT),
		NULL);
	types = g_list_prepend(types, type);

	type = purple_status_type_new_with_attrs(PURPLE_STATUS_OFFLINE,
		HON_STATUS_OFFLINE_S, NULL, TRUE, TRUE, FALSE,
		"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
		//NULL,NULL, //this produces crash :(
		NULL);
	types = g_list_prepend(types, type);

	return g_list_reverse(types);
}

static void blist_example_menu_item(PurpleBlistNode *node, gpointer userdata) {

	purple_debug_info(HON_DEBUG_PREFIX, "example menu item clicked on user %s\n",
		((PurpleBuddy *)node)->name);
#if 0
	purple_notify_info(NULL,  /* plugin handle or PurpleConnection */
		_("Primary title"),
		_("Secondary title"),
		_("This is the callback for the honprpl menu item."));
#endif
}

static GList *honprpl_blist_node_menu(PurpleBlistNode *node) {
	return NULL;

	purple_debug_info(HON_DEBUG_PREFIX, "providing buddy list context menu item\n");

	if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		PurpleMenuAction *action = purple_menu_action_new(
			_("honprpl example menu item"),
			PURPLE_CALLBACK(blist_example_menu_item),
			NULL,   /* userdata passed to the callback */
			NULL);  /* child menu items */
		return g_list_append(NULL, action);
	} else {
		return NULL;
	}
}

static GList *honprpl_chat_info(PurpleConnection *gc) {
	struct proto_chat_entry *pce; /* defined in prpl.h */

	purple_debug_info(HON_DEBUG_PREFIX, "returning chat setting 'room'\n");

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Chat _room");
	pce->identifier = "room";
	pce->required = TRUE;

	return g_list_append(NULL, pce);
}

static GHashTable *honprpl_chat_info_defaults(PurpleConnection *gc,
											  const char *room) 
{
	GHashTable *defaults;

	purple_debug_info(HON_DEBUG_PREFIX, "returning chat default setting "
		"'room' = 'HoN'\n");

	defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
	g_hash_table_insert(defaults, "room", g_strdup(room));
	return defaults;
}

static int do_write(int fd, void* buffer, int len){
	return write(fd,buffer,len);
}


/* Packet parsers */
static void pong(PurpleConnection *gc){
	gchar p = 2;
	hon_account* hon = gc->proto_data;
	do_write(hon->fd,&p,1);
}

static void initiall_statuses(PurpleConnection *gc,gchar* buffer){
	guint32 status,flags;
	hon_account* hon;
	guint32 id,count = read_guint32(buffer);
	hon = gc->proto_data;
	purple_debug_info(HON_DEBUG_PREFIX, "parsing status for %d buddies\n",count);
	while (count-- > 0)
	{
		gchar* nick,*gamename=NULL, *server=NULL,*status_id = HON_STATUS_ONLINE_S;
		
		id = read_guint32(buffer);
		status = *buffer++;
		flags = *buffer++;
		nick = g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(id));
		if (status == HON_STATUS_INLOBBY || status == HON_STATUS_INGAME)
		{
			server = read_string(buffer);
			status_id = HON_STATUS_INGAME_S;
		}
		if (status == HON_STATUS_INGAME)
		{
			gamename = read_string(buffer);
			gamename = hon_strip(gamename,TRUE);
		}
		if(!status)
			status_id = HON_STATUS_OFFLINE_S;
 		purple_debug_info(HON_DEBUG_PREFIX, "status for %s,flags:%d,status:%d,game:%s,server:%s\n",nick,flags,status,gamename,server);
		purple_prpl_got_user_status(gc->account, nick, status_id,
			HON_BUDDYID_ATTR , id,
			HON_STATUS_ATTR,status,HON_FLAGS_ATTR,flags,
			server ? HON_SERVER_ATTR : NULL,server,gamename ? HON_GAME_ATTR : NULL,gamename,NULL);
		
		g_free(gamename);
		
	}
}
static void user_status(PurpleConnection *gc,gchar* buffer){
	gchar* nick,*gamename=NULL, *server=NULL,*status_id = HON_STATUS_ONLINE_S;
	gchar* clan; // or channel?
	guint32 clanid;
	hon_account* hon = gc->proto_data;
	guint32 status;
	guint32 flags;
	guint32 matchid = 0;
	
	guint32 id = read_guint32(buffer);
	status = *buffer++;
	flags = *buffer++;
	nick = g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(id));
	/* TODO: figure this out */
	clanid = read_guint32(buffer);
	clan = read_string(buffer); // huh ?
	if (status == HON_STATUS_INLOBBY || status == HON_STATUS_INGAME)
	{
		server = read_string(buffer);
		status_id = HON_STATUS_INGAME_S;
	}
	if (status == HON_STATUS_INGAME)
	{
		gamename = read_string(buffer);
		gamename = hon_strip(gamename,TRUE);
		matchid = read_guint32(buffer);
	}
	if(!status)
		status_id = HON_STATUS_OFFLINE_S;
	purple_debug_info(HON_DEBUG_PREFIX, "status for %s,flags:%d,status:%d,game:%s,server:%s\nclanid:%d, clan?:%s matchid:%d\n"
		,nick,flags,status,gamename,server,clanid,clan,matchid);
	purple_prpl_got_user_status(gc->account, nick, status_id,
		HON_STATUS_ATTR,status,HON_FLAGS_ATTR,flags,
		HON_BUDDYID_ATTR , id,
		server ? HON_SERVER_ATTR : NULL,server,gamename ? HON_GAME_ATTR : NULL,gamename,
		matchid > 0 ? HON_MATCHID_ATTR : NULL, matchid,
		NULL);
	g_free(gamename);
}


static void got_pm(PurpleConnection *gc,gchar* buffer,guint8 is_whisper)
{
	hon_account* hon = gc->proto_data;
	PurpleMessageFlags receive_flags;
	gchar* message,*from_username = read_string(buffer);
	message = hon2html(buffer);
	if (from_username[0] == '[')
	{
		while (from_username[0] != ']')
			from_username++;
		from_username++;
	}
	if (is_whisper)
		 receive_flags = PURPLE_MESSAGE_WHISPER;
	else
		receive_flags = PURPLE_MESSAGE_RECV;
	serv_got_im(gc, from_username, message, receive_flags, time(NULL));
	g_free(message);
}
static void got_chanlist(PurpleConnection *gc,gchar* buffer){
	hon_account* hon = gc->proto_data;
	guint32 count = read_guint32(buffer);
	if (!hon->roomlist)
		return;
	while (count--)
	{
		PurpleRoomlistRoom *room;
		gchar* name,*colorname;
		guint32 id,participants;
		id = read_guint32(buffer);
		name = read_string(buffer);
		colorname = hon_strip(name,FALSE);
		participants = read_guint32(buffer);

		room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, colorname, NULL);

		purple_roomlist_room_add_field(hon->roomlist, room, GINT_TO_POINTER(id));
		purple_roomlist_room_add_field(hon->roomlist, room, name);
		purple_roomlist_room_add_field(hon->roomlist, room, GINT_TO_POINTER(participants));
		purple_roomlist_room_add(hon->roomlist, room);
		g_free(colorname);
	}
	purple_roomlist_set_in_progress(hon->roomlist, FALSE);
	purple_roomlist_unref(hon->roomlist);
	hon->roomlist = NULL;

}
static void entered_chat(PurpleConnection *gc,gchar* buffer)
{
	PurpleConversation *convo;
	hon_account* hon = gc->proto_data;
	guint8 unknown,flags;
	guint32 op_count,chat_id,count;
	guint32 purple_flags = 0;
	gchar* topic,*topic_raw;
	gchar* extra;
	GHashTable* ops = NULL;
	
	gchar* room = buffer;
	buffer += strlen(buffer) + 1;
	chat_id = read_guint32(buffer);
	unknown = *buffer++;
	topic_raw = read_string(buffer);
	topic = hon2html(topic_raw);
	topic_raw = hon_strip(topic_raw,TRUE);
	op_count = read_guint32(buffer);
	if (op_count != 0)
	{
		guint32 op_id,op_type;
		ops = g_hash_table_new(g_direct_hash,g_direct_equal);
		while (op_count--)
		{
			op_id = read_guint32(buffer);
			op_type = *buffer++;
			g_hash_table_insert(ops,GINT_TO_POINTER(op_id),GINT_TO_POINTER(op_type));
		}
	}
	count = read_guint32(buffer);
	convo = serv_got_joined_chat(gc, chat_id, room);
	purple_conv_chat_set_topic(PURPLE_CONV_CHAT(convo), NULL, topic_raw);
	
	while (count--)
	{
		guint32 account_id;
		guint8 status;
		gchar* nickname = read_string(buffer);
		account_id = read_guint32(buffer);
		status = *buffer++;
		flags = *buffer++;
		purple_debug_info(HON_DEBUG_PREFIX, "room participant: %s , id=%d,status=%d,flags=%d\n",
			nickname,account_id,status,flags);
		
		purple_flags = PURPLE_CBFLAGS_NONE;

		if (flags & HON_FLAGS_CHAT_FOUNDER)
		{
			purple_flags |= PURPLE_CBFLAGS_FOUNDER;
		}
		else if (flags & HON_FLAGS_CHAT_MOD)
			purple_flags |= PURPLE_CBFLAGS_OP;
			

		extra = nickname;
		nickname = honprpl_normalize(gc->account,nickname);
		purple_conv_chat_add_user(PURPLE_CONV_CHAT(convo), nickname, extra, purple_flags, FALSE);
		if (!g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(account_id)))
		{
			g_hash_table_insert(hon->id2nick,GINT_TO_POINTER(account_id),g_strdup(nickname));
		}

	}
	flags = 0;
	purple_flags = PURPLE_CBFLAGS_NONE;

	if(ops)
	{
		flags = GPOINTER_TO_INT(g_hash_table_lookup(ops,GINT_TO_POINTER(hon->self.account_id)));
		g_hash_table_destroy(ops);
	}
	if (flags & HON_FLAGS_CHAT_FOUNDER)
	{
		purple_flags |= PURPLE_CBFLAGS_FOUNDER;
	}
	else if (flags & HON_FLAGS_CHAT_MOD)
		purple_flags |= PURPLE_CBFLAGS_OP;

	purple_conv_chat_write(PURPLE_CONV_CHAT(convo), "", topic, PURPLE_MESSAGE_SYSTEM|PURPLE_MESSAGE_NO_LOG, time(NULL));
	g_free(topic);
	g_free(topic_raw);

	purple_conv_chat_add_user(PURPLE_CONV_CHAT(convo), hon->self.nickname, NULL,purple_flags , FALSE);
}
static void chat_msg(PurpleConnection *gc,gchar* buffer){
	hon_account *hon = gc->proto_data;
	guint32 account_id;
	guint32 chan_id;
	gchar* msg;
	gchar* sender;
	
	account_id = read_guint32(buffer);
	chan_id = read_guint32(buffer);
	msg = hon2html(buffer);
	sender = g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(account_id));
	serv_got_chat_in(gc,chan_id,sender? sender : "unknown user" ,PURPLE_MESSAGE_RECV,msg,time(NULL));
	g_free(msg);
}
static void joined_chat(PurpleConnection *gc,gchar* buffer){
	hon_account* hon = gc->proto_data;
	guint32 account_id;
	guint32 chan_id,purple_flags = PURPLE_CBFLAGS_NONE;
	PurpleConversation* conv;
	guint8 status,flags;
	gchar* extra;
	gchar* nick = read_string(buffer);
	account_id = read_guint32(buffer);
	chan_id = read_guint32(buffer);
	conv = purple_find_chat(gc,chan_id);

	/* TODO: there are common status and flags after this! */
	extra = nick;
	nick = honprpl_normalize(gc->account,nick);
	status = *buffer++;
	flags = *buffer++;

	if (flags & HON_FLAGS_CHAT_FOUNDER)
	{
		purple_flags |= PURPLE_CBFLAGS_FOUNDER;
	}
	else if (flags & HON_FLAGS_CHAT_MOD)
		purple_flags |= PURPLE_CBFLAGS_OP;


	if (conv)
	{
		purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv),nick,extra,purple_flags,TRUE);
	}
	if (!g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(account_id)))
	{
		g_hash_table_insert(hon->id2nick,GINT_TO_POINTER(account_id),g_strdup(nick));
	}
}
static void leaved_chat(PurpleConnection *gc,gchar* buffer){
	hon_account* hon = gc->proto_data;
	guint32 account_id;
	guint32 chan_id;
	gchar* nick;
	PurpleConversation* conv;
	account_id = read_guint32(buffer);
	chan_id = read_guint32(buffer);
	nick = g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(account_id));
	conv = purple_find_chat(gc,chan_id);
	if (conv && nick)
	{
		purple_conv_chat_remove_user(PURPLE_CONV_CHAT(conv),nick,"");
	}
}
static void got_clan_whisper(PurpleConnection *gc,gchar* buffer){
	hon_account* hon = gc->proto_data;
	guint32 buddy_id;
	gchar* message,*user;
	PurpleConversation* clanConv;
	GString* clan_chat_name;
	buddy_id = read_guint32(buffer);
	message = hon2html(buffer);
	clan_chat_name = g_string_new("Clan ");
	clan_chat_name = g_string_append(clan_chat_name,hon->self.clan_name);
	clanConv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,clan_chat_name->str,gc->account);
	g_string_free(clan_chat_name,TRUE);
	if (clanConv)
	{
		user = g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(buddy_id));
		purple_conv_chat_write(PURPLE_CONV_CHAT(clanConv), user,message, PURPLE_MESSAGE_WHISPER, time(NULL));
	}
	g_free(message);
}
static void got_userinfo(PurpleConnection* gc,gchar* buffer,guint8 packet_id){
	hon_account* hon = gc->proto_data;
	/* TODO: this is not right .. conversation could be closed already */
	gchar* message = NULL;
	gchar* user = read_string(buffer);
	if (!hon->whois_conv)
		return;

	switch (packet_id){
	case 0x2b:
		message = g_strdup_printf(_("Cannot find user %s"),user);
		break;
	case 0x2c:
		message = g_strdup_printf(_("User %s is offline, last seen %s"),user,buffer);
		break;
	case 0x2d:
		{
			GString* msg = g_string_new(NULL);
			guint32 chan_count = read_guint32(buffer);
			if (chan_count > 0)
				g_string_printf(msg,_("User %s is online and in channels: "),user);
			else
				g_string_printf(msg,_("User %s is online."),user);
			while (chan_count--)
			{
				msg = g_string_append(msg,buffer);
				buffer += strlen(buffer) + 1;
				if (chan_count == 0)
					msg = g_string_append(msg,".");
				else
					msg = g_string_append(msg,", ");
			}
			message = g_string_free(msg,FALSE);
		}
		break;
	case 0x2e:
		message = g_strdup_printf(_("User %s is ingame, game name: %s, game time: %s"),user,buffer,buffer + (strlen(buffer) + 1));
		break;
	}

	purple_conversation_write(hon->whois_conv, "",message, PURPLE_MESSAGE_SYSTEM|PURPLE_MESSAGE_NO_LOG, time(NULL));
	g_free(message);
	hon->whois_conv = NULL;
}
static void parse_packet(PurpleConnection *gc, gchar* buffer, int packet_length){
	guint8 packet_id = *buffer++;
	GString* hexdump;
#if 0
	hexdump = g_string_new(NULL);
	hexdump_g_string_append(hexdump,"",buffer,packet_length - 1);
	purple_debug_info(HON_DEBUG_PREFIX, "packet:\nid:%X(%d)\nlength:%d\ndata:\n%s\n",packet_id,packet_id,packet_length, hexdump->str);
	g_string_free(hexdump,TRUE);
#endif
	switch (packet_id)
	{
	case 0x00: /* logged on ! */
		purple_connection_update_progress(gc, _("Connected"),
			3,   /* which connection step this is */
			4);  /* total number of steps */
		purple_connection_set_state(gc, PURPLE_CONNECTED);
		break;
	case 0x01:
		pong(gc);
		purple_debug_info(HON_DEBUG_PREFIX, "server ping, sending pong\n");
		break;
	case 0x03:
		chat_msg(gc,buffer);
		break;
	case 0x04:
#ifdef _DEBUG
		hexdump = g_string_new(NULL);
		hexdump_g_string_append(hexdump,"",buffer,packet_length - 1);
		purple_debug_info(HON_DEBUG_PREFIX, "channel join packet:\nid:%X(%d)\nlength:%d\ndata:\n%s\n",packet_id,packet_id,packet_length, hexdump->str);
		g_string_free(hexdump,TRUE);
#endif
		entered_chat(gc,buffer);
		break;
	case 0x05:
		joined_chat(gc,buffer);
		break;
	case 0x06:
		leaved_chat(gc,buffer);
		break;
	case 0x08:
		got_pm(gc,buffer,TRUE);
		break;
	case 0x0B:
		initiall_statuses(gc,buffer);
		break;
	case 0x0C:
#ifdef _DEBUG
		hexdump = g_string_new(NULL);
		hexdump_g_string_append(hexdump,"",buffer,packet_length - 1);
		purple_debug_info(HON_DEBUG_PREFIX, "user status packet:\nid:%X(%d)\nlength:%d\ndata:\n%s\n",packet_id,packet_id,packet_length, hexdump->str);
		g_string_free(hexdump,TRUE);
#endif
		user_status(gc,buffer);
		break;
	case 0x13:
		got_clan_whisper(gc,buffer);
		break;
	case 0x1C:
		got_pm(gc,buffer,FALSE);
		break;
	case 0x1F:
		got_chanlist(gc,buffer);
		break;
	case 0x2b:
	case 0x2c:
	case 0x2d:
	case 0x2e:
		got_userinfo(gc,buffer,packet_id);
	default:
		hexdump = g_string_new(NULL);
		hexdump_g_string_append(hexdump,"",buffer,packet_length - 1);
		purple_debug_info(HON_DEBUG_PREFIX, "unknown packet:\nid:%X(%d)\nlength:%d\ndata:\n%s\n",packet_id,packet_id,packet_length, hexdump->str);
		g_string_free(hexdump,TRUE);
		break;
	}
}
/*
	called from read_cb, attempts to read needed data from sock and
    pass it to the parser, passing back the return code from the read
    call for handling in read_cb 
*/
static int read_recv(PurpleConnection *gc, int sock) {
	int packet_length,len;
	

	len = read(sock,&packet_length,4);
	if(len == 4 && packet_length > 0) {
		gchar* buffer = g_malloc0(packet_length);
		while (recv(sock, buffer, packet_length,MSG_PEEK) < packet_length && (errno == 0 || errno == EAGAIN));
		len = read(sock, buffer, packet_length);
		if (len == packet_length)
		{
			parse_packet(gc,buffer,packet_length);
		} 
		else
		{
			len = -1;
		}
		g_free(buffer);
	}
	return len;
}


/** callback triggered from purple_input_add, watches the socked for
    available data to be processed by the session */
static void read_cb(gpointer data, gint source, PurpleInputCondition cond) {
	PurpleConnection *gc = data;
	hon_account *hon = gc->proto_data;

	int ret = 0, err = 0;

	g_return_if_fail(hon != NULL);

	ret = read_recv(gc, hon->fd);

	/* normal operation ends here */
	if(ret > 0) return;

	/* fetch the global error value */
	err = errno;

	/* read problem occurred if we're here, so we'll need to take care of
	it and clean up internal state */

	if(hon->fd) {
		close(hon->fd);
		hon->fd = 0;
	}

	if(gc->inpa) {
		purple_input_remove(gc->inpa);
		gc->inpa = 0;
	}

	if(! ret) {

		purple_connection_error_reason(gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Server closed the connection"));

	} else if(ret < 0) {
		const gchar *err_str = g_strerror(err);
		char *msg = NULL;



		msg = g_strdup_printf(_("Lost connection with server: %s"), err_str);
		purple_connection_error_reason(gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			msg);
		g_free(msg);
	}
}


static void hon_login_cb(gpointer data, gint source, const gchar *error_message)
{

	PurpleConnection *gc = data;
	hon_account *hon = gc->proto_data;
	GByteArray* buff;
	int len;
	int on = 1;
	if (source < 0) {
		gchar *tmp = g_strdup_printf(_("Unable to connect: %s"),
			error_message);
		purple_connection_error_reason (gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR, tmp);
		g_free(tmp);
		return;
	}

	hon->fd = source;
#ifdef _WIN32
	len = setsockopt(source,IPPROTO_TCP, TCP_NODELAY, &on, sizeof (on));
#else
	setsockopt(source,SOL_TCP, TCP_NODELAY, &on, sizeof (on));
#endif

	buff = g_byte_array_sized_new(1);
	buff->data[0] = 0xFF;
	buff->len = 1;
	buff = g_byte_array_append(buff,(const guint8*) &hon->self.account_id,4);
	buff = g_byte_array_append(buff,hon->cookie,strlen(hon->cookie) + 1); /* needs to have 0x00 at the end */
	len = 2;
	buff = g_byte_array_append(buff,(const guint8*) &len,4);
	len = do_write(hon->fd,buff->data,buff->len);

	if(len == buff->len){
		purple_connection_update_progress(gc, _("Authenticating"),
			2,   /* which connection step this is */
			4);  /* total number of steps */
		gc->inpa = purple_input_add(source, PURPLE_INPUT_READ,
			read_cb, gc);
	}
	else
	{
		gchar *tmp = g_strdup_printf(_("Unable to connect: %s"),
			error_message);
		purple_connection_error_reason (gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR, tmp);
		g_free(tmp);

	}

	g_byte_array_free(buff,TRUE);
}

static void start_hon_session_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message){
	PurpleConnection *gc = user_data;
	hon_account* hon = gc->proto_data;
	deserialized_element* account_data= hon->account_data;
	if (account_data)
	{
		destroy_php_element(account_data);
	}

	if(!(url_text)){
		purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,error_message);
		purple_connection_set_state(gc, PURPLE_DISCONNECTED);
	}
	else{
		purple_debug_info(HON_DEBUG_PREFIX, "data from masterserver: \n%s\n",
		url_text);

		account_data = deserialize_php(&url_text,strlen(url_text));
		if (account_data->type != PHP_ARRAY)
		{
			purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_OTHER_ERROR,_("Bad data received from server"));
			purple_connection_set_state(gc, PURPLE_DISCONNECTED);
		}
		else{
			deserialized_element* res = g_hash_table_lookup(account_data->array,"0");
			if (!res || !res->int_val)
			{
				destroy_php_element(res);
				res = g_hash_table_lookup(account_data->array,"auth");
				if (res && res->type == PHP_STRING)
				{
					purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_OTHER_ERROR,res->string->str);
				}
				else
					purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_OTHER_ERROR,_("Unknown error"));
				purple_connection_set_state(gc, PURPLE_DISCONNECTED);
			}
			else{
				deserialized_element* tmp;
				gchar* account_id = ((deserialized_element*)(g_hash_table_lookup(account_data->array,"account_id")))->string->str;
				/* TODO: check for errors */
				hon->cookie = ((deserialized_element*)(g_hash_table_lookup(account_data->array,"cookie")))->string->str;
				hon->self.nickname = ((deserialized_element*)(g_hash_table_lookup(account_data->array,"nickname")))->string->str;
				hon->self.account_id = atoi(((deserialized_element*)(g_hash_table_lookup(account_data->array,"account_id")))->string->str);
				
				tmp = ((deserialized_element*)(g_hash_table_lookup(account_data->array,"buddy_list")));
				if (tmp){
					tmp = g_hash_table_lookup(tmp->array,account_id);
					hon->buddies = tmp ? tmp->array : NULL;
				}

				tmp = ((deserialized_element*)(g_hash_table_lookup(account_data->array,"banned_list")));
				if (tmp){
					tmp = g_hash_table_lookup(tmp->array,account_id);
					hon->banned = tmp ? tmp->array : NULL;
				}

				tmp = ((deserialized_element*)(g_hash_table_lookup(account_data->array,"ignored_list")));
				if (tmp){
					tmp = g_hash_table_lookup(tmp->array,account_id);
					hon->ignores = tmp ? tmp->array : NULL;
				}

				hon->clanmates = ((deserialized_element*)(g_hash_table_lookup(account_data->array,"clan_roster")))->array;
				if (g_hash_table_lookup(hon->clanmates,"error"))
				{
					hon->clanmates = NULL;
				}

				hon->clan_info = ((deserialized_element*)(g_hash_table_lookup(account_data->array,"clan_member_info")))->array;
				if (g_hash_table_lookup(hon->clan_info,"error"))
					hon->clan_info = NULL;
				if (hon->clan_info)
				{
					hon->self.clan_tag = ((deserialized_element*)g_hash_table_lookup(hon->clan_info,"tag"))->string->str;
					hon->self.clan_name = ((deserialized_element*)g_hash_table_lookup(hon->clan_info,"name"))->string->str;
				}
				
				
				

				update_buddies(gc);
				update_clanmates(gc);


				purple_connection_update_progress(gc, _("Connecting"),
					1,   /* which connection step this is */
					4);  /* total number of steps */


 				if (purple_proxy_connect(gc, gc->account, 
 				                         ((deserialized_element*)(g_hash_table_lookup(account_data->array,"chat_url")))->string->str,
 				                         HON_CHAT_PORT,
 					hon_login_cb, gc) == NULL)
				{
					purple_connection_error_reason (gc,
						PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
						_("Unable to connect"));
					return;
				}
			}


		}		


	}
	hon->account_data = account_data;
}

static void honprpl_login(PurpleAccount *acct)
{
	PurpleUtilFetchUrlData *url_data;
	PurpleConnection *gc = purple_account_get_connection(acct);
	hon_account* honacc;
	GString* request_url = g_string_new(NULL);
	GString* password_md5;

	if ( !purple_account_get_bool(acct,IS_MD5_OPTION,TRUE)){
		password_md5 = get_md5_string(acct->password);
	}
	else
		password_md5 = g_string_new(acct->password);


	g_string_printf(request_url,"%s%s?f=auth&login=%s&password=%s",
		purple_account_get_string(acct, "masterserver", HON_DEFAULT_MASTER_SERVER),
		HON_CLIENT_REQUESTER,
		acct->username,password_md5->str);


	purple_debug_info(HON_DEBUG_PREFIX, "logging in %s\n", acct->username);

	purple_connection_update_progress(gc, _("Getting COOKIE"),
		0,   /* which connection step this is */
		2);  /* total number of steps */

	gc->proto_data = honacc = g_new0(hon_account, 1);

	url_data = purple_util_fetch_url_request_len_with_account(gc->account,request_url->str,TRUE,NULL,FALSE,NULL,FALSE,-1,start_hon_session_cb,gc);

	g_string_free(password_md5,TRUE);
	g_string_free(request_url,TRUE);  

	honacc->account_data = NULL;
	honacc->id2nick = g_hash_table_new_full(g_direct_hash,g_direct_equal,NULL,g_free);
}

static void honprpl_close(PurpleConnection *gc)
{
	hon_account* hon = gc->proto_data;
	close(hon->fd);

	if (gc->inpa)
		purple_input_remove(gc->inpa);
	g_hash_table_destroy(hon->id2nick);
	destroy_php_element(hon->account_data);
	g_free(hon);
	gc->proto_data = NULL;
}

static int honprpl_send_im(PurpleConnection *gc, const char *who,
						   const char *message, PurpleMessageFlags flags)
{
	const char *from_username = gc->account->username;
	hon_account* hon = gc->proto_data;
	GByteArray* buff = g_byte_array_new();
	guint8 packet_id = 0x1C;
#ifdef _DEBUG
	purple_debug_info(HON_DEBUG_PREFIX, "sending message from %s to %s: %s\n",
		from_username, who, message);
#endif
	buff = g_byte_array_append(buff,&packet_id,1);
	buff = g_byte_array_append(buff,who,strlen(who) + 1);
	buff = g_byte_array_append(buff,message,strlen(message) + 1);

	do_write(hon->fd,buff->data,buff->len);

	g_byte_array_free(buff,TRUE);

	return 1;
}
#define fetch_info_row(x,y) 	\
			if ((info_row = g_hash_table_lookup(needed_data->array,x)) != NULL\
				&& info_row->type == PHP_STRING\
				)\
				purple_notify_user_info_add_pair(info_tmp->info, _(y), info_row->string->str);


static void honpurple_info_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message){
	honprpl_info_tmp* info_tmp = user_data;
	PurpleConnection *gc = info_tmp->gc;
	hon_account* hon = gc->proto_data;
	deserialized_element* data = NULL;
	deserialized_element* needed_data = NULL;
	deserialized_element* info_row = NULL;
	if(
		!url_text
		|| (data = deserialize_php(&url_text,strlen(url_text)))->type != PHP_ARRAY
		)
	{
		purple_notify_user_info_add_pair(info_tmp->info, _("Error"), _("error retrieving account stats"));
	}
	else
	{
		if (
			(needed_data = g_hash_table_lookup(data->array,"clan_info"))
			&& needed_data->type == PHP_ARRAY
			&& (needed_data = g_hash_table_lookup(needed_data->array,info_tmp->account_id))
			&& needed_data->type == PHP_ARRAY
			)
		{
			fetch_info_row("name","Clan Name")
			fetch_info_row("tag","Clan Tag")
			fetch_info_row("rank","Clan Rank")
		}

		if (
			(needed_data = g_hash_table_lookup(data->array,"all_stats"))
			&& needed_data->type == PHP_ARRAY
			&& (needed_data = g_hash_table_lookup(needed_data->array,info_tmp->account_id))
			&& needed_data->type == PHP_ARRAY
			)
		{
			fetch_info_row("level","Level")
			fetch_info_row("acc_exp","Current XP")
			fetch_info_row("acc_wins","Wins")
			fetch_info_row("acc_losses","Losses")
			fetch_info_row("acc_games_played","Games Played")
			fetch_info_row("acc_concedes","Concedes")
			fetch_info_row("acc_concedevotes","Concede Votes")
			fetch_info_row("acc_kicked","Kicks")
			fetch_info_row("acc_discos","Disconnects")
			fetch_info_row("acc_pub_skill","Current PUB Skill")
			fetch_info_row("acc_herokills","Kills")
			fetch_info_row("acc_deaths","Deaths")
			fetch_info_row("acc_buybacks","Hero Buy Backs")
			fetch_info_row("acc_herodmg","Damage to Heroes")
			fetch_info_row("acc_heroexp","XP From Hero Kills")
			fetch_info_row("acc_heroassists","Hero Kill Assists")
			fetch_info_row("acc_denies","Total Kills Denied")
			fetch_info_row("acc_exp_denied","Total XP Denied")
			fetch_info_row("acc_teamcreepkills","Enemy Creep Kills")
			fetch_info_row("acc_teamcreepdmg","Enemy Creep Damage")
			fetch_info_row("acc_teamcreepexp","Enemy Creep XP")
			fetch_info_row("acc_neutralcreepkills","Neutral Creep Kills")
			fetch_info_row("acc_neutralcreepdmg","Neutral Creep Damage")
			fetch_info_row("acc_neutralcreepexp","Neutral Creep XP")
			fetch_info_row("acc_razed","Buildings Razed")
			fetch_info_row("acc_bdmg","Total Building Damage")
			fetch_info_row("acc_bdmgexp","XP from buidings")
			fetch_info_row("acc_gold","Total Gold Farmed")
			fetch_info_row("acc_herokillsgold","Gold From Hero Kills")
			fetch_info_row("acc_teamcreepgold","Gold From Enemy Creeps")
			fetch_info_row("acc_neutralcreepgold","Gold From Neutral Creeps")
			fetch_info_row("acc_bgold","Gold From Buildings")
			fetch_info_row("acc_goldlost2death","Gold Lost To Death")
			fetch_info_row("acc_gold_spent","Total Gold Spent")
			fetch_info_row("acc_secs","Total Seconds Played")
			fetch_info_row("acc_secs_dead","Total Seconds Dead")
			fetch_info_row("acc_actions","Total Actions performed")
			fetch_info_row("acc_consumables","Total Consumables Used")
			fetch_info_row("acc_wards","Total Wards Used")
			fetch_info_row("create_date","Account Created")
			fetch_info_row("last_login","Last Login")
			fetch_info_row("last_activity","Last Activity")
	
			/* unused:
			minXP: 1741932
			nickname: dancercod
			APEM: 0
			maxXP: 2177414
			account_id: 78959
			AP: 0
			acc_avg_score: 0.00
			AR: 0
			AREM: 0
			*/
		}

	}
	purple_notify_userinfo(gc,        /* connection the buddy info came through */
		info_tmp->username,  /* buddy's username */
		info_tmp->info,      /* body */
		destroy_php_element,      /* callback called when dialog closed */
		data);     /* userdata for callback */

	g_free(info_tmp->username);
	g_free(info_tmp->account_id);
	g_free(info_tmp);
}
#undef fetch_info_row
static void honpurple_info_nick2id_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message){
	honprpl_info_tmp* info_tmp = user_data;
	PurpleConnection *gc = info_tmp->gc;
	hon_account* hon = gc->proto_data;
	deserialized_element* data;
	deserialized_element* data2;

	if(
		!url_text
		|| (data = deserialize_php(&url_text,strlen(url_text)))->type != PHP_ARRAY
		|| ((data2 = g_hash_table_lookup(data->array,info_tmp->username)) == 0)
		|| data2->type != PHP_STRING
		){
		purple_notify_user_info_add_pair(info_tmp->info, _("Error"), _("error retrieving account id"));
		purple_notify_userinfo(gc,        /* connection the buddy info came through */
			info_tmp->username,  /* buddy's username */
			info_tmp->info,      /* body */
			NULL,      /* callback called when dialog closed */
			NULL);     /* userdata for callback */
		g_free(info_tmp->username);
		g_free(info_tmp);
	}
	else
	{
		gchar* url;
		info_tmp->account_id = g_strdup(data2->string->str);
		url = g_strdup_printf("%s%s?f=get_all_stats&account_id[0]=%s",
			purple_account_get_string(gc->account, "masterserver", HON_DEFAULT_MASTER_SERVER),
			HON_CLIENT_REQUESTER,info_tmp->account_id
			);
		purple_util_fetch_url_request_len_with_account(gc->account,url,TRUE,NULL,FALSE,NULL,FALSE,-1,honpurple_info_cb,info_tmp);
		g_free(url);
	}
	if (data)
		destroy_php_element(data);
}
static void honprpl_get_info(PurpleConnection *gc, const char *username) {
	hon_account* hon = gc->proto_data;
	guint32 buddy_id = 0;
	gchar* url;
	honprpl_info_tmp* hon_info = g_new0(honprpl_info_tmp,sizeof(honprpl_info_tmp));

	hon_info->info = purple_notify_user_info_new();
	hon_info->gc = gc;
	hon_info->username = g_strdup(username);

	purple_debug_info(HON_DEBUG_PREFIX, "Fetching %s's user info for %s\n", username,
		gc->account->username);
	url = g_strdup_printf("%s%s?f=nick2id&nickname[]=%s",
		purple_account_get_string(gc->account, "masterserver", HON_DEFAULT_MASTER_SERVER),
		HON_CLIENT_REQUESTER,username
		);
	purple_util_fetch_url_request_len_with_account(gc->account,url,TRUE,NULL,FALSE,NULL,FALSE,-1,honpurple_info_nick2id_cb,hon_info);
	g_free(url);
}



static void honprpl_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
							  PurpleGroup *group)
{
	int a = 0;
#if 0
	const char *username = gc->account->username;
	PurpleConnection *buddy_gc = NULL;//get_honprpl_gc(buddy->name);

	purple_debug_info(HON_DEBUG_PREFIX, "adding %s to %s's buddy list\n", buddy->name,
		username);

	if (buddy_gc) {
		PurpleAccount *buddy_acct = buddy_gc->account;

		discover_status(gc, buddy_gc, NULL);

		if (purple_find_buddy(buddy_acct, username)) {
			purple_debug_info(HON_DEBUG_PREFIX, "%s is already on %s's buddy list\n",
				username, buddy->name);
		} else {
			purple_debug_info(HON_DEBUG_PREFIX, "asking %s if they want to add %s\n",
				buddy->name, username);
			purple_account_request_add(buddy_acct,
				username,
				NULL,   /* local account id (rarely used) */
				NULL,   /* alias */
				NULL);  /* message */
		}
	}
#endif
}

static void honprpl_add_buddies(PurpleConnection *gc, GList *buddies,
								GList *groups)
{
	GList *buddy = buddies;
	GList *group = groups;

	purple_debug_info(HON_DEBUG_PREFIX, "adding multiple buddies\n");

	while (buddy && group) {
		honprpl_add_buddy(gc, (PurpleBuddy *)buddy->data, (PurpleGroup *)group->data);
		buddy = g_list_next(buddy);
		group = g_list_next(group);
	}
}

static void honprpl_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
								 PurpleGroup *group)
{
	purple_debug_info(HON_DEBUG_PREFIX, "removing %s from %s's buddy list\n",
		buddy->name, gc->account->username);
}

static void honprpl_remove_buddies(PurpleConnection *gc, GList *buddies,
								   GList *groups) 
{
	GList *buddy = buddies;
	GList *group = groups;

	purple_debug_info(HON_DEBUG_PREFIX, "removing multiple buddies\n");

	while (buddy && group) {
		honprpl_remove_buddy(gc, (PurpleBuddy *)buddy->data,
			(PurpleGroup *)group->data);
		buddy = g_list_next(buddy);
		group = g_list_next(group);
	}
}

static void honprpl_join_chat(PurpleConnection *gc, GHashTable *components) {
	hon_account* hon = gc->proto_data;
	const char *room = g_hash_table_lookup(components, "room");
	GByteArray* buf = g_byte_array_new();
	guint8 packet_id = 0x1e;
	buf = g_byte_array_append(buf,&packet_id,1);
	buf = g_byte_array_append(buf,room,strlen(room) + 1);
	do_write(hon->fd,buf->data,buf->len);
	g_byte_array_free(buf,TRUE);
}

static char *honprpl_get_chat_name(GHashTable *components) {
	const char *room = g_hash_table_lookup(components, "room");
	purple_debug_info(HON_DEBUG_PREFIX, "reporting chat room name '%s'\n", room);
	return g_strdup(room);
}

static void honprpl_chat_leave(PurpleConnection *gc, int id) {
	hon_account* hon = gc->proto_data;
	guint8 packet_id = 0x22;
	GByteArray* buffer = g_byte_array_new();
	PurpleConversation* conv = purple_find_chat(gc,id);
	buffer = g_byte_array_append(buffer,&packet_id,1);
	buffer = g_byte_array_append(buffer,conv->name,strlen(conv->name) + 1);
	do_write(hon->fd,buffer->data,buffer->len);
	g_byte_array_free(buffer,TRUE);
}


static int honprpl_chat_send(PurpleConnection *gc, int id, const char *message,
							 PurpleMessageFlags flags) 
{
	GByteArray* buffer = g_byte_array_new();
	hon_account* hon = gc->proto_data;
	gchar* coloredmessage = hon2html(message);
	guint8 packet_id = 0x03;
	buffer = g_byte_array_append(buffer,&packet_id,1);
	buffer = g_byte_array_append(buffer,message,strlen(message)+1);
	buffer = g_byte_array_append(buffer,(guint8*)&id,4);
	do_write(hon->fd,buffer->data,buffer->len);
	g_byte_array_free(buffer,TRUE);
	serv_got_chat_in(gc,id,hon->self.nickname,PURPLE_MESSAGE_SEND,coloredmessage,time(NULL));
	g_free(coloredmessage);
	return 0;
}



static void honprpl_set_chat_topic(PurpleConnection *gc, int id,
								   const char *topic)
{
}

static PurpleRoomlist *honprpl_roomlist_get_list(PurpleConnection *gc) {
	
	PurpleRoomlist *roomlist = purple_roomlist_new(gc->account);
	hon_account* hon = gc->proto_data;
	GList *fields = NULL;
	PurpleRoomlistField *field;
	guint8 packet_id = 0x1F;


	/* set up the room list */
	field = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_INT, _("Id"), "Id", FALSE);
	fields = g_list_append(fields, field);

	field = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "room",
		"room", TRUE /* hidden */);
	fields = g_list_append(fields, field);

	field = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_INT, _("participants"), "participants", FALSE);
	fields = g_list_append(fields, field);

	purple_roomlist_set_fields(roomlist, fields);


	purple_roomlist_set_in_progress(roomlist, TRUE);

	do_write(hon->fd,&packet_id,1);
	if(hon->roomlist)
		purple_roomlist_unref(hon->roomlist);

	hon->roomlist = roomlist;

	return roomlist;
}

static void honprpl_roomlist_cancel(PurpleRoomlist *list) {
	PurpleConnection *gc;
	hon_account *hon;

	gc = purple_account_get_connection(list->account);
	hon = gc->proto_data;

	purple_roomlist_set_in_progress(list, FALSE);

	if (hon->roomlist == list) {
		hon->roomlist = NULL;
		purple_roomlist_unref(list);
	}

}

static PurpleCmdRet send_whisper(PurpleConversation *conv, const gchar *cmd,
								 gchar **args, gchar **error, void *userdata) 
{
	const char *to_username;
	const char *message;
	gchar* colored_msg;
	hon_account* hon = conv->account->gc->proto_data;
	GByteArray* buffer;
	guint8 packet_id = 0x08;
	guint32 whisper_type = GPOINTER_TO_INT(userdata);

	/* parse args */
	if (whisper_type == 0)
	{
		to_username = args[0];
		message = args[1];
	}
	else
	{
		to_username = conv->name;
		message = args[0];
	}
	

	if (!to_username || strlen(to_username) == 0) {
		*error = g_strdup(_("Whisper is missing recipient."));
		return PURPLE_CMD_RET_FAILED;
	} else if (!message || strlen(message) == 0) {
		*error = g_strdup(_("Whisper is missing message."));
		return PURPLE_CMD_RET_FAILED;
	}
    buffer = g_byte_array_new();
	buffer = g_byte_array_append(buffer,&packet_id,1);
	buffer = g_byte_array_append(buffer,to_username,strlen(to_username) + 1);
	buffer = g_byte_array_append(buffer,message,strlen(message) + 1);

	purple_debug_info("honprpl", "%s whispers to %s in chat room %s: %s\n",
		hon->self.nickname, to_username, conv->name, message);
	
	do_write(hon->fd,buffer->data,buffer->len);
	g_byte_array_free(buffer,TRUE);

	colored_msg = hon2html(message);
	purple_conversation_write(conv,hon->self.nickname, colored_msg, PURPLE_MESSAGE_SEND, time(NULL));
	g_free(colored_msg);

	return PURPLE_CMD_RET_OK;
	
}

static PurpleCmdRet clan_commands(PurpleConversation *conv, const gchar *cmd,
								 gchar **args, gchar **error, void *userdata) 
{
	
	const char *command = args[0];
	hon_account* hon = conv->account->gc->proto_data;
	GByteArray* buffer;
	GString* msg;
	guint8 packet_id;

	if (!hon->clan_info)
	{
		*error = g_strdup(_("Not in clan"));
		return PURPLE_CMD_RET_FAILED;
	}

	if (!g_strcmp0(command,"invite"))
	{
		deserialized_element* rank = g_hash_table_lookup(hon->clan_info,"rank");
		if (rank && !g_ascii_strncasecmp(rank->string->str,"member",6))
		{
			*error = g_strdup(_("Only clan founder or officer can invite"));
			return PURPLE_CMD_RET_FAILED;
		}
		packet_id = 0x47;
		buffer = g_byte_array_new();
		buffer = g_byte_array_append(buffer,&packet_id,1);
		buffer = g_byte_array_append(buffer,args[1],strlen(args[1])+1);
		do_write(hon->fd,buffer->data,buffer->len);
		msg = g_string_new(NULL);
		g_string_printf(msg,_("Invited %s to clan"),args[1]);
		purple_conversation_write(conv, "",msg->str , PURPLE_MESSAGE_SYSTEM|PURPLE_MESSAGE_NO_LOG, time(NULL));
		g_string_free(msg,TRUE);
		g_byte_array_free(buffer,TRUE);
		return PURPLE_CMD_RET_OK;
	}
	else if (!g_strcmp0(command,"m") || !g_strcmp0(command,"message"))
	{
		PurpleConversation* clanConv;
		gchar* message = hon2html(args[1]);
		GString* clan_chat_name = g_string_new("Clan ");
		clan_chat_name = g_string_append(clan_chat_name,hon->self.clan_name);
		clanConv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,clan_chat_name->str,conv->account);
		g_string_free(clan_chat_name,TRUE);
		if (clanConv)
		{
			purple_conv_chat_write(PURPLE_CONV_CHAT(clanConv), hon->self.nickname,message, PURPLE_MESSAGE_WHISPER, time(NULL));
		}
		g_free(message);

		packet_id = 0x13;
		buffer = g_byte_array_new();
		buffer = g_byte_array_append(buffer,&packet_id,1);
		buffer = g_byte_array_append(buffer,args[1],strlen(args[1])+1);
		do_write(hon->fd,buffer->data,buffer->len);
		g_byte_array_free(buffer,TRUE);
		return PURPLE_CMD_RET_OK;
	}

#if 0
	else
	{
		guint32 matchid = 0x006af2ff;
		//guint32 matchid = 0;
		gchar* status = "Too bad, it's me - BLACKSMITH!!!";
		gchar* server = "0.0.0.0";
		packet_id = 0x10;
		buffer = g_byte_array_new();
		buffer = g_byte_array_append(buffer,&packet_id,1);
		buffer = g_byte_array_append(buffer,status,strlen(status)+1);
		buffer = g_byte_array_append(buffer,&matchid,4);
		buffer = g_byte_array_append(buffer,server,strlen(server)+1);
		do_write(hon->fd,buffer->data,buffer->len);
		g_byte_array_free(buffer,TRUE);
		return PURPLE_CMD_RET_OK;
	}
#endif
	
	

	*error = g_strdup(_("Unknown clan command"));
	return PURPLE_CMD_RET_FAILED;
}

static PurpleCmdRet honprpl_who(PurpleConversation *conv, const gchar *cmd,
								  gchar **args, gchar **error, void *userdata) 
{

	const char *user = args[0];
	hon_account* hon = conv->account->gc->proto_data;
	GByteArray* buffer;
	guint8 packet_id = 0x2a;

	if (!user || strlen(user) == 0) {
		*error = g_strdup(_("Request is missing nickname."));
		return PURPLE_CMD_RET_FAILED;
	} 
	hon->whois_conv = conv;

	buffer = g_byte_array_new();
	buffer = g_byte_array_append(buffer,&packet_id,1);
	buffer = g_byte_array_append(buffer,user,strlen(user)+1);
	do_write(hon->fd,buffer->data,buffer->len);
	g_byte_array_free(buffer,TRUE);
	return PURPLE_CMD_RET_OK;
}


/*
* prpl stuff. see prpl.h for more information.
*/

static PurplePluginProtocolInfo prpl_info =
{
	OPT_PROTO_CHAT_TOPIC | OPT_PROTO_UNIQUE_CHATNAME,  /* options */
	NULL,               /* user_splits, initialized in honprpl_init() */
	NULL,               /* protocol_options, initialized in honprpl_init() */
	{   /* icon_spec, a PurpleBuddyIconSpec */
		"",                   /* format */
			0,                               /* min_width */
			0,                               /* min_height */
			0,                             /* max_width */
			0,                             /* max_height */
			0,                           /* max_filesize */
			0,       /* scale_rules */
	},
	honprpl_list_icon,                  /* list_icon */
	honprpl_list_emblem,                                /* list_emblem */
	honprpl_status_text,                /* status_text */
	honprpl_tooltip_text,               /* tooltip_text */
	honprpl_status_types,               /* status_types */
	honprpl_blist_node_menu,            /* blist_node_menu */
	honprpl_chat_info,                  /* chat_info */
	honprpl_chat_info_defaults,         /* chat_info_defaults */
	honprpl_login,                      /* login */
	honprpl_close,                      /* close */
	honprpl_send_im,                    /* send_im */
	NULL,                   /* set_info */
	NULL,                /* send_typing */
	honprpl_get_info,                   /* get_info */
	NULL,                 /* set_status */
	NULL,                   /* set_idle */
	NULL,              /* change_passwd */
	honprpl_add_buddy,                  /* add_buddy */
	honprpl_add_buddies,                /* add_buddies */
	honprpl_remove_buddy,               /* remove_buddy */
	honprpl_remove_buddies,             /* remove_buddies */
	NULL,                 /* add_permit */
	NULL,                   /* add_deny */
	NULL,                 /* rem_permit */
	NULL,                   /* rem_deny */
	NULL,            /* set_permit_deny */
	honprpl_join_chat,                  /* join_chat */
	NULL,                /* reject_chat */
	honprpl_get_chat_name,              /* get_chat_name */
	NULL,                /* chat_invite */
	honprpl_chat_leave,                 /* chat_leave */
	NULL,               /* chat_whisper */
	honprpl_chat_send,                  /* chat_send */
	NULL,                                /* keepalive */
	NULL,              /* register_user */
	NULL,                /* get_cb_info */
	NULL,                                /* get_cb_away */
	NULL,                /* alias_buddy */
	NULL,                /* group_buddy */
	NULL,               /* rename_group */
	NULL,                                /* buddy_free */
	NULL,               /* convo_closed */
	honprpl_normalize,                  /* normalize */
	NULL,             /* set_buddy_icon */
	NULL,               /* remove_group */
	NULL,                                /* get_cb_real_name */
	honprpl_set_chat_topic,             /* set_chat_topic */
	NULL,                                /* find_blist_chat */
	honprpl_roomlist_get_list,          /* roomlist_get_list */
	honprpl_roomlist_cancel,            /* roomlist_cancel */
	NULL,   /* roomlist_expand_category */
	NULL,           /* can_receive_file */
	NULL,                                /* send_file */
	NULL,                                /* new_xfer */
	NULL,            /* offline_message */
	NULL,                                /* whiteboard_prpl_ops */
	NULL,                                /* send_raw */
	NULL,                                /* roomlist_room_serialize */
	NULL,                                /* unregister_user */
	NULL,                                /* send_attention */
	NULL,                                /* get_attention_types */
	sizeof(PurplePluginProtocolInfo),    /* struct_size */
	NULL,
	NULL,                                 /* initiate_media */
	NULL                                  /* can_do_media */	
};
static void honprpl_init(PurplePlugin *plugin)
{
	PurpleAccountOption *option_store_md5;
	PurpleAccountOption *option = purple_account_option_string_new(
		_("HoN masterserver URL"),      /* text shown to user */
		"masterserver",                /* pref name */
		HON_DEFAULT_MASTER_SERVER);               /* default value */

	purple_debug_info(HON_DEBUG_PREFIX, "starting up\n");

	option_store_md5 = purple_account_option_bool_new(
		_("Password is a MD5 hash"),      /* text shown to user */
		IS_MD5_OPTION,                /* pref name */
		FALSE
		);
	prpl_info.protocol_options = g_list_append(NULL, option);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option_store_md5);

	/* register whisper chat command */
	purple_cmd_register("whisper",
		"ws",                  /* args: recipient and message */
		PURPLE_CMD_P_DEFAULT,  /* priority */
		PURPLE_CMD_FLAG_CHAT  ,
		"prpl-hon",
		send_whisper,
		"whisper &lt;username&gt; &lt;message&gt;: send a whisper message",
		GINT_TO_POINTER(0));                 /* userdata */
	purple_cmd_register("whisper",
		"s",                  /* args: recipient and message */
		PURPLE_CMD_P_DEFAULT,  /* priority */
		PURPLE_CMD_FLAG_IM,
		"prpl-hon",
		send_whisper,
		"whisper &lt;message&gt;: send a whisper message",
		GINT_TO_POINTER(1));                 /* userdata */
	purple_cmd_register("w",
		"ws",                  /* args: recipient and message */
		PURPLE_CMD_P_DEFAULT,  /* priority */
		PURPLE_CMD_FLAG_CHAT  ,
		"prpl-hon",
		send_whisper,
		"whisper &lt;username&gt; &lt;message&gt;: send a whisper message",
		GINT_TO_POINTER(0));                 /* userdata */
	purple_cmd_register("w",
		"s",                  /* args: recipient and message */
		PURPLE_CMD_P_DEFAULT,  /* priority */
		PURPLE_CMD_FLAG_IM,
		"prpl-hon",
		send_whisper,
		"whisper &lt;message&gt;: send a whisper message",
		GINT_TO_POINTER(1));                 /* userdata */

	
	/* clan commands */
	purple_cmd_register("clan",
		"ws",                  /* args: recipient and message */
		PURPLE_CMD_P_DEFAULT,  /* priority */
		PURPLE_CMD_FLAG_IM|PURPLE_CMD_FLAG_CHAT,
		"prpl-hon",
		clan_commands,
		_("clan invite - invite to clan\nmessage or m - clan message\nother not implemented"),
		NULL);   
	purple_cmd_register("c",
		"ws",                  /* args: recipient and message */
		PURPLE_CMD_P_DEFAULT,  /* priority */
		PURPLE_CMD_FLAG_IM|PURPLE_CMD_FLAG_CHAT,
		"prpl-hon",
		clan_commands,
		_("clan invite - invite to clan\nmessage or m - clan message\nother not implemented"),
		NULL); 


	/* whois */
	purple_cmd_register("whois",
		"s",                  /* args: user */
		PURPLE_CMD_P_DEFAULT,  /* priority */
		PURPLE_CMD_FLAG_IM|PURPLE_CMD_FLAG_CHAT,
		"prpl-hon",
		honprpl_who,
		_("Request user status"),
		NULL); 
	/* whois */
	purple_cmd_register("who",
		"s",                  /* args: user */
		PURPLE_CMD_P_DEFAULT,  /* priority */
		PURPLE_CMD_FLAG_IM|PURPLE_CMD_FLAG_CHAT,
		"prpl-hon",
		honprpl_who,
		_("Request user status"),
		NULL); 


	_HON_protocol = plugin;
}

static void honprpl_destroy(PurplePlugin *plugin) {
	purple_debug_info(HON_DEBUG_PREFIX, "shutting down\n");
}


static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,                                     /* magic */
	PURPLE_MAJOR_VERSION,                                    /* major_version */
	PURPLE_MINOR_VERSION,                                    /* minor_version */
	PURPLE_PLUGIN_PROTOCOL,                                  /* type */
	NULL,                                                    /* ui_requirement */
	0,                                                       /* flags */
	NULL,                                                    /* dependencies */
	PURPLE_PRIORITY_DEFAULT,                                 /* priority */
	honprpl_ID,                                             /* id */
	"Heroes of Newerth",                                 /* name */
	DISPLAY_VERSION,                                         /* version */
	N_("Heroes of Newerth Protocol Plugin"),                              /* summary */
	N_("Protocol Plugin for Heroes of Newerth chat server"),                              /* description */
	NULL,                                                    /* author */
	"http://theli.is-a-geek.org/",                                          /* homepage */
	NULL,                                                    /* load */
	NULL,                                                    /* unload */
	honprpl_destroy,                                        /* destroy */
	NULL,                                                    /* ui_info */
	&prpl_info,                                              /* extra_info */
	NULL,                                                    /* prefs_info */
	honprpl_actions,                                        /* actions */
	NULL,                                                    /* padding... */
	NULL,
	NULL,
	NULL,
};

PURPLE_INIT_PLUGIN(null, honprpl_init, info);
