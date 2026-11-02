/*
 * tico68.r - the application's resources
 */

#include "Dialogs.r"
#include "Processes.r"
#include "Finder.r"

resource 'SIZE' (-1) {
	reserved,
	ignoreSuspendResumeEvents,
	reserved,
	cannotBackground,
	needsActivateOnFGSwitch,
	backgroundAndForeground,
	dontGetFrontClicks,
	ignoreChildDiedEvents,
	is32BitCompatible,
	notHighLevelEventAware,
	onlyLocalHLEvents,
	notStationeryAware,
	dontUseTextEditServices,
	notDisplayManagerAware,
	reserved,
	reserved,
	2048 * 1024,
	512 * 1024
};

data 'Tc68' (0, "Owner resource") {
	$"0674 6963 6F36 38"
};

resource 'vers' (1) {
	0x00, 0x01, development, 0x00, verUS, "0.01", "0.01, tico68"
};

resource 'FREF' (128) { 'APPL', 0, "" };
resource 'FREF' (129) { 'TEXT', 1, "" };

resource 'BNDL' (128) {
	'Tc68', 0,
	{
		'ICN#', { 0, 128; 1, 129 },
		'FREF', { 0, 128; 1, 129 }
	}
};

resource 'ICN#' (128) {
	{
		$"0000000007FFFFE0040000200400002004000020043FF820043FF82004038020"
		$"0403802004038020040380200403802004038020040380200403802004038020"
		$"04038020040000200400002004000020047FFE200400002004000020047FFE20"
		$"0400002004000020047FFE2004000020040000200400002007FFFFE000000000",
		$"0000000007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE0"
		$"07FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE0"
		$"07FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE0"
		$"07FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE000000000"
	}
};

resource 'ICN#' (129) {
	{
		$"0000000007FFFFE0040000200400002004000020040000200400002004000020"
		$"047FFE200400002004000020047FFE200400002004000020047FFE2004000020"
		$"04000020040000200400002004000020047FFE200400002004000020047FFE20"
		$"0400002004000020047FFE2004000020040000200400002007FFFFE000000000",
		$"0000000007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE0"
		$"07FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE0"
		$"07FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE0"
		$"07FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE007FFFFE000000000"
	}
};

/* Save changes? */
resource 'ALRT' (128) {
	{ 80, 90, 210, 422 }, 128,
	{ OK, visible, silent; OK, visible, silent;
	  OK, visible, silent; OK, visible, silent },
	noAutoCenter
};

resource 'DITL' (128) {
	{
		{ 96, 244, 116, 318 }, Button { enabled, "Save" };
		{ 96, 160, 116, 234 }, Button { enabled, "Cancel" };
		{ 96, 14, 116, 112 }, Button { enabled, "Don't Save" };
		{ 12, 72, 80, 318 }, StaticText { disabled,
			"Save changes to \0xD2^0\0xD3 before closing?" }
	}
};

/* A file error. */
resource 'ALRT' (129) {
	{ 80, 90, 200, 422 }, 129,
	{ OK, visible, sound1; OK, visible, sound1;
	  OK, visible, sound1; OK, visible, sound1 },
	noAutoCenter
};

resource 'DITL' (129) {
	{
		{ 88, 244, 108, 318 }, Button { enabled, "OK" };
		{ 12, 72, 78, 318 }, StaticText { disabled,
			"^0\0x0D\0x0D^1 ^2" }
	}
};

/* About. */
resource 'ALRT' (130) {
	{ 70, 110, 220, 402 }, 130,
	{ OK, visible, silent; OK, visible, silent;
	  OK, visible, silent; OK, visible, silent },
	noAutoCenter
};

resource 'DITL' (130) {
	{
		{ 116, 206, 136, 280 }, Button { enabled, "OK" };
		{ 12, 72, 106, 280 }, StaticText { disabled,
			"tico68 ^0\0x0D\0x0DA tico/nano-style text editor for the classic Macintosh.\0x0D\0x0DMIT License." };
		{ 12, 20, 44, 52 }, Icon { disabled, 128 }
	}
};

/* A file that isn't TEXT. */
resource 'ALRT' (131) {
	{ 80, 90, 210, 422 }, 131,
	{ OK, visible, silent; OK, visible, silent;
	  OK, visible, silent; OK, visible, silent },
	noAutoCenter
};

resource 'DITL' (131) {
	{
		{ 96, 244, 116, 318 }, Button { enabled, "Open" };
		{ 96, 160, 116, 234 }, Button { enabled, "Cancel" };
		{ 12, 72, 84, 318 }, StaticText { disabled,
			"\0xD2^0\0xD3 is of type \0xD2^1\0xD3, not text. Open its data fork anyway?" }
	}
};

/* A save stopped part way: where the text is. */
resource 'ALRT' (132) {
	{ 80, 90, 200, 422 }, 132,
	{ OK, visible, sound1; OK, visible, sound1;
	  OK, visible, sound1; OK, visible, sound1 },
	noAutoCenter
};

resource 'DITL' (132) {
	{
		{ 88, 244, 108, 318 }, Button { enabled, "OK" };
		{ 12, 72, 78, 318 }, StaticText { disabled,
			"The file on disk may be damaged. A complete copy of the text is in \0xD2^0\0xD3, in the same folder." }
	}
};

/* A replacing save left files behind: what and where they are. */
resource 'ALRT' (133) {
	{ 70, 90, 210, 422 }, 133,
	{ OK, visible, sound1; OK, visible, sound1;
	  OK, visible, sound1; OK, visible, sound1 },
	noAutoCenter
};

resource 'DITL' (133) {
	{
		{ 108, 244, 128, 318 }, Button { enabled, "OK" };
		{ 12, 72, 98, 318 }, StaticText { disabled, "^0" }
	}
};

/* Preferences: prefsfile.c counts on the order of these. */
resource 'DLOG' (128) {
	{ 50, 106, 272, 406 },
	dBoxProc, invisible, noGoAway, 0, 200, "", noAutoCenter
};

resource 'DITL' (200) {
	{
		{ 188, 206, 208, 280 }, Button { enabled, "OK" };
		{ 188, 118, 208, 192 }, Button { enabled, "Cancel" };
		{ 182, 200, 214, 286 }, UserItem { disabled };

		{ 10, 16, 26, 200 }, StaticText { disabled, "Keyboard shortcuts:" };
		{ 28, 28, 46, 200 }, RadioButton { enabled, "tico legacy" };
		{ 46, 28, 64, 200 }, RadioButton { enabled, "tico modern" };
		{ 64, 28, 82, 200 }, RadioButton { enabled, "Macintosh" };

		{ 90, 16, 106, 140 }, StaticText { disabled, "Title bar:" };
		{ 108, 28, 126, 132 }, RadioButton { enabled, "Always show" };
		{ 126, 28, 144, 132 }, RadioButton { enabled, "Always hide" };

		{ 90, 160, 106, 290 }, StaticText { disabled, "Shortcut bar:" };
		{ 108, 172, 126, 276 }, RadioButton { enabled, "Always show" };
		{ 126, 172, 144, 276 }, RadioButton { enabled, "Always hide" };

		{ 152, 16, 178, 290 }, StaticText { disabled,
			"The document font is in the Font menu." }
	}
};
