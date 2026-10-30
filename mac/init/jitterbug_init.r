/*
 * The Jitterbug extension file: the INIT code resource plus Finder
 * resources. JitterbugINIT.flt is the flat 68K code built from
 * jitterbug_init.c (see mac/CMakeLists.txt).
 *
 * sysHeap + locked: the INIT must load into the system heap (so Retro68
 * allocates its globals there too) and must not move once relocated.
 */
#include "Retro68.r"
#include "Finder.r"
#include "JitterbugIcons.r"

#define JB_CREATOR 'JtBg'

type 'INIT' {
	RETRO68_CODE_TYPE
};

resource 'INIT' (128, sysHeap, locked) {
	dontBreakAtEntry, $$read("JitterbugINIT.flt");
};

data JB_CREATOR (0, "Owner resource") {
	$"00"
};

resource 'FREF' (128, "Extension") {
	'INIT', 0, ""
};

resource 'BNDL' (128) {
	JB_CREATOR, 0,
	{
		'ICN#', { 0, 128 },
		'FREF', { 0, 128 }
	}
};

resource 'vers' (1) {
	0x00, 0x10, development, 0x00, 0,
	"0.1d0",
	"0.1d0, Jitterbug system entropy source (experimental)"
};
