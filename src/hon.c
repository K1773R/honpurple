#include "hon.h"
#include "packet_id.h"
#include "debug.h"
#include <stdarg.h>
#include <notify.h>
#include <string.h>

#ifdef _WIN32
#include <libc_interface.h>
#undef vsnprintf /* conflicts with msvc definition and not needed */
#else
#include <sys/types.h>
#include <sys/socket.h>
#endif

/*
	Macroses and utilities
 */
static int do_write(int fd, void* buffer, int len){
	return write(fd,buffer,len);
}

static gboolean hon_send_packet(PurpleConnection* gc,guint16 packet_id,const gchar* paramstring , ...){
	gboolean res;
	guint32 intparam;
	guint8 byteparam;
	const gchar* stringparam;
	va_list marker;
	hon_account* hon = gc->proto_data;
	GByteArray* buffer = g_byte_array_new();
	buffer = g_byte_array_append(buffer,&packet_id,2);
	va_start( marker, paramstring );

	while (paramstring != 0x00 && *paramstring != 0x00)
	{
		switch (*paramstring){
			case 'i':
				intparam = va_arg( marker, int);
				buffer = g_byte_array_append(buffer,(const guint8*)&intparam,4);
				break;
			case 's':
				stringparam = va_arg( marker, const gchar*);
				buffer = g_byte_array_append(buffer,stringparam,strlen(stringparam)+1);
				break;
			case 'b':
				byteparam = va_arg( marker, int);
				buffer = g_byte_array_append(buffer,&byteparam,1);
				break;
		}
		paramstring++;
	}
	va_end(marker);
	res = buffer->len == do_write(hon->fd,buffer->data,buffer->len);
	g_byte_array_free(buffer,TRUE);
	return res;
}
const char *hon_normalize_nick(const PurpleAccount *acct,
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
guint16 read_guint16(int fd){
	int aaa = 0;
	guint16 res = 0;
	recv(fd,&res,2,MSG_WAITALL);
	return res;
}
gchar read_byte(int fd){
	gchar res;
	recv(fd,&res,1,MSG_WAITALL);
	return res;
}
guint32 read_guint32(int fd){
	guint32 res;
	recv(fd,&res,4,MSG_WAITALL);
	return res;
}
gchar* read_string(int fd){
	char c;
	GString* str = g_string_new(0);
	recv(fd,&c,1,MSG_WAITALL);
	while (c != 0)
	{
		str = g_string_append_c(str,c);
		recv(fd,&c,1,MSG_WAITALL);
	}
	return g_string_free(str,FALSE);
}


/* Packet parsers */
int hon_parse_packet(PurpleConnection *gc, int sock){
	gchar* buffer;
 	guint16 packet_id = read_guint16(sock);
#if 0
	hexdump = g_string_new(NULL);
	hexdump_g_string_append(hexdump,"",buffer,packet_length - 1);
	purple_debug_info(HON_DEBUG_PREFIX, "packet:\nid:%X(%d)\nlength:%d\ndata:\n%s\n",packet_id,packet_id,packet_length, hexdump->str);
	g_string_free(hexdump,TRUE);
#endif
	purple_debug_info(HON_DEBUG_PREFIX, "packet:id:%X(%d)\n",packet_id,packet_id);
	switch (packet_id)
	{
	case 0:
		return 0;
	case HON_SC_AUTH_ACCEPTED/*0x1C00*/: /* logged on ! */
		purple_connection_update_progress(gc, _("Connected"),
			3,   /* which connection step this is */
			4);  /* total number of steps */
		purple_connection_set_state(gc, PURPLE_CONNECTED);
		break;
	case HON_SC_PING/*0x01*/:
		hon_send_pong(gc);
		purple_debug_info(HON_DEBUG_PREFIX, "server ping, sending pong\n");
		break;
	case HON_SC_CHANNEL_MSG/*0x03*/:
		hon_parse_chat_message(gc,sock);
		break;
	case HON_SC_CHANGED_CHANNEL/*0x04*/:
		hon_parse_chat_entering(gc,sock);
		break;
	case HON_SC_JOINED_CHANNEL/*0x05*/:
		hon_parse_chat_join(gc,sock);
		break;
	case HON_SC_LEFT_CHANNEL/*0x06*/:
		hon_parse_chat_leave(gc,sock);
		break;
	case HON_SC_WHISPER/*0x08*/:
		hon_parse_pm_whisper(gc,sock,TRUE);
		break;
	case HON_SC_WHISPER_FAILED/*0x09*/:
		hon_parse_whisper_failed(gc,sock);
		break;
	case HON_SC_INITIAL_STATUS/*0x0B*/:
		hon_parse_initiall_statuses(gc,sock);
		break;
	case HON_SC_UPDATE_STATUS/*0x0C*/:
		hon_parse_user_status(gc,sock);
		break;
	case HON_SC_NOTIFICATION/*0x12*/:
		hon_parse_notification(gc,sock);
		break;
	case HON_SC_CLAN_MESSAGE/*0x13*/:
		hon_parse_clan_message(gc,sock);
		break;
	case HON_SC_PM/*0x1C*/:
		hon_parse_pm_whisper(gc,sock,FALSE);
		break;
	case HON_SC_PM_FAILED/*0x1D*/:
		hon_parse_pm_failed(gc,sock);
		break;
	case HON_SC_CHANNEL_LIST/*0x1F*/:
		hon_parse_channel_list(gc,sock);
		break;
	case HON_SC_WHISPER_BUDDIES/*0x20*/:
		hon_parse_pm_whisper(gc,sock,TRUE);
		break;
	case HON_SC_MAX_CHANNELS/*0x21*/:
		hon_parse_max_channels(gc,sock);
		break;
	case HON_SC_USER_INFO_NO_EXIST/*0x2b*/:
	case HON_SC_USER_INFO_OFFLINE/*0x2c*/:
	case HON_SC_USER_INFO_ONLINE/*0x2d*/:
	case HON_SC_USER_INFO_IN_GAME/*0x2e*/:
		hon_parse_userinfo(gc,sock,packet_id);
		break;
	case HON_SC_UPDATE_TOPIC/*0x30*/:
		hon_parse_chat_topic(gc,sock);
		break;
	case HON_SC_CHANNEL_KICK/*0x31*/:
		hon_parse_channel_kick(gc,sock);
		break;
	case HON_SC_CHANNEL_BAN/*0x32*/:
	case HON_SC_CHANNEL_UNBAN/*0x33*/:
		hon_parse_channel_ban_unban(gc,sock,packet_id);
		break;
	case HON_SC_CHANNEL_BANNED/*0x34*/:
		hon_parse_channel_banned(gc,sock);
		break;
	case HON_SC_CHANNEL_SILENCED/*0x35*/:
		hon_parse_channel_silenced(gc,sock);
		break;
	case HON_SC_CHANNEL_SILENCE_LIFTED/*0x36*/:
		hon_parse_channel_silence_lifted(gc,sock);
		break;
	case HON_SC_CHANNEL_SILENCE_PLACED/*0x37*/:
		hon_parse_channel_silence_placed(gc,sock);
		break;
	case HON_SC_CHANNEL_PROMOTE/*0x3A*/:
	case HON_SC_CHANNEL_DEMOTE/*0x3A*/:
		hon_parse_channel_promote_demote(gc,sock,packet_id);
		break;
	case HON_SC_MESSAGE_ALL/*0x39*/:
		hon_parse_global_notification(gc,sock);
		break;
	case HON_SC_CHANNEL_AUTH_ENABLE:
	case HON_SC_CHANNEL_AUTH_DISABLE:
		hon_parse_channel_auth_enable_disable(gc,sock,packet_id);
		break;
	case HON_SC_CHANNEL_AUTH_ADD:
	case HON_SC_CHANNEL_AUTH_DELETE:
	case HON_SC_CHANNEL_ADD_AUTH_FAIL:
	case HON_SC_CHANNEL_DEL_AUTH_FAIL:
		hon_parse_channel_auth_add_delete(gc,sock,packet_id);
		break;
	case HON_SC_CHANNEL_AUTH_LIST:
		hon_parse_channel_auth_list(gc,sock);
		break;
	case HON_SC_CHANNEL_PASSWORD_CHANGED/*0x43*/:
		hon_parse_channel_password_changed(gc,sock);
		break;
	case HON_SC_JOIN_CHANNEL_PASSWORD/*0x46*/:
		hon_parse_join_channel_password(gc,sock);
		break;
	case HON_SC_CHANNEL_EMOTE/*0x65*/:
		hon_parse_emote(gc,sock);
		break;
	case 0x18:
		read_string(sock);
		break;
	case 0x68:
		read_guint32(sock);
		break;
	default:
		purple_debug_info(HON_DEBUG_PREFIX, "unknown packet:id:%X(%d), trying to skip\n",packet_id,packet_id);
		//try to skip this
		buffer = g_malloc0(1024);
		packet_id = recv(sock,buffer,1024,0);
		g_free(buffer);
		/*
		hexdump = g_string_new(NULL);
		hexdump_g_string_append(hexdump,"",buffer,packet_length - 1);
		purple_debug_info(HON_DEBUG_PREFIX, "unknown packet:\nid:%X(%d)\nlength:%d\ndata:\n%s\n",packet_id,packet_id,packet_length, hexdump->str);
		g_string_free(hexdump,TRUE);
		*/

		break;
	}
	return 1;
}
void hon_parse_channel_auth_list(PurpleConnection *gc,int fd)
{
	hon_account* hon = gc->proto_data;
	PurpleConversation* chat;
	guint32 count,chatid = read_guint32(fd);
	count = read_guint32(fd);
	chat = purple_find_chat(gc,chatid);
	if (!chat)
		return;
	purple_conv_chat_write(PURPLE_CONV_CHAT(chat), "", 
		_("Listing Authorized Users..."),
		PURPLE_MESSAGE_SYSTEM, time(NULL));
	if (count == 0)
	{
		purple_conv_chat_write(PURPLE_CONV_CHAT(chat), "", 
			_("There are no authorized users for this channel."),
			PURPLE_MESSAGE_SYSTEM, time(NULL));
	} 
	else
	{
		while (count--)
		{
			gchar* buffer = read_string(fd);
			purple_conv_chat_write(PURPLE_CONV_CHAT(chat), "", 
				buffer,
				PURPLE_MESSAGE_SYSTEM, time(NULL));
			g_free(buffer);
		}
	}
	
}
void hon_parse_channel_auth_add_delete(PurpleConnection *gc,int fd,guint16 packet_id)
{
	hon_account* hon = gc->proto_data;
	PurpleConversation* chat;
	gchar* msg_template,*msg;
	guint32 chatid = read_guint32(fd);
	gchar* buffer = read_string(fd);
	chat = purple_find_chat(gc,chatid);
	if (!chat)
		return;
	if (packet_id == HON_SC_CHANNEL_AUTH_ADD)
		msg_template = _("%s has been added to the authorization list.");
	else if (packet_id == HON_SC_CHANNEL_AUTH_DELETE)
		msg_template = _("%s has been removed from the authorization list.");
	else if (packet_id == HON_SC_CHANNEL_ADD_AUTH_FAIL)
		msg_template = _("%s is already on the authorization list.");
	else /* if (packet_id == HON_SC_CHANNEL_DEL_AUTH_FAIL)*/
		msg_template = _("%s is not on the authorization list.");
	msg = g_strdup_printf(msg_template,buffer);
	purple_conv_chat_write(PURPLE_CONV_CHAT(chat), "",msg,
		PURPLE_MESSAGE_SYSTEM, time(NULL));
	g_free(msg);
	g_free(buffer);
}
void hon_parse_channel_auth_enable_disable(PurpleConnection *gc,int fd,guint16 packet_id)
{
	hon_account* hon = gc->proto_data;
	PurpleConversation* chat;
	guint32 chatid = read_guint32(fd);
	chat = purple_find_chat(gc,chatid);
	if (!chat)
		return;
	if (packet_id == HON_SC_CHANNEL_AUTH_ENABLE)
		purple_conv_chat_write(PURPLE_CONV_CHAT(chat), "", 
		_("Authorization has been enabled for this channel, you must now be on the authorized list to join."),
		PURPLE_MESSAGE_SYSTEM, time(NULL));
	else
		purple_conv_chat_write(PURPLE_CONV_CHAT(chat), "",
		_("Authorization has been disabled for this channel, all users can now join."),
		PURPLE_MESSAGE_SYSTEM, time(NULL));
}
void hon_parse_channel_silence_placed(PurpleConnection* gc,int fd){
	PurpleConversation *convo;
	hon_account* hon = gc->proto_data;
	gchar * msg , *silencer, *silenced,*chatname;
	guint32 duration;
	chatname = read_string(fd);
	silencer = read_string(fd);
	silenced = read_string(fd);
	duration = read_guint32(fd);
	convo = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,chatname,gc->account);
	if (!convo) {
		purple_debug(PURPLE_DEBUG_ERROR, HON_DEBUG_PREFIX, "Got a silenced message for %s, which doesn't exist\n", chatname);
		return;
	}
	msg = g_strdup_printf(_("%s has been silenced by %s for %d ms."),silenced,silencer,duration);
	purple_conv_chat_write(PURPLE_CONV_CHAT(convo), "", msg, PURPLE_MESSAGE_SYSTEM, time(NULL));
	g_free(msg);
	g_free(chatname);
	g_free(silencer);
	g_free(silenced);
}
void hon_parse_channel_silence_lifted(PurpleConnection* gc,int fd){
	PurpleConversation *convo;
	hon_account* hon = gc->proto_data;
	gchar * msg;
	gchar* buffer = read_string(fd);
	convo = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,buffer,gc->account);
	if (!convo) {
		purple_debug(PURPLE_DEBUG_ERROR, HON_DEBUG_PREFIX, "Got an unsilenced message for %s, which doesn't exist\n", buffer);
	}
	else
	{
		msg = g_strdup_printf(_("Your silence has been lifted in the channel '%s'."),buffer);
		purple_conv_chat_write(PURPLE_CONV_CHAT(convo), "", msg, PURPLE_MESSAGE_SYSTEM, time(NULL));
		g_free(msg);
	}
	g_free(buffer);
}
void hon_parse_channel_silenced(PurpleConnection* gc,int fd){
	PurpleConversation *convo;
	hon_account* hon = gc->proto_data;
	guint32 chan_id = read_guint32(fd);
	convo = purple_find_chat(gc,chan_id);
	if (!convo) {
		purple_debug(PURPLE_DEBUG_ERROR, HON_DEBUG_PREFIX, "Got a silenced message for %d, which doesn't exist\n", chan_id);
		return;
	}
	purple_conv_chat_write(PURPLE_CONV_CHAT(convo), "", _("You are silenced in this channel and cannot talk."), PURPLE_MESSAGE_SYSTEM, time(NULL));
}
void hon_parse_channel_password_changed(PurpleConnection* gc,int fd){
	PurpleConversation *convo;
	hon_account* hon = gc->proto_data;
	gchar * msg;
	guint32 chan_id = read_guint32(fd);
	gchar* buffer = read_string(fd);
	convo = purple_find_chat(gc,chan_id);
	if (!convo) {
		purple_debug(PURPLE_DEBUG_ERROR, HON_DEBUG_PREFIX, "Got a message for %d, which doesn't exist\n", chan_id);
	}
	else
	{
		msg = g_strdup_printf(_("The password for this channel has been changed by %s."),buffer);
		purple_conv_chat_write(PURPLE_CONV_CHAT(convo), "", msg, PURPLE_MESSAGE_SYSTEM, time(NULL));
		g_free(msg);
	}
	g_free(buffer);
}
void hon_parse_channel_ban_unban(PurpleConnection *gc,int fd,guint16 packet_id)
{
	hon_account* hon = gc->proto_data;
	guint32 chatid,kickerid;
	gchar* kicked,*kicker,*msg,*action;
	PurpleConversation* chat;
	chatid = read_guint32(fd);
	kickerid = read_guint32(fd);
	kicked = read_string(fd);
	chat = purple_find_chat(gc,chatid);
	if (!chat)
		return;

	if (kickerid == hon->self.account_id)
		kicker = hon->self.nickname;
	else if((kicker = g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(kickerid))))
	{}
	else
		kicker = _("Someone");

	if (packet_id == HON_SC_CHANNEL_BAN)
		action = _("banned");
	else
		action = _("unbanned");
	

	msg = g_strdup_printf(_("%s was %s from the channel by %s."),kicked,action,kicker);
	purple_conv_chat_write(PURPLE_CONV_CHAT(chat), "", msg, PURPLE_MESSAGE_SYSTEM, time(NULL));
	g_free(msg);
	g_free(kicked);
}
void hon_parse_join_channel_password(PurpleConnection *gc,int fd){
	gchar* buffer = read_string(fd);
	gchar* msg = g_strdup_printf(_("The channel '%s' requires a password."),buffer);
	purple_notify_error(NULL,_("Banned"),msg,NULL);
	g_free(msg);
	g_free(buffer);
}
void hon_parse_channel_banned(PurpleConnection *gc,int fd){
	gchar* buffer = read_string(fd);
	gchar* msg = g_strdup_printf(_("You are banned from the channel '%s'"),buffer);
	purple_notify_error(NULL,_("Banned"),msg,NULL);
	g_free(msg);
}
void hon_parse_channel_kick(PurpleConnection *gc,int fd){
	hon_account* hon = gc->proto_data;
	guint32 chatid,kickerid,kickedid;
	gchar* kicked,*kicker,*msg;
	PurpleConversation* chat;
	chatid = read_guint32(fd);
	kickerid = read_guint32(fd);
	kickedid = read_guint32(fd);
	chat = purple_find_chat(gc,chatid);
	if (!chat)
		return;

	if (kickerid == hon->self.account_id)
		kicker = hon->self.nickname;
	else if((kicker = g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(kickerid))))
	{}
	else
		kicker = _("Someone");

	if (kickedid == hon->self.account_id)
		kicked = hon->self.nickname;
	else if((kicked = g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(kickedid))))
	{}
	else
		kicked = _("Someone");

	msg = g_strdup_printf(_("%s was kicked from the channel by %s"),kicked,kicker);
	purple_conv_chat_write(PURPLE_CONV_CHAT(chat), "", msg, PURPLE_MESSAGE_SYSTEM, time(NULL));

	if (kickedid == hon->self.account_id)
		serv_got_chat_left(gc, chatid);

	g_free(msg);
}
void hon_parse_channel_promote_demote(PurpleConnection *gc,int fd,guint16 packet_id){
	hon_account* hon = gc->proto_data;
	guint32 chatid,kickerid,kickedid;
	gchar* kicked,*kicker,*msg;
	PurpleConversation* chat;
	const char* action,*rank;
	PurpleConvChatBuddyFlags chatbuddy_flags;

	chatid = read_guint32(fd);
	kickedid = read_guint32(fd);
	kickerid = read_guint32(fd);
	chat = purple_find_chat(gc,chatid);
	if (!chat)
		return;

	if (kickerid == hon->self.account_id)
		kicker = hon->self.nickname;
	else if((kicker = g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(kickerid))))
	{}
	else
		kicker = _("Someone");

	if (kickedid == hon->self.account_id)
		kicked = hon->self.nickname;
	else if((kicked = g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(kickedid))))
	{}
	else
		return;

	
	chatbuddy_flags = purple_conv_chat_user_get_flags(PURPLE_CONV_CHAT(chat),kicked);
	if (packet_id == HON_SC_CHANNEL_PROMOTE)
	{
		action = _("promoted");
		if(chatbuddy_flags == PURPLE_CBFLAGS_NONE)
		{
			rank = _("Channel Officer");
			chatbuddy_flags = PURPLE_CBFLAGS_HALFOP;
		}
		else if (chatbuddy_flags == PURPLE_CBFLAGS_FOUNDER)
		{
			//nowhere to update .. "Stuff" maybe? o_O
			rank = _("Stuff");
		}
		else if (chatbuddy_flags == PURPLE_CBFLAGS_OP)
		{
			rank = _("Channel Administrator");
			chatbuddy_flags = PURPLE_CBFLAGS_FOUNDER;
		}
		else if (chatbuddy_flags == PURPLE_CBFLAGS_HALFOP)
		{
			rank = _("Channel Leader");
			chatbuddy_flags = PURPLE_CBFLAGS_OP;
		}
		else
			rank = _("huh?");
	}
	else
	{
		rank = _("huh?");
		action = _("demoted");

		if (chatbuddy_flags == PURPLE_CBFLAGS_FOUNDER)
		{
			rank = _("Channel Leader");
			chatbuddy_flags = PURPLE_CBFLAGS_OP;
		}
		else if (chatbuddy_flags == PURPLE_CBFLAGS_OP)
		{
			rank = _("Channel Officer");
			chatbuddy_flags = PURPLE_CBFLAGS_HALFOP;
		}
		else if (chatbuddy_flags == PURPLE_CBFLAGS_HALFOP)
		{
			rank = _("Non admin status");
			chatbuddy_flags = PURPLE_CBFLAGS_NONE;
		}


	}

	purple_conv_chat_user_set_flags(PURPLE_CONV_CHAT(chat),kicked,chatbuddy_flags);
	msg = g_strdup_printf(_("%s has been %s to %s by %s"),kicked,action,rank,kicker);
	purple_conv_chat_write(PURPLE_CONV_CHAT(chat), "", msg, PURPLE_MESSAGE_SYSTEM, time(NULL));

	g_free(msg);
}

void hon_parse_pm_failed(PurpleConnection *gc,int fd){
	purple_notify_error(NULL,_("Message failed"),_("The user you tried to chat with is not online"),
		NULL);
}
void hon_parse_whisper_failed(PurpleConnection *gc,int fd){
	purple_notify_error(NULL,_("Whisper failed"),_("The user you tried to whisper is not online"),
		NULL);
}
void hon_parse_max_channels(PurpleConnection *gc,int fd){
	purple_notify_error(NULL,_("Channel limit reached"),_("You have reached an open channels limit."),
		_("To join other channel close some already opened"));
}
void hon_parse_global_notification(PurpleConnection *gc,int fd){
	hon_account* hon = gc->proto_data;
	gchar* username = read_string(fd);
	gchar* buffer = read_string(fd);
	purple_notify_warning(NULL,username,buffer,NULL);
	g_free(buffer);
	g_free(username);
}
void hon_parse_notification(PurpleConnection *gc,int fd){
	hon_account* hon = gc->proto_data;
	guint8 notification_type = read_byte(fd);
	gchar* title = NULL;
	gchar* buffer = read_string(fd);
	switch (notification_type)
	{
	case HON_NOTIFICATION_ADDED_AS_BUDDY:
		title = g_strdup(_("User added you as buddy"));
		break;
	case HON_NOTIFICATION_BUDDY_ADDED:
		title = g_strdup(_("Buddy added"));
		break;
	case HON_NOTIFICATION_REMOVED_AS_BUDDY:
		title = g_strdup(_("User removed you as buddy"));
		break;
	case HON_NOTIFICATION_BUDDY_REMOVED:
		title = g_strdup(_("Buddy removed"));
		break;
	default :
		title = g_strdup_printf(_("Unknown notification type (%d)"),notification_type);
		break;
	}
	purple_notify_info(NULL,title,buffer,NULL);
	g_free(buffer);
	g_free(title);
}
void hon_parse_initiall_statuses(PurpleConnection *gc,int fd){
	guint32 status,flags;
	
	hon_account* hon;
	guint32 id,count = read_guint32(fd);
	hon = gc->proto_data;
	purple_debug_info(HON_DEBUG_PREFIX, "parsing status for %d buddies\n",count);
	while (count-- > 0)
	{
		gchar* raw_gamename = NULL;
		gchar* nick,*gamename=NULL, *server=NULL,*status_id = HON_STATUS_ONLINE_S;

		id = read_guint32(fd);
		status = read_byte(fd);
		flags = read_byte(fd);
		nick = g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(id));
		if (status == HON_STATUS_INLOBBY || status == HON_STATUS_INGAME)
		{
			server = read_string(fd);
			status_id = HON_STATUS_INGAME_S;
		}
		if (status == HON_STATUS_INGAME)
		{
			raw_gamename = read_string(fd);
			gamename = hon_strip(raw_gamename,TRUE);
		}
		if(!status)
			status_id = HON_STATUS_OFFLINE_S;
		purple_debug_info(HON_DEBUG_PREFIX, "status for %s,flags:%d,status:%d,game:%s,server:%s\n",nick,flags,status,gamename,server);
		purple_prpl_got_user_status(gc->account, nick, status_id,
			HON_BUDDYID_ATTR , id,
			HON_STATUS_ATTR,status,HON_FLAGS_ATTR,flags,
			server ? HON_SERVER_ATTR : NULL,server,gamename ? HON_GAME_ATTR : NULL,gamename,NULL);
		g_free(gamename);
		g_free(server);
#ifdef MINBIF
		if (status == HON_STATUS_INGAME)
			status_id = g_strdup_printf("%s %s %d 0 %s",MINBIF_STATUS,
					nick,status,raw_gamename);
		else
			status_id = g_strdup_printf("%s %s %d",MINBIF_STATUS,
					nick,status);
		serv_got_im(gc,MINBIF_USER,status_id,PURPLE_MESSAGE_RECV,time(NULL));
		g_free(status_id);
#endif
		g_free(raw_gamename);
	}
}
void hon_parse_user_status(PurpleConnection *gc,int fd){
	gchar* nick,*gamename=NULL, *server=NULL,*status_id = HON_STATUS_ONLINE_S;
	gchar* clan = NULL; // or channel?
	guint32 clanid;
	hon_account* hon = gc->proto_data;
	guint32 status;
	guint32 flags;
	guint32 matchid = 0;
	gchar* raw_gamename = NULL;

	guint32 id = read_guint32(fd);
	status = read_byte(fd);
	flags = read_byte(fd);
	nick = g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(id));
	/* TODO: figure this out */
	clanid = read_guint32(fd);
	clan = read_string(fd); // huh ?
	if (status == HON_STATUS_INLOBBY || status == HON_STATUS_INGAME)
	{
		server = read_string(fd);
		status_id = HON_STATUS_INGAME_S;
	}
	if (status == HON_STATUS_INGAME)
	{
		raw_gamename = read_string(fd);
		gamename = hon_strip(raw_gamename,TRUE);
		matchid = read_guint32(fd);
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
	g_free(clan);
	g_free(server);
#ifdef MINBIF
	if (status == HON_STATUS_INGAME)
		status_id = g_strdup_printf("%s %s %d %d %s",MINBIF_STATUS,
				nick,status,matchid,raw_gamename);
	else
		status_id = g_strdup_printf("%s %s %d",MINBIF_STATUS,
				nick,status);
	serv_got_im(gc,MINBIF_USER,status_id,PURPLE_MESSAGE_RECV,time(NULL));
	g_free(status_id);
#endif
	g_free(raw_gamename);
}


void hon_parse_pm_whisper(PurpleConnection *gc,int fd,guint16 is_whisper)
{
	hon_account* hon = gc->proto_data;
	PurpleMessageFlags receive_flags;
	gchar* message,*from_username; 
	gchar* buffer;
	gchar* tmp = read_string(fd);
	buffer = read_string(fd);
	from_username = tmp;
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
	g_free(tmp);
	g_free(buffer);
}
void hon_parse_channel_list(PurpleConnection *gc,int fd){
	hon_account* hon = gc->proto_data;
	guint32 count = read_guint32(fd);
	count = read_guint32(fd);
	if (!hon->roomlist)
		return;
	while (count--)
	{
		PurpleRoomlistRoom *room;
		gchar* name,*colorname;
		guint32 id,participants;
		id = read_guint32(fd);
		name = read_string(fd);
		colorname = hon_strip(name,FALSE);
		participants = read_guint32(fd);

		room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, colorname, NULL);

		purple_roomlist_room_add_field(hon->roomlist, room, GINT_TO_POINTER(id));
		purple_roomlist_room_add_field(hon->roomlist, room, name);
		purple_roomlist_room_add_field(hon->roomlist, room, GINT_TO_POINTER(participants));
		purple_roomlist_room_add(hon->roomlist, room);
		g_free(colorname);
		g_free(name);
	}
	purple_roomlist_set_in_progress(hon->roomlist, FALSE);
	purple_roomlist_unref(hon->roomlist);
	hon->roomlist = NULL;

}
void hon_parse_chat_entering(PurpleConnection *gc,int fd)
{
	PurpleConversation *convo;
	hon_account* hon = gc->proto_data;
	guint8 unknown,flags;
	guint32 op_count,chat_id,count;
	guint32 purple_flags = 0;
	gchar* topic,*topic_raw;
	const gchar* extra;
	GHashTable* ops = NULL;
	gchar* buf;

	gchar* room = read_string(fd);
	chat_id = read_guint32(fd);
	unknown = read_byte(fd);
	buf = read_string(fd);
	topic = hon2html(buf);
	topic_raw = hon_strip(buf,FALSE);
	g_free(buf);
	op_count = read_guint32(fd);
	if (op_count != 0)
	{
		guint32 op_id,op_type;
		ops = g_hash_table_new(g_direct_hash,g_direct_equal);
		while (op_count--)
		{
			op_id = read_guint32(fd);
			op_type = read_byte(fd);
			g_hash_table_insert(ops,GINT_TO_POINTER(op_id),GINT_TO_POINTER(op_type));
		}
	}
	count = read_guint32(fd);
	convo = serv_got_joined_chat(gc, chat_id, room);
	purple_conv_chat_set_topic(PURPLE_CONV_CHAT(convo), NULL, topic_raw);

	g_free(room);
	while (count--)
	{
		guint32 account_id;
		guint8 status;
		const gchar* nickname;
		buf = read_string(fd);
		nickname = buf;
		account_id = read_guint32(fd);
		status = read_byte(fd);
		flags = read_byte(fd);
		purple_debug_info(HON_DEBUG_PREFIX, "room participant: %s , id=%d,status=%d,flags=%d\n",
			nickname,account_id,status,flags);

		flags |= GPOINTER_TO_INT(g_hash_table_lookup(ops,GINT_TO_POINTER(account_id)));
		
		flags &= 0xF;
		purple_flags = PURPLE_CBFLAGS_NONE;

		if (flags == HON_FLAGS_CHAT_ADMINISTRATOR)
			purple_flags = PURPLE_CBFLAGS_FOUNDER;
		else if (flags == HON_FLAGS_CHAT_LEADER)
			purple_flags = PURPLE_CBFLAGS_OP;
		else if (flags == HON_FLAGS_CHAT_OFFICER)
			purple_flags = PURPLE_CBFLAGS_HALFOP;


		extra = nickname;
		nickname = hon_normalize_nick(gc->account,nickname);
		purple_conv_chat_add_user(PURPLE_CONV_CHAT(convo), nickname, extra, purple_flags, FALSE);
		if (!g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(account_id)))
		{
			g_hash_table_insert(hon->id2nick,GINT_TO_POINTER(account_id),g_strdup(nickname));
		}
		g_free(buf);

	}
	flags = 0;
	purple_flags = PURPLE_CBFLAGS_NONE;

	if(ops)
	{
		flags = GPOINTER_TO_INT(g_hash_table_lookup(ops,GINT_TO_POINTER(hon->self.account_id)));
		g_hash_table_destroy(ops);
	}
	if (flags == HON_FLAGS_CHAT_ADMINISTRATOR)
		purple_flags = PURPLE_CBFLAGS_FOUNDER;
	else if (flags == HON_FLAGS_CHAT_LEADER)
		purple_flags = PURPLE_CBFLAGS_OP;
	else if (flags == HON_FLAGS_CHAT_OFFICER)
		purple_flags = PURPLE_CBFLAGS_HALFOP;


	purple_conv_chat_write(PURPLE_CONV_CHAT(convo), "", topic, PURPLE_MESSAGE_SYSTEM|PURPLE_MESSAGE_NO_LOG, time(NULL));
	g_free(topic);
	g_free(topic_raw);

	purple_conv_chat_add_user(PURPLE_CONV_CHAT(convo), hon->self.nickname, NULL,purple_flags , FALSE);
}

void hon_parse_emote(PurpleConnection *gc,int fd){
	hon_account *hon = gc->proto_data;
	guint32 account_id;
	guint32 chan_id;
	gchar* msg;
	PurpleConversation* chat;
	gchar* sender;
	gchar* buffer;
	account_id = read_guint32(fd);
	chan_id = read_guint32(fd);
	buffer = read_string(fd);
	msg = hon2html(buffer);
	g_free(buffer);
	chat = purple_find_chat(gc,chan_id);
	sender = g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(account_id));
	serv_got_chat_in(gc,chan_id,sender ,PURPLE_MESSAGE_RECV,msg,time(NULL));
	g_free(msg);
}
void hon_parse_chat_message(PurpleConnection *gc,int fd){
	hon_account *hon = gc->proto_data;
	guint32 account_id;
	guint32 chan_id;
	gchar* msg;
	gchar* sender;
	gchar* buffer;

	account_id = read_guint32(fd);
	chan_id = read_guint32(fd);
	buffer = read_string(fd);
	msg = hon2html(buffer);
	g_free(buffer);
	sender = g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(account_id));
	serv_got_chat_in(gc,chan_id,sender? sender : "unknown user" ,PURPLE_MESSAGE_RECV,msg,time(NULL));
	g_free(msg);
}
void hon_parse_chat_join(PurpleConnection *gc,int fd){
	hon_account* hon = gc->proto_data;
	guint32 account_id;
	guint32 chan_id,purple_flags = PURPLE_CBFLAGS_NONE;
	PurpleConversation* conv;
	guint8 status,flags;
	const gchar* extra;
	const gchar* nick;
	gchar* buf = read_string(fd);
	nick = buf;
	account_id = read_guint32(fd);
	chan_id = read_guint32(fd);
	conv = purple_find_chat(gc,chan_id);

	extra = nick;
	nick = hon_normalize_nick(gc->account,nick);
	status = read_byte(fd);
	flags = read_byte(fd);

	if (flags == HON_FLAGS_CHAT_ADMINISTRATOR)
		purple_flags = PURPLE_CBFLAGS_FOUNDER;
	else if (flags == HON_FLAGS_CHAT_LEADER)
		purple_flags = PURPLE_CBFLAGS_OP;
	else if (flags == HON_FLAGS_CHAT_OFFICER)
		purple_flags = PURPLE_CBFLAGS_HALFOP;


	if (conv)
	{
		purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv),nick,extra,purple_flags,TRUE);
	}
	if (!g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(account_id)))
	{
		g_hash_table_insert(hon->id2nick,GINT_TO_POINTER(account_id),g_strdup(nick));
	}
	g_free(buf);
}
void hon_parse_chat_leave(PurpleConnection *gc,int fd){
	hon_account* hon = gc->proto_data;
	guint32 account_id;
	guint32 chan_id;
	gchar* nick;
	PurpleConversation* conv;
	account_id = read_guint32(fd);
	chan_id = read_guint32(fd);
	nick = g_hash_table_lookup(hon->id2nick,GINT_TO_POINTER(account_id));
	conv = purple_find_chat(gc,chan_id);
	if (conv && nick)
	{
		purple_conv_chat_remove_user(PURPLE_CONV_CHAT(conv),nick,"");
	}
	if (account_id == hon->self.account_id)
		serv_got_chat_left(gc, chan_id);
}
void hon_parse_clan_message(PurpleConnection *gc,int fd){
	hon_account* hon = gc->proto_data;
	guint32 buddy_id;
	gchar* message,*user,*buffer;
	PurpleConversation* clanConv;
	GString* clan_chat_name;

	buddy_id = read_guint32(fd);

	buffer = read_string(fd);
	message = hon2html(buffer);
	g_free(buffer);
	
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
void hon_parse_chat_topic(PurpleConnection* gc,int fd){
	PurpleConversation *convo;
	hon_account* hon = gc->proto_data;
	gchar * topic_raw, * topic_html, * msg;
	guint32 chan_id = read_guint32(fd);
	gchar* buffer = read_string(fd);

	convo = purple_find_chat(gc,chan_id);
	if (!convo) {
		purple_debug(PURPLE_DEBUG_ERROR, HON_DEBUG_PREFIX, "Got a topic for %d, which doesn't exist\n", chan_id);
	}
	else
	{
		topic_raw = hon_strip(buffer,FALSE);
		topic_html = hon2html(buffer);
		msg = g_strdup_printf(_("Topic changed to '%s'."), topic_html);

		purple_conv_chat_set_topic(PURPLE_CONV_CHAT(convo), NULL, topic_raw);
		purple_conv_chat_write(PURPLE_CONV_CHAT(convo), "", msg, PURPLE_MESSAGE_SYSTEM, time(NULL));

		g_free(topic_raw);
		g_free(topic_html);
		g_free(msg);
	}
	g_free(buffer);
}

void hon_parse_userinfo(PurpleConnection* gc,int fd,guint16 packet_id){
	hon_account* hon = gc->proto_data;
	/* TODO: this is not right .. conversation could be closed already */
	gchar* message = NULL;
	gchar* name = NULL,*strtime = NULL;
	gchar* user = read_string(fd);
	gchar* buffer;
	if (!hon->whois_conv)
		return;

	switch (packet_id){
	case 0x2b:
		message = g_strdup_printf(_("Cannot find user %s"),user);
		break;
	case 0x2c:
		buffer = read_string(fd);
		message = g_strdup_printf(_("User %s is offline, last seen %s"),user,buffer);
		g_free(buffer);
		break;
	case 0x2d:
		{
			GString* msg = g_string_new(NULL);
			guint32 chan_count = read_guint32(fd);
			if (chan_count > 0)
				g_string_printf(msg,_("User %s is online and in channels: "),user);
			else
				g_string_printf(msg,_("User %s is online."),user);
			while (chan_count--)
			{
				buffer = read_string(fd);
				msg = g_string_append(msg,buffer);
				g_free(buffer);
				if (chan_count == 0)
					msg = g_string_append(msg,".");
				else
					msg = g_string_append(msg,", ");
			}
			message = g_string_free(msg,FALSE);
		}
		break;
	case 0x2e:
		name = read_string(fd);
		strtime = read_string(fd);
		message = g_strdup_printf(_("User %s is ingame, game name: %s, game time: %s"),user,name,strtime);
		break;
	}

	purple_conversation_write(hon->whois_conv, "",message, PURPLE_MESSAGE_SYSTEM|PURPLE_MESSAGE_NO_LOG, time(NULL));
	g_free(message);
	
	
#ifdef MINBIF
	if (packet_id == 0x2e)
		message = g_strdup_printf("%s %s %d %s %s", MINBIF_INFO,user,packet_id, name,strtime);
	else
		message = g_strdup_printf("%s %s %d", MINBIF_INFO,user,packet_id);
	serv_got_im(gc,MINBIF_USER,message,PURPLE_MESSAGE_RECV,time(NULL));
	g_free(message);
#endif
	g_free(name);
	g_free(strtime);
	g_free(user);
	hon->whois_conv = NULL;
}


/* packet creators */
gboolean hon_send_pong(PurpleConnection *gc){
	return hon_send_packet(gc,HON_CS_PONG/*0x02*/,"");
}
gboolean hon_send_login(PurpleConnection *gc, const gchar* cookie){
	hon_account* hon = gc->proto_data;
	return hon_send_packet(gc,HON_CS_AUTH_INFO/*0xFF*/,"isi",hon->self.account_id,hon->cookie,HON_PROTOCOL_VERSION);
}
gboolean hon_send_pm(PurpleConnection* gc,const gchar *username,const gchar* message){
	return hon_send_packet(gc,HON_CS_PM/*0x1C*/,"ss",username,message);
}
gboolean hon_send_join_chat(PurpleConnection* gc,const gchar *room){
	return hon_send_packet(gc,HON_CS_JOIN_CHANNEL/*0x1e*/,"s",room);
}
gboolean hon_send_join_chat_password(PurpleConnection* gc,const gchar *room,const gchar* password){
	return hon_send_packet(gc,HON_CS_JOIN_CHANNEL_PASSWORD/*0x46*/,"ss",room,password);
}
gboolean hon_send_leave_chat(PurpleConnection* gc,gchar* name){
	return hon_send_packet(gc,HON_CS_LEAVE_CHANNEL/*0x22*/,"s",name);
}
gboolean hon_send_chat_message(PurpleConnection *gc, guint32 id, const char *message){
	return hon_send_packet(gc,HON_CS_CHANNEL_MSG/*0x03*/,"si",message,id);
}
gboolean hon_send_chat_topic(PurpleConnection *gc, guint32 id, const char *topic){
	return hon_send_packet(gc,HON_CS_UPDATE_TOPIC/*0x30*/,"is",id,topic);
}
gboolean hon_send_room_list_request(PurpleConnection *gc){
	return hon_send_packet(gc,HON_CS_CHANNEL_LIST/*0x1F*/,"");
}
gboolean hon_send_whisper(PurpleConnection* gc,const gchar *username,const gchar* message){
	return hon_send_packet(gc,HON_CS_WHISPER/*0x08*/,"ss",username,message);
}
gboolean hon_send_clan_invite(PurpleConnection* gc,const gchar *username){
	return hon_send_packet(gc,HON_CS_CLAN_ADD_MEMBER/*0x47*/,"s",username);
}
gboolean hon_send_clan_message(PurpleConnection* gc,const gchar *message){
	return hon_send_packet(gc,HON_CS_CLAN_MESSAGE/*0x13*/,"s",message);
}
gboolean hon_send_whois(PurpleConnection* gc,const gchar *username){
	return hon_send_packet(gc,HON_CS_USER_INFO/*0x2a*/,"s",username);
}

gboolean hon_send_remove_buddy_notification(PurpleConnection* gc,guint32 buddyid, guint32 code1, guint32 code2){
	return hon_send_packet(gc,HON_CS_BUDDY_REMOVE_NOTIFY/*0x0e*/,"iii",buddyid,code1,code2);
}
gboolean hon_send_add_buddy_notification(PurpleConnection* gc,guint32 buddyid, guint32 code1, guint32 code2){
	return hon_send_packet(gc,HON_CS_BUDDY_ADD_NOTIFY/*0x0d*/,"iii",buddyid,code1,code2);
}
gboolean hon_send_channel_kick(PurpleConnection* gc,guint32 chatid, guint32 kickedid){
	return hon_send_packet(gc,HON_CS_CHANNEL_KICK/*0x31*/,"ii",chatid,kickedid);
}
gboolean hon_send_channel_promote(PurpleConnection* gc,guint32 chatid, guint32 promotedid){
	return hon_send_packet(gc,HON_CS_CHANNEL_PROMOTE/*0x3A*/,"ii",chatid,promotedid);
}
gboolean hon_send_channel_demote(PurpleConnection* gc,guint32 chatid, guint32 demotedid){
	return hon_send_packet(gc,HON_CS_CHANNEL_DEMOTE/*0x3B*/,"ii",chatid,demotedid);
}
gboolean hon_send_channel_ban(PurpleConnection* gc,guint32 chatid, const gchar* banned){
	return hon_send_packet(gc,HON_CS_CHANNEL_BAN/*0x32*/,"is",chatid,banned);
}
gboolean hon_send_channel_unban(PurpleConnection* gc,guint32 chatid, const gchar* banned){
	return hon_send_packet(gc,HON_CS_CHANNEL_UNBAN/*0x33*/,"is",chatid,banned);
}
gboolean hon_send_channel_password(PurpleConnection* gc,guint32 chatid, const gchar* password){
	return hon_send_packet(gc,HON_CS_CHANNEL_SET_PASSWORD/*0x33*/,"is",chatid,password);
}
gboolean hon_send_channel_silence(PurpleConnection* gc,guint32 chatid, const gchar* user,guint32 duration){
	return hon_send_packet(gc,HON_CS_CHANNEL_SILENCE_USER/*0x38*/,"isi",chatid,user,duration);
}
gboolean hon_send_channel_auth_enable(PurpleConnection* gc,guint32 chatid){
	return hon_send_packet(gc,HON_CS_CHANNEL_AUTH_ENABLE,"i",chatid);
}
gboolean hon_send_channel_auth_disable(PurpleConnection* gc,guint32 chatid){
	return hon_send_packet(gc,HON_CS_CHANNEL_AUTH_DISABLE,"i",chatid);
}
gboolean hon_send_channel_auth_add(PurpleConnection* gc,guint32 chatid,gchar* username){
	return hon_send_packet(gc,HON_CS_CHANNEL_AUTH_ADD,"is",chatid,username);
}
gboolean hon_send_channel_auth_delete(PurpleConnection* gc,guint32 chatid,gchar* username){
	return hon_send_packet(gc,HON_CS_CHANNEL_AUTH_DELETE,"is",chatid,username);
}
gboolean hon_send_channel_auth_list(PurpleConnection* gc,guint32 chatid){
	return hon_send_packet(gc,HON_CS_CHANNEL_AUTH_LIST,"i",chatid);
}
gboolean hon_send_emote(PurpleConnection* gc,guint32 chatid,const gchar* string){
	return hon_send_packet(gc,HON_CS_CHANNEL_EMOTE,"si",string,chatid);
}
gboolean hon_send_join_game(PurpleConnection* gc,const gchar* status,guint32 matchid,gchar* server){
	return hon_send_packet(gc,HON_CS_JOIN_GAME,"sis",status,matchid,server);
}
gboolean hon_send_whisper_buddies(PurpleConnection* gc,const gchar* message){
	return hon_send_packet(gc,HON_CS_WHISPER_BUDDIES,"s",message);
}
