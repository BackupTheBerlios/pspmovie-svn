#define  DBUS_API_SUBJECT_TO_CHANGE

#include <hal/libhal.h>
#include <hal/libhal-storage.h>

#include <stdio.h>
#include <string.h>

#include "pspdetect.h"

char *find_psp_mount(char *error_buff, int bufsize)
{
	DBusError err;
	dbus_error_init(&err);

	DBusConnection *dc = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
	if ( !dc ) {
		strncpy(error_buff, "No dbus connection", bufsize);
		return 0;
	}
	
	LibHalContext *hc = libhal_ctx_new();
	if ( !libhal_ctx_set_dbus_connection( hc, dc )) {
		strncpy(error_buff, "libhal_ctx_set_dbus_connection failed", bufsize);
		return 0;
	}
	
	if ( !libhal_ctx_init(hc, &err) ) {
		strncpy(error_buff, "libhal_ctx_init failed", bufsize);
		return 0;
	}
	
	for(char dev = 'a'; dev < 'z'; dev++) {	
		char device_file[16];
		sprintf(device_file, "/dev/sd%c", dev);
		LibHalDrive *haldrive = libhal_drive_from_device_file(hc, device_file);
		if ( !haldrive ) {
			continue;
		}
		const char *vendor = libhal_drive_get_vendor(haldrive);
		const char *model = libhal_drive_get_model(haldrive);

		printf("%s -> Vendor [%s] Model [%s]\n", device_file, vendor, model);
		bool psp_detected = !strcmp(vendor, "Sony") && !strcmp(model, "PSP");

		libhal_drive_free(haldrive);
		
		sprintf(device_file, "/dev/sd%c1", dev);
		LibHalVolume *halvolume = libhal_volume_from_device_file (hc, device_file);
		if ( !halvolume ) {
			continue;
		}

		if ( libhal_volume_is_mounted(halvolume) ) {
			const char *mount_point = libhal_volume_get_mount_point(halvolume);
			printf("\tmounted on [%s]\n", mount_point);
			if ( psp_detected ) {
				char *mount_point_dup = strdup(mount_point);
				libhal_volume_free(halvolume);
				return mount_point_dup;
			}
		}
		libhal_volume_free(halvolume);
	}
	return 0;
}
