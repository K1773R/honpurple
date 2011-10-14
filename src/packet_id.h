
/* server -> client */
#define	 HON_SC_AUTH_ACCEPTED 	 0x1C00
#define	 HON_SC_PING 	 0x2A00
#define	 HON_SC_CHANNEL_MSG 	 0x03
#define	 HON_SC_CHANGED_CHANNEL 	 0x04
#define	 HON_SC_JOINED_CHANNEL 	 0x05
#define	 HON_SC_LEFT_CHANNEL 	 0x06
#define	 HON_SC_WHISPER 	 0x08
#define	 HON_SC_WHISPER_FAILED 	 0x09
#define	 HON_SC_INITIAL_STATUS 	 0x0B
#define	 HON_SC_UPDATE_STATUS 	 0x0C
#define	 HON_SC_CLAN_MESSAGE 	 0x13
#define	 HON_SC_LOOKING_FOR_CLAN 	 0x18
#define	 HON_SC_PM 	 0x1C
#define	 HON_SC_PM_FAILED 	 0x1D
#define	 HON_SC_WHISPER_BUDDIES 	 0x20
#define	 HON_SC_MAX_CHANNELS 	 0x21
#define	 HON_SC_USER_INFO_NO_EXIST 	 0x2B
#define	 HON_SC_USER_INFO_OFFLINE 	 0x2C
#define	 HON_SC_USER_INFO_ONLINE 	 0x2D
#define	 HON_SC_USER_INFO_IN_GAME 	 0x2E
#define	 HON_SC_CHANNEL_UPDATE 	 0x2F
#define	 HON_SC_UPDATE_TOPIC 	 0x30
#define	 HON_SC_CHANNEL_KICK 	 0x31
#define	 HON_SC_CHANNEL_BAN 	 0x32
#define	 HON_SC_CHANNEL_UNBAN 	 0x33
#define	 HON_SC_CHANNEL_BANNED 	 0x34
#define	 HON_SC_CHANNEL_SILENCED 	 0x35
#define	 HON_SC_CHANNEL_SILENCE_LIFTED 	 0x36
#define	 HON_SC_CHANNEL_SILENCE_PLACED 	 0x37
#define	 HON_SC_MESSAGE_ALL 	 0x39
#define	 HON_SC_CHANNEL_PROMOTE 	 0x3A
#define	 HON_SC_CHANNEL_DEMOTE 	 0x3B
#define	 HON_SC_CHANNEL_AUTH_ENABLE 	 0x3E
#define	 HON_SC_CHANNEL_AUTH_DISABLE		0x3F
#define	 HON_SC_CHANNEL_AUTH_ADD 	 0x40
#define	 HON_SC_CHANNEL_AUTH_DELETE		0x41
#define	 HON_SC_CHANNEL_AUTH_LIST 	 0x42
#define	 HON_SC_CHANNEL_PASSWORD_CHANGED 	 0x43
#define	 HON_SC_CHANNEL_ADD_AUTH_FAIL	0x44
#define	 HON_SC_CHANNEL_DEL_AUTH_FAIL	0x45
#define	 HON_SC_JOIN_CHANNEL_PASSWORD 	 0x46
#define	 HON_SC_CHANNEL_EMOTE 	 0x65
#define	 HON_SC_TOTAL_ONLINE 	 0x68
#define	 HON_SC_REQUEST_NOTIFICATION 	 0xB2
#define	 HON_SC_NOTIFICATION 	 0xB4

/* client -> server */
#define	 HON_CS_PONG 	 0x2A01
#define	 HON_CS_CHANNEL_MSG 	 0x03
#define	 HON_CS_WHISPER 	 0x08
#define	 HON_CS_AUTH_INFO 	 0x0C00
#define	 HON_CS_BUDDY_ADD_NOTIFY 	 0x0D
#define	 HON_CS_JOIN_GAME 	 0x10
#define	 HON_CS_CLAN_MESSAGE 	 0x13
#define	 HON_CS_PM 	 0x1C
#define	 HON_CS_JOIN_CHANNEL 	 0x1E
#define	 HON_CS_WHISPER_BUDDIES 	 0x20
#define	 HON_CS_LEAVE_CHANNEL 	 0x22
#define	 HON_CS_USER_INFO 	 0x2A
#define	 HON_CS_UPDATE_TOPIC 	 0x30
#define	 HON_CS_CHANNEL_KICK 	 0x31
#define	 HON_CS_CHANNEL_BAN 	 0x33
#define	 HON_CS_CHANNEL_UNBAN 	 0x32
#define	 HON_CS_CHANNEL_SILENCE_USER 	 0x38
#define	 HON_CS_CHANNEL_PROMOTE 	 0x3A
#define	 HON_CS_CHANNEL_DEMOTE 	 0x3B
#define	 HON_CS_CHANNEL_AUTH_ENABLE 	 0x3E
#define	 HON_CS_CHANNEL_AUTH_DISABLE		0x3F
#define	 HON_CS_CHANNEL_AUTH_ADD 	 0x40
#define	 HON_CS_CHANNEL_AUTH_DELETE		0x41
#define	 HON_CS_CHANNEL_AUTH_LIST 	 0x42
#define	 HON_CS_CHANNEL_SET_PASSWORD 	 0x43
#define	 HON_CS_JOIN_CHANNEL_PASSWORD 	 0x46
#define	 HON_CS_CLAN_ADD_MEMBER 	 0x47
#define	 HON_CS_CHANNEL_EMOTE 	 0x65
#define	 HON_CS_BUDDY_ACCEPT 	 0xB3

#define HON_PROTOCOL_VERSION 19

