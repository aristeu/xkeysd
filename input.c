#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <search.h>

#include <linux/input.h>

#include "input.h"

#define HASH_SIZE 1024

static const char *input_events[EV_MAX + 1] = {
	[EV_SYN] = "EV_SYN",
	[EV_KEY] = "EV_KEY",
	[EV_REL] = "EV_REL",
	[EV_ABS] = "EV_ABS",
	[EV_MSC] = "EV_MSC",
	[EV_SW] = "EV_SW",
	[EV_LED] = "EV_LED",
	[EV_SND] = "EV_SND",
	[EV_REP] = "EV_REP",
	[EV_FF] = "EV_FF",
	[EV_PWR] = "EV_PWR",
	[EV_FF_STATUS] = "EV_FF_STATUS",
};

#define SYN_MAX 2
static const char *input_syn_events[SYN_MAX + 1] = {
	[SYN_REPORT] = "SYN_REPORT",
	[SYN_CONFIG] = "SYN_CONFIG",
	[SYN_MT_REPORT] = "SYN_MT_REPORT",
};

static const char *input_key_events[KEY_MAX + 1] = {
	[KEY_RESERVED] = "KEY_RESERVED",
	[KEY_ESC] = "KEY_ESC",
	[KEY_1] = "KEY_1",
	[KEY_2] = "KEY_2",
	[KEY_3] = "KEY_3",
	[KEY_4] = "KEY_4",
	[KEY_5] = "KEY_5",
	[KEY_6] = "KEY_6",
	[KEY_7] = "KEY_7",
	[KEY_8] = "KEY_8",
	[KEY_9] = "KEY_9",
	[KEY_0] = "KEY_0",
	[KEY_MINUS] = "KEY_MINUS",
	[KEY_EQUAL] = "KEY_EQUAL",
	[KEY_BACKSPACE] = "KEY_BACKSPACE",
	[KEY_TAB] = "KEY_TAB",
	[KEY_Q] = "KEY_Q",
	[KEY_W] = "KEY_W",
	[KEY_E] = "KEY_E",
	[KEY_R] = "KEY_R",
	[KEY_T] = "KEY_T",
	[KEY_Y] = "KEY_Y",
	[KEY_U] = "KEY_U",
	[KEY_I] = "KEY_I",
	[KEY_O] = "KEY_O",
	[KEY_P] = "KEY_P",
	[KEY_LEFTBRACE] = "KEY_LEFTBRACE",
	[KEY_RIGHTBRACE] = "KEY_RIGHTBRACE",
	[KEY_ENTER] = "KEY_ENTER",
	[KEY_LEFTCTRL] = "KEY_LEFTCTRL",
	[KEY_A] = "KEY_A",
	[KEY_S] = "KEY_S",
	[KEY_D] = "KEY_D",
	[KEY_F] = "KEY_F",
	[KEY_G] = "KEY_G",
	[KEY_H] = "KEY_H",
	[KEY_J] = "KEY_J",
	[KEY_K] = "KEY_K",
	[KEY_L] = "KEY_L",
	[KEY_SEMICOLON] = "KEY_SEMICOLON",
	[KEY_APOSTROPHE] = "KEY_APOSTROPHE",
	[KEY_GRAVE] = "KEY_GRAVE",
	[KEY_LEFTSHIFT] = "KEY_LEFTSHIFT",
	[KEY_BACKSLASH] = "KEY_BACKSLASH",
	[KEY_Z] = "KEY_Z",
	[KEY_X] = "KEY_X",
	[KEY_C] = "KEY_C",
	[KEY_V] = "KEY_V",
	[KEY_B] = "KEY_B",
	[KEY_N] = "KEY_N",
	[KEY_M] = "KEY_M",
	[KEY_COMMA] = "KEY_COMMA",
	[KEY_DOT] = "KEY_DOT",
	[KEY_SLASH] = "KEY_SLASH",
	[KEY_RIGHTSHIFT] = "KEY_RIGHTSHIFT",
	[KEY_KPASTERISK] = "KEY_KPASTERISK",
	[KEY_LEFTALT] = "KEY_LEFTALT",
	[KEY_SPACE] = "KEY_SPACE",
	[KEY_CAPSLOCK] = "KEY_CAPSLOCK",
	[KEY_F1] = "KEY_F1",
	[KEY_F2] = "KEY_F2",
	[KEY_F3] = "KEY_F3",
	[KEY_F4] = "KEY_F4",
	[KEY_F5] = "KEY_F5",
	[KEY_F6] = "KEY_F6",
	[KEY_F7] = "KEY_F7",
	[KEY_F8] = "KEY_F8",
	[KEY_F9] = "KEY_F9",
	[KEY_F10] = "KEY_F10",
	[KEY_NUMLOCK] = "KEY_NUMLOCK",
	[KEY_SCROLLLOCK] = "KEY_SCROLLLOCK",
	[KEY_KP7] = "KEY_KP7",
	[KEY_KP8] = "KEY_KP8",
	[KEY_KP9] = "KEY_KP9",
	[KEY_KPMINUS] = "KEY_KPMINUS",
	[KEY_KP4] = "KEY_KP4",
	[KEY_KP5] = "KEY_KP5",
	[KEY_KP6] = "KEY_KP6",
	[KEY_KPPLUS] = "KEY_KPPLUS",
	[KEY_KP1] = "KEY_KP1",
	[KEY_KP2] = "KEY_KP2",
	[KEY_KP3] = "KEY_KP3",
	[KEY_KP0] = "KEY_KP0",
	[KEY_KPDOT] = "KEY_KPDOT",
	[KEY_ZENKAKUHANKAKU] = "KEY_ZENKAKUHANKAKU",
	[KEY_102ND] = "KEY_102ND",
	[KEY_F11] = "KEY_F11",
	[KEY_F12] = "KEY_F12",
	[KEY_RO] = "KEY_RO",
	[KEY_KATAKANA] = "KEY_KATAKANA",
	[KEY_HIRAGANA] = "KEY_HIRAGANA",
	[KEY_HENKAN] = "KEY_HENKAN",
	[KEY_KATAKANAHIRAGANA] = "KEY_KATAKANAHIRAGANA",
	[KEY_MUHENKAN] = "KEY_MUHENKAN",
	[KEY_KPJPCOMMA] = "KEY_KPJPCOMMA",
	[KEY_KPENTER] = "KEY_KPENTER",
	[KEY_RIGHTCTRL] = "KEY_RIGHTCTRL",
	[KEY_KPSLASH] = "KEY_KPSLASH",
	[KEY_SYSRQ] = "KEY_SYSRQ",
	[KEY_RIGHTALT] = "KEY_RIGHTALT",
	[KEY_LINEFEED] = "KEY_LINEFEED",
	[KEY_HOME] = "KEY_HOME",
	[KEY_UP] = "KEY_UP",
	[KEY_PAGEUP] = "KEY_PAGEUP",
	[KEY_LEFT] = "KEY_LEFT",
	[KEY_RIGHT] = "KEY_RIGHT",
	[KEY_END] = "KEY_END",
	[KEY_DOWN] = "KEY_DOWN",
	[KEY_PAGEDOWN] = "KEY_PAGEDOWN",
	[KEY_INSERT] = "KEY_INSERT",
	[KEY_DELETE] = "KEY_DELETE",
	[KEY_MACRO] = "KEY_MACRO",
	[KEY_MUTE] = "KEY_MUTE",
	[KEY_VOLUMEDOWN] = "KEY_VOLUMEDOWN",
	[KEY_VOLUMEUP] = "KEY_VOLUMEUP",
	[KEY_POWER] = "KEY_POWER",
	[KEY_KPEQUAL] = "KEY_KPEQUAL",
	[KEY_KPPLUSMINUS] = "KEY_KPPLUSMINUS",
	[KEY_PAUSE] = "KEY_PAUSE",
	[KEY_SCALE] = "KEY_SCALE",
	[KEY_KPCOMMA] = "KEY_KPCOMMA",
	[KEY_HANGEUL] = "KEY_HANGEUL",
	[KEY_HANGUEL] = "KEY_HANGUEL",
	[KEY_HANJA] = "KEY_HANJA",
	[KEY_YEN] = "KEY_YEN",
	[KEY_LEFTMETA] = "KEY_LEFTMETA",
	[KEY_RIGHTMETA] = "KEY_RIGHTMETA",
	[KEY_COMPOSE] = "KEY_COMPOSE",
	[KEY_STOP] = "KEY_STOP",
	[KEY_AGAIN] = "KEY_AGAIN",
	[KEY_PROPS] = "KEY_PROPS",
	[KEY_UNDO] = "KEY_UNDO",
	[KEY_FRONT] = "KEY_FRONT",
	[KEY_COPY] = "KEY_COPY",
	[KEY_OPEN] = "KEY_OPEN",
	[KEY_PASTE] = "KEY_PASTE",
	[KEY_FIND] = "KEY_FIND",
	[KEY_CUT] = "KEY_CUT",
	[KEY_HELP] = "KEY_HELP",
	[KEY_MENU] = "KEY_MENU",
	[KEY_CALC] = "KEY_CALC",
	[KEY_SETUP] = "KEY_SETUP",
	[KEY_SLEEP] = "KEY_SLEEP",
	[KEY_WAKEUP] = "KEY_WAKEUP",
	[KEY_FILE] = "KEY_FILE",
	[KEY_SENDFILE] = "KEY_SENDFILE",
	[KEY_DELETEFILE] = "KEY_DELETEFILE",
	[KEY_XFER] = "KEY_XFER",
	[KEY_PROG1] = "KEY_PROG1",
	[KEY_PROG2] = "KEY_PROG2",
	[KEY_WWW] = "KEY_WWW",
	[KEY_MSDOS] = "KEY_MSDOS",
	[KEY_COFFEE] = "KEY_COFFEE",
	[KEY_SCREENLOCK] = "KEY_SCREENLOCK",
	[KEY_DIRECTION] = "KEY_DIRECTION",
	[KEY_CYCLEWINDOWS] = "KEY_CYCLEWINDOWS",
	[KEY_MAIL] = "KEY_MAIL",
	[KEY_BOOKMARKS] = "KEY_BOOKMARKS",
	[KEY_COMPUTER] = "KEY_COMPUTER",
	[KEY_BACK] = "KEY_BACK",
	[KEY_FORWARD] = "KEY_FORWARD",
	[KEY_CLOSECD] = "KEY_CLOSECD",
	[KEY_EJECTCD] = "KEY_EJECTCD",
	[KEY_EJECTCLOSECD] = "KEY_EJECTCLOSECD",
	[KEY_NEXTSONG] = "KEY_NEXTSONG",
	[KEY_PLAYPAUSE] = "KEY_PLAYPAUSE",
	[KEY_PREVIOUSSONG] = "KEY_PREVIOUSSONG",
	[KEY_STOPCD] = "KEY_STOPCD",
	[KEY_RECORD] = "KEY_RECORD",
	[KEY_REWIND] = "KEY_REWIND",
	[KEY_PHONE] = "KEY_PHONE",
	[KEY_ISO] = "KEY_ISO",
	[KEY_CONFIG] = "KEY_CONFIG",
	[KEY_HOMEPAGE] = "KEY_HOMEPAGE",
	[KEY_REFRESH] = "KEY_REFRESH",
	[KEY_EXIT] = "KEY_EXIT",
	[KEY_MOVE] = "KEY_MOVE",
	[KEY_EDIT] = "KEY_EDIT",
	[KEY_SCROLLUP] = "KEY_SCROLLUP",
	[KEY_SCROLLDOWN] = "KEY_SCROLLDOWN",
	[KEY_KPLEFTPAREN] = "KEY_KPLEFTPAREN",
	[KEY_KPRIGHTPAREN] = "KEY_KPRIGHTPAREN",
	[KEY_NEW] = "KEY_NEW",
	[KEY_REDO] = "KEY_REDO",
	[KEY_F13] = "KEY_F13",
	[KEY_F14] = "KEY_F14",
	[KEY_F15] = "KEY_F15",
	[KEY_F16] = "KEY_F16",
	[KEY_F17] = "KEY_F17",
	[KEY_F18] = "KEY_F18",
	[KEY_F19] = "KEY_F19",
	[KEY_F20] = "KEY_F20",
	[KEY_F21] = "KEY_F21",
	[KEY_F22] = "KEY_F22",
	[KEY_F23] = "KEY_F23",
	[KEY_F24] = "KEY_F24",
	[KEY_PLAYCD] = "KEY_PLAYCD",
	[KEY_PAUSECD] = "KEY_PAUSECD",
	[KEY_PROG3] = "KEY_PROG3",
	[KEY_PROG4] = "KEY_PROG4",
	[KEY_DASHBOARD] = "KEY_DASHBOARD",
	[KEY_SUSPEND] = "KEY_SUSPEND",
	[KEY_CLOSE] = "KEY_CLOSE",
	[KEY_PLAY] = "KEY_PLAY",
	[KEY_FASTFORWARD] = "KEY_FASTFORWARD",
	[KEY_BASSBOOST] = "KEY_BASSBOOST",
	[KEY_PRINT] = "KEY_PRINT",
	[KEY_HP] = "KEY_HP",
	[KEY_CAMERA] = "KEY_CAMERA",
	[KEY_SOUND] = "KEY_SOUND",
	[KEY_QUESTION] = "KEY_QUESTION",
	[KEY_EMAIL] = "KEY_EMAIL",
	[KEY_CHAT] = "KEY_CHAT",
	[KEY_SEARCH] = "KEY_SEARCH",
	[KEY_CONNECT] = "KEY_CONNECT",
	[KEY_FINANCE] = "KEY_FINANCE",
	[KEY_SPORT] = "KEY_SPORT",
	[KEY_SHOP] = "KEY_SHOP",
	[KEY_ALTERASE] = "KEY_ALTERASE",
	[KEY_CANCEL] = "KEY_CANCEL",
	[KEY_BRIGHTNESSDOWN] = "KEY_BRIGHTNESSDOWN",
	[KEY_BRIGHTNESSUP] = "KEY_BRIGHTNESSUP",
	[KEY_MEDIA] = "KEY_MEDIA",
	[KEY_SWITCHVIDEOMODE] = "KEY_SWITCHVIDEOMODE",
	[KEY_KBDILLUMTOGGLE] = "KEY_KBDILLUMTOGGLE",
	[KEY_KBDILLUMDOWN] = "KEY_KBDILLUMDOWN",
	[KEY_KBDILLUMUP] = "KEY_KBDILLUMUP",
	[KEY_SEND] = "KEY_SEND",
	[KEY_REPLY] = "KEY_REPLY",
	[KEY_FORWARDMAIL] = "KEY_FORWARDMAIL",
	[KEY_SAVE] = "KEY_SAVE",
	[KEY_DOCUMENTS] = "KEY_DOCUMENTS",
	[KEY_BATTERY] = "KEY_BATTERY",
	[KEY_BLUETOOTH] = "KEY_BLUETOOTH",
	[KEY_WLAN] = "KEY_WLAN",
	[KEY_UWB] = "KEY_UWB",
	[KEY_UNKNOWN] = "KEY_UNKNOWN",
	[KEY_VIDEO_NEXT] = "KEY_VIDEO_NEXT",
	[KEY_VIDEO_PREV] = "KEY_VIDEO_PREV",
	[KEY_BRIGHTNESS_CYCLE] = "KEY_BRIGHTNESS_CYCLE",
	[KEY_BRIGHTNESS_ZERO] = "KEY_BRIGHTNESS_ZERO",
	[KEY_DISPLAY_OFF] = "KEY_DISPLAY_OFF",
	[KEY_WIMAX] = "KEY_WIMAX",
	[KEY_OK] = "KEY_OK",
	[KEY_SELECT] = "KEY_SELECT",
	[KEY_GOTO] = "KEY_GOTO",
	[KEY_CLEAR] = "KEY_CLEAR",
	[KEY_POWER2] = "KEY_POWER2",
	[KEY_OPTION] = "KEY_OPTION",
	[KEY_INFO] = "KEY_INFO",
	[KEY_TIME] = "KEY_TIME",
	[KEY_VENDOR] = "KEY_VENDOR",
	[KEY_ARCHIVE] = "KEY_ARCHIVE",
	[KEY_PROGRAM] = "KEY_PROGRAM",
	[KEY_CHANNEL] = "KEY_CHANNEL",
	[KEY_FAVORITES] = "KEY_FAVORITES",
	[KEY_EPG] = "KEY_EPG",
	[KEY_PVR] = "KEY_PVR",
	[KEY_MHP] = "KEY_MHP",
	[KEY_LANGUAGE] = "KEY_LANGUAGE",
	[KEY_TITLE] = "KEY_TITLE",
	[KEY_SUBTITLE] = "KEY_SUBTITLE",
	[KEY_ANGLE] = "KEY_ANGLE",
	[KEY_ZOOM] = "KEY_ZOOM",
	[KEY_MODE] = "KEY_MODE",
	[KEY_KEYBOARD] = "KEY_KEYBOARD",
	[KEY_SCREEN] = "KEY_SCREEN",
	[KEY_PC] = "KEY_PC",
	[KEY_TV] = "KEY_TV",
	[KEY_TV2] = "KEY_TV2",
	[KEY_VCR] = "KEY_VCR",
	[KEY_VCR2] = "KEY_VCR2",
	[KEY_SAT] = "KEY_SAT",
	[KEY_SAT2] = "KEY_SAT2",
	[KEY_CD] = "KEY_CD",
	[KEY_TAPE] = "KEY_TAPE",
	[KEY_RADIO] = "KEY_RADIO",
	[KEY_TUNER] = "KEY_TUNER",
	[KEY_PLAYER] = "KEY_PLAYER",
	[KEY_TEXT] = "KEY_TEXT",
	[KEY_DVD] = "KEY_DVD",
	[KEY_AUX] = "KEY_AUX",
	[KEY_MP3] = "KEY_MP3",
	[KEY_AUDIO] = "KEY_AUDIO",
	[KEY_VIDEO] = "KEY_VIDEO",
	[KEY_DIRECTORY] = "KEY_DIRECTORY",
	[KEY_LIST] = "KEY_LIST",
	[KEY_MEMO] = "KEY_MEMO",
	[KEY_CALENDAR] = "KEY_CALENDAR",
	[KEY_RED] = "KEY_RED",
	[KEY_GREEN] = "KEY_GREEN",
	[KEY_YELLOW] = "KEY_YELLOW",
	[KEY_BLUE] = "KEY_BLUE",
	[KEY_CHANNELUP] = "KEY_CHANNELUP",
	[KEY_CHANNELDOWN] = "KEY_CHANNELDOWN",
	[KEY_FIRST] = "KEY_FIRST",
	[KEY_LAST] = "KEY_LAST",
	[KEY_AB] = "KEY_AB",
	[KEY_NEXT] = "KEY_NEXT",
	[KEY_RESTART] = "KEY_RESTART",
	[KEY_SLOW] = "KEY_SLOW",
	[KEY_SHUFFLE] = "KEY_SHUFFLE",
	[KEY_BREAK] = "KEY_BREAK",
	[KEY_PREVIOUS] = "KEY_PREVIOUS",
	[KEY_DIGITS] = "KEY_DIGITS",
	[KEY_TEEN] = "KEY_TEEN",
	[KEY_TWEN] = "KEY_TWEN",
	[KEY_VIDEOPHONE] = "KEY_VIDEOPHONE",
	[KEY_GAMES] = "KEY_GAMES",
	[KEY_ZOOMIN] = "KEY_ZOOMIN",
	[KEY_ZOOMOUT] = "KEY_ZOOMOUT",
	[KEY_ZOOMRESET] = "KEY_ZOOMRESET",
	[KEY_WORDPROCESSOR] = "KEY_WORDPROCESSOR",
	[KEY_EDITOR] = "KEY_EDITOR",
	[KEY_SPREADSHEET] = "KEY_SPREADSHEET",
	[KEY_GRAPHICSEDITOR] = "KEY_GRAPHICSEDITOR",
	[KEY_PRESENTATION] = "KEY_PRESENTATION",
	[KEY_DATABASE] = "KEY_DATABASE",
	[KEY_NEWS] = "KEY_NEWS",
	[KEY_VOICEMAIL] = "KEY_VOICEMAIL",
	[KEY_ADDRESSBOOK] = "KEY_ADDRESSBOOK",
	[KEY_MESSENGER] = "KEY_MESSENGER",
	[KEY_DISPLAYTOGGLE] = "KEY_DISPLAYTOGGLE",
	[KEY_SPELLCHECK] = "KEY_SPELLCHECK",
	[KEY_LOGOFF] = "KEY_LOGOFF",
	[KEY_DOLLAR] = "KEY_DOLLAR",
	[KEY_EURO] = "KEY_EURO",
	[KEY_FRAMEBACK] = "KEY_FRAMEBACK",
	[KEY_FRAMEFORWARD] = "KEY_FRAMEFORWARD",
	[KEY_CONTEXT_MENU] = "KEY_CONTEXT_MENU",
	[KEY_MEDIA_REPEAT] = "KEY_MEDIA_REPEAT",
	[KEY_10CHANNELSUP] = "KEY_10CHANNELSUP",
	[KEY_10CHANNELSDOWN] = "KEY_10CHANNELSDOWN",
	[KEY_DEL_EOL] = "KEY_DEL_EOL",
	[KEY_DEL_EOS] = "KEY_DEL_EOS",
	[KEY_INS_LINE] = "KEY_INS_LINE",
	[KEY_DEL_LINE] = "KEY_DEL_LINE",
	[KEY_FN] = "KEY_FN",
	[KEY_FN_ESC] = "KEY_FN_ESC",
	[KEY_FN_F1] = "KEY_FN_F1",
	[KEY_FN_F2] = "KEY_FN_F2",
	[KEY_FN_F3] = "KEY_FN_F3",
	[KEY_FN_F4] = "KEY_FN_F4",
	[KEY_FN_F5] = "KEY_FN_F5",
	[KEY_FN_F6] = "KEY_FN_F6",
	[KEY_FN_F7] = "KEY_FN_F7",
	[KEY_FN_F8] = "KEY_FN_F8",
	[KEY_FN_F9] = "KEY_FN_F9",
	[KEY_FN_F10] = "KEY_FN_F10",
	[KEY_FN_F11] = "KEY_FN_F11",
	[KEY_FN_F12] = "KEY_FN_F12",
	[KEY_FN_1] = "KEY_FN_1",
	[KEY_FN_2] = "KEY_FN_2",
	[KEY_FN_D] = "KEY_FN_D",
	[KEY_FN_E] = "KEY_FN_E",
	[KEY_FN_F] = "KEY_FN_F",
	[KEY_FN_S] = "KEY_FN_S",
	[KEY_FN_B] = "KEY_FN_B",
	[KEY_BRL_DOT1] = "KEY_BRL_DOT1",
	[KEY_BRL_DOT2] = "KEY_BRL_DOT2",
	[KEY_BRL_DOT3] = "KEY_BRL_DOT3",
	[KEY_BRL_DOT4] = "KEY_BRL_DOT4",
	[KEY_BRL_DOT5] = "KEY_BRL_DOT5",
	[KEY_BRL_DOT6] = "KEY_BRL_DOT6",
	[KEY_BRL_DOT7] = "KEY_BRL_DOT7",
	[KEY_BRL_DOT8] = "KEY_BRL_DOT8",
	[KEY_BRL_DOT9] = "KEY_BRL_DOT9",
	[KEY_BRL_DOT10] = "KEY_BRL_DOT10",
	[KEY_NUMERIC_0] = "KEY_NUMERIC_0",
	[KEY_NUMERIC_1] = "KEY_NUMERIC_1",
	[KEY_NUMERIC_2] = "KEY_NUMERIC_2",
	[KEY_NUMERIC_3] = "KEY_NUMERIC_3",
	[KEY_NUMERIC_4] = "KEY_NUMERIC_4",
	[KEY_NUMERIC_5] = "KEY_NUMERIC_5",
	[KEY_NUMERIC_6] = "KEY_NUMERIC_6",
	[KEY_NUMERIC_7] = "KEY_NUMERIC_7",
	[KEY_NUMERIC_8] = "KEY_NUMERIC_8",
	[KEY_NUMERIC_9] = "KEY_NUMERIC_9",
	[KEY_NUMERIC_STAR] = "KEY_NUMERIC_STAR",
	[KEY_NUMERIC_POUND] = "KEY_NUMERIC_POUND",
	[KEY_RFKILL] = "KEY_RFKILL",
	[KEY_MIN_INTERESTING] = "KEY_MIN_INTERESTING",

	[BTN_MISC] = "BTN_MISC",
	[BTN_0] = "BTN_0",
	[BTN_1] = "BTN_1",
	[BTN_2] = "BTN_2",
	[BTN_3] = "BTN_3",
	[BTN_4] = "BTN_4",
	[BTN_5] = "BTN_5",
	[BTN_6] = "BTN_6",
	[BTN_7] = "BTN_7",
	[BTN_8] = "BTN_8",
	[BTN_9] = "BTN_9",
	[BTN_MOUSE] = "BTN_MOUSE",
	[BTN_LEFT] = "BTN_LEFT",
	[BTN_RIGHT] = "BTN_RIGHT",
	[BTN_MIDDLE] = "BTN_MIDDLE",
	[BTN_SIDE] = "BTN_SIDE",
	[BTN_EXTRA] = "BTN_EXTRA",
	[BTN_FORWARD] = "BTN_FORWARD",
	[BTN_BACK] = "BTN_BACK",
	[BTN_TASK] = "BTN_TASK",
	[BTN_JOYSTICK] = "BTN_JOYSTICK",
	[BTN_TRIGGER] = "BTN_TRIGGER",
	[BTN_THUMB] = "BTN_THUMB",
	[BTN_THUMB2] = "BTN_THUMB2",
	[BTN_TOP] = "BTN_TOP",
	[BTN_TOP2] = "BTN_TOP2",
	[BTN_PINKIE] = "BTN_PINKIE",
	[BTN_BASE] = "BTN_BASE",
	[BTN_BASE2] = "BTN_BASE2",
	[BTN_BASE3] = "BTN_BASE3",
	[BTN_BASE4] = "BTN_BASE4",
	[BTN_BASE5] = "BTN_BASE5",
	[BTN_BASE6] = "BTN_BASE6",
	[BTN_DEAD] = "BTN_DEAD",
	[BTN_GAMEPAD] = "BTN_GAMEPAD",
	[BTN_A] = "BTN_A",
	[BTN_B] = "BTN_B",
	[BTN_C] = "BTN_C",
	[BTN_X] = "BTN_X",
	[BTN_Y] = "BTN_Y",
	[BTN_Z] = "BTN_Z",
	[BTN_TL] = "BTN_TL",
	[BTN_TR] = "BTN_TR",
	[BTN_TL2] = "BTN_TL2",
	[BTN_TR2] = "BTN_TR2",
	[BTN_SELECT] = "BTN_SELECT",
	[BTN_START] = "BTN_START",
	[BTN_MODE] = "BTN_MODE",
	[BTN_THUMBL] = "BTN_THUMBL",
	[BTN_THUMBR] = "BTN_THUMBR",
	[BTN_DIGI] = "BTN_DIGI",
	[BTN_TOOL_PEN] = "BTN_TOOL_PEN",
	[BTN_TOOL_RUBBER] = "BTN_TOOL_RUBBER",
	[BTN_TOOL_BRUSH] = "BTN_TOOL_BRUSH",
	[BTN_TOOL_PENCIL] = "BTN_TOOL_PENCIL",
	[BTN_TOOL_AIRBRUSH] = "BTN_TOOL_AIRBRUSH",
	[BTN_TOOL_FINGER] = "BTN_TOOL_FINGER",
	[BTN_TOOL_MOUSE] = "BTN_TOOL_MOUSE",
	[BTN_TOOL_LENS] = "BTN_TOOL_LENS",
	[BTN_TOUCH] = "BTN_TOUCH",
	[BTN_STYLUS] = "BTN_STYLUS",
	[BTN_STYLUS2] = "BTN_STYLUS2",
	[BTN_TOOL_DOUBLETAP] = "BTN_TOOL_DOUBLETAP",
	[BTN_TOOL_TRIPLETAP] = "BTN_TOOL_TRIPLETAP",
	[BTN_TOOL_QUADTAP] = "BTN_TOOL_QUADTAP",
	[BTN_WHEEL] = "BTN_WHEEL",
	[BTN_GEAR_DOWN] = "BTN_GEAR_DOWN",
	[BTN_GEAR_UP] = "BTN_GEAR_UP",
};

static const char *input_rel_events[REL_MAX + 1] = {
	[REL_X] = "REL_X",
	[REL_Y] = "REL_Y",
	[REL_Z] = "REL_Z",
	[REL_RX] = "REL_RX",
	[REL_RY] = "REL_RY",
	[REL_RZ] = "REL_RZ",
	[REL_HWHEEL] = "REL_HWHEEL",
	[REL_DIAL] = "REL_DIAL",
	[REL_WHEEL] = "REL_WHEEL",
	[REL_MISC] = "REL_MISC",
};

static const char *input_abs_events[ABS_MAX + 1] = {
	[ABS_X] = "ABS_X",
	[ABS_Y] = "ABS_Y",
	[ABS_Z] = "ABS_Z",
	[ABS_RX] = "ABS_RX",
	[ABS_RY] = "ABS_RY",
	[ABS_RZ] = "ABS_RZ",
	[ABS_THROTTLE] = "ABS_THROTTLE",
	[ABS_RUDDER] = "ABS_RUDDER",
	[ABS_WHEEL] = "ABS_WHEEL",
	[ABS_GAS] = "ABS_GAS",
	[ABS_BRAKE] = "ABS_BRAKE",
	[ABS_HAT0X] = "ABS_HAT0X",
	[ABS_HAT0Y] = "ABS_HAT0Y",
	[ABS_HAT1X] = "ABS_HAT1X",
	[ABS_HAT1Y] = "ABS_HAT1Y",
	[ABS_HAT2X] = "ABS_HAT2X",
	[ABS_HAT2Y] = "ABS_HAT2Y",
	[ABS_HAT3X] = "ABS_HAT3X",
	[ABS_HAT3Y] = "ABS_HAT3Y",
	[ABS_PRESSURE] = "ABS_PRESSURE",
	[ABS_DISTANCE] = "ABS_DISTANCE",
	[ABS_TILT_X] = "ABS_TILT_X",
	[ABS_TILT_Y] = "ABS_TILT_Y",
	[ABS_TOOL_WIDTH] = "ABS_TOOL_WIDTH",
	[ABS_VOLUME] = "ABS_VOLUME",
	[ABS_MISC] = "ABS_MISC",
	[ABS_MT_TOUCH_MAJOR] = "ABS_MT_TOUCH_MAJOR",
	[ABS_MT_TOUCH_MINOR] = "ABS_MT_TOUCH_MINOR",
	[ABS_MT_WIDTH_MAJOR] = "ABS_MT_WIDTH_MAJOR",
	[ABS_MT_WIDTH_MINOR] = "ABS_MT_WIDTH_MINOR",
	[ABS_MT_ORIENTATION] = "ABS_MT_ORIENTATION",
	[ABS_MT_POSITION_X] = "ABS_MT_POSITION_X",
	[ABS_MT_POSITION_Y] = "ABS_MT_POSITION_Y",
	[ABS_MT_TOOL_TYPE] = "ABS_MT_TOOL_TYPE",
	[ABS_MT_BLOB_ID] = "ABS_MT_BLOB_ID",
	[ABS_MT_TRACKING_ID] = "ABS_MT_TRACKING_ID",
};

static const char *input_sw_events[SW_MAX + 1] = {
	[SW_LID] = "SW_LID",
	[SW_TABLET_MODE] = "SW_TABLET_MODE",
	[SW_HEADPHONE_INSERT] = "SW_HEADPHONE_INSERT",
	[SW_RFKILL_ALL] = "SW_RFKILL_ALL",
	[SW_RADIO] = "SW_RADIO",
	[SW_MICROPHONE_INSERT] = "SW_MICROPHONE_INSERT",
	[SW_DOCK] = "SW_DOCK",
	[SW_LINEOUT_INSERT] = "SW_LINEOUT_INSERT",
	[SW_JACK_PHYSICAL_INSERT] = "SW_JACK_PHYSICAL_INSERT",
	[SW_VIDEOOUT_INSERT] = "SW_VIDEOOUT_INSERT",
};

static const char *input_msc_events[MSC_MAX + 1] = {
	[MSC_SERIAL] = "MSC_SERIAL",
	[MSC_PULSELED] = "MSC_PULSELED",
	[MSC_GESTURE] = "MSC_GESTURE",
	[MSC_RAW] = "MSC_RAW",
	[MSC_SCAN] = "MSC_SCAN",
};

static const char *input_led_events[LED_MAX + 1] = {
	[LED_NUML] = "LED_NUML",
	[LED_CAPSL] = "LED_CAPSL",
	[LED_SCROLLL] = "LED_SCROLLL",
	[LED_COMPOSE] = "LED_COMPOSE",
	[LED_KANA] = "LED_KANA",
	[LED_SLEEP] = "LED_SLEEP",
	[LED_SUSPEND] = "LED_SUSPEND",
	[LED_MUTE] = "LED_MUTE",
	[LED_MISC] = "LED_MISC",
	[LED_MAIL] = "LED_MAIL",
	[LED_CHARGING] = "LED_CHARGING",
};

static const char *input_rep_events[REP_MAX + 1] = {
	[REP_DELAY] = "REP_DELAY",
	[REP_PERIOD] = "REP_PERIOD",
};

static const char *input_snd_events[SND_MAX + 1] = {
	[SND_CLICK] = "SND_CLICK",
	[SND_BELL] = "SND_BELL",
	[SND_TONE] = "SND_TONE",
};

static const char *input_bus_events[] = {
	[BUS_PCI] = "BUS_PCI",
	[BUS_ISAPNP] = "BUS_ISAPNP",
	[BUS_USB] = "BUS_USB",
	[BUS_HIL] = "BUS_HIL",
	[BUS_BLUETOOTH] = "BUS_BLUETOOTH",
	[BUS_VIRTUAL] = "BUS_VIRTUAL",
	[BUS_ISA] = "BUS_ISA",
	[BUS_I8042] = "BUS_I8042",
	[BUS_XTKBD] = "BUS_XTKBD",
	[BUS_RS232] = "BUS_RS232",
	[BUS_GAMEPORT] = "BUS_GAMEPORT",
	[BUS_PARPORT] = "BUS_PARPORT",
	[BUS_AMIGA] = "BUS_AMIGA",
	[BUS_ADB] = "BUS_ADB",
	[BUS_I2C] = "BUS_I2C",
	[BUS_HOST] = "BUS_HOST",
	[BUS_GSC] = "BUS_GSC",
	[BUS_ATARI] = "BUS_ATARI",
};

#define MT_TOOL_MAX 1
static const char *input_mt_tool_events[MT_TOOL_MAX + 1] = {
	[MT_TOOL_FINGER] = "MT_TOOL_FINGER",
	[MT_TOOL_PEN] = "MT_TOOL_PEN",
};

static const char *input_ff_events[FF_MAX + 1] = {
	[FF_STATUS_STOPPED] = "FF_STATUS_STOPPED",
	[FF_STATUS_PLAYING] = "FF_STATUS_PLAYING",
	[FF_STATUS_MAX] = "FF_STATUS_MAX",
	[FF_RUMBLE] = "FF_RUMBLE",
	[FF_PERIODIC] = "FF_PERIODIC",
	[FF_CONSTANT] = "FF_CONSTANT",
	[FF_SPRING] = "FF_SPRING",
	[FF_FRICTION] = "FF_FRICTION",
	[FF_DAMPER] = "FF_DAMPER",
	[FF_INERTIA] = "FF_INERTIA",
	[FF_RAMP] = "FF_RAMP",
	[FF_EFFECT_MIN] = "FF_EFFECT_MIN",
	[FF_EFFECT_MAX] = "FF_EFFECT_MAX",
	[FF_SQUARE] = "FF_SQUARE",
	[FF_TRIANGLE] = "FF_TRIANGLE",
	[FF_SINE] = "FF_SINE",
	[FF_SAW_UP] = "FF_SAW_UP",
	[FF_SAW_DOWN] = "FF_SAW_DOWN",
	[FF_CUSTOM] = "FF_CUSTOM",
	[FF_WAVEFORM_MIN] = "FF_WAVEFORM_MIN",
	[FF_WAVEFORM_MAX] = "FF_WAVEFORM_MAX",
	[FF_GAIN] = "FF_GAIN",
	[FF_AUTOCENTER] = "FF_AUTOCENTER",
};

static struct {
	int max;
	const char **events;
} ev_table[EV_MAX] = {
	[EV_KEY] = { KEY_MAX, input_key_events },
	[EV_SYN] = { SYN_MAX, input_syn_events },
	[EV_REL] = { REL_MAX, input_rel_events },
	[EV_ABS] = { ABS_MAX, input_abs_events },
	[EV_MSC] = { MSC_MAX, input_msc_events },
	[EV_SW] = { SW_MAX, input_sw_events },
	[EV_LED] = { LED_MAX, input_led_events },
	[EV_SND] = { SND_MAX, input_snd_events },
	[EV_REP] = { REP_MAX, input_rep_events },
	[EV_FF] = { FF_MAX, input_ff_events },
};

#define array_entries(x) (sizeof(x)/sizeof(char *))

struct input_translate {
	struct hsearch_data hash;
};

#define add_to_hash(x, y) \
	do {\
		unsigned long i; \
		ENTRY e, *ep; \
		for (i = 0; i < array_entries((x)); i++) { \
			if ((x)[i] == NULL) \
				continue; \
			e.key = (char *)(x)[i]; \
			e.data = (void *)i; \
			if (!hsearch_r(e, ENTER, &ep, (y))) { \
				fprintf(stderr, "Error adding to the hash\n"); \
				return 1; \
			} \
		} \
	} while(0)
static int create_hash(struct input_translate *priv)
{
	add_to_hash(input_events, &priv->hash);
	add_to_hash(input_key_events, &priv->hash);
	add_to_hash(input_syn_events, &priv->hash);
	add_to_hash(input_rel_events, &priv->hash);
	add_to_hash(input_abs_events, &priv->hash);
	add_to_hash(input_sw_events, &priv->hash);
	add_to_hash(input_msc_events, &priv->hash);
	add_to_hash(input_led_events, &priv->hash);
	add_to_hash(input_rep_events, &priv->hash);
	add_to_hash(input_snd_events, &priv->hash);
	add_to_hash(input_bus_events, &priv->hash);
	add_to_hash(input_mt_tool_events, &priv->hash);
	add_to_hash(input_ff_events, &priv->hash);

	return 0;
}

int input_translate_string(struct input_translate *priv, char *value, struct input_translate_type *type)
{
	char t[20], *ptr;
	ENTRY *ep, e;

	ptr = strstr(value, "_");
	/* make sure EV_<x> will fit in t[] */
	if (ptr == NULL || (ptr - value) > (sizeof(t) - 4)) {
		errno = EINVAL;
		return 1;
	}

	ptr = strndup(value, ptr - value);
	if (ptr == NULL) {
		errno = ENOMEM;
		return 1;
	}

	snprintf(t, sizeof(t), "EV_%s", ptr);
	free(ptr);

	e.key = t;
	if (!hsearch_r(e, FIND, &ep, &priv->hash))
		return 1;
	type->type = (unsigned long)ep->data;

	e.key = value;
	if (!hsearch_r(e, FIND, &ep, &priv->hash))
		return 1;
	type->code = (unsigned long)ep->data;

	return 0;
}

const char *input_translate_code(uint16_t type, uint16_t code)
{
	if (type >= EV_MAX || ev_table[type].max == 0)
		return NULL;
	if (code >= ev_table[type].max)
		return NULL;

	return ev_table[type].events[code];
}

struct input_translate *input_translate_init(void)
{
	struct input_translate *priv = malloc(sizeof(*priv));
	if (priv) {
		if (!hcreate_r(HASH_SIZE, &(priv->hash))) {
			free(priv);
			priv = NULL;
		} else
			create_hash(priv);
	}
	return priv;
}

