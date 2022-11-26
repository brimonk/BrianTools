// Brian Chrzanowski
//
// I like my main source files being .c too much

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Shobjidl.h>

#include <string.h>

GUID GuidZero = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

GUID GetCOMInterfaceGUIDFromName(char *s)
{
	if (s == NULL)
		return GuidZero;

	if (strcmp(s, "DesktopWallpaper") == 0) {
		return __uuidof(DesktopWallpaper);
	} else if (strcmp(s, "IDesktopWallpaper") == 0) {
		return __uuidof(IDesktopWallpaper);
	}

	return GuidZero;
}

int IsGUIDZero(GUID checkme)
{
	return memcmp((const void *)&checkme, (const void *)&GuidZero, sizeof(GUID)) == 0;
}

#ifdef __cplusplus
}
#endif // __cplusplus
