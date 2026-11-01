/*
 * Finder resources for JitterbugDemo: owner resource, bundle and file
 * reference so the Finder shows its application icon (family 128, from
 * mac/resources/DemoIcons.r).
 *
 * JB_CREATOR must match CREATOR in mac/CMakeLists.txt (see collector.r for
 * why each app has its own).
 */
#include "Finder.r"

#define JB_CREATOR 'JbDm'

/* Owner (signature) resource; the Finder only needs it to exist. */
data JB_CREATOR (0, "Owner resource") {
	$"00"
};

resource 'FREF' (128, "Application") {
	'APPL', 0, ""
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
	"0.1d0, Jitterbug SDK demo"
};
