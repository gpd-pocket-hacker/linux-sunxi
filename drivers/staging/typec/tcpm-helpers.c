/*
 * Copyright 2017 Hans de Goede <hdegoede@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb/typec.h>

#include "tcpm.h"

/* Generic (helper) implementations for some tcpc_dev callbacks */
int tcpm_get_usb2_current_limit_extcon(struct tcpc_dev *tcpc)
{
	struct extcon_dev *extcon = tcpc->usb2_extcon;
	int current_limit = 0;
	unsigned long timeout;

	if (!extcon)
		return 0;

	/*
	 * USB2 Charger detection may still be in progress when we get here,
	 * this can take upto 600ms, wait 800ms max.
	 */
	timeout = jiffies + msecs_to_jiffies(800);
	do {
		if (extcon_get_state(extcon, EXTCON_CHG_USB_SDP) == 1) {
			current_limit = 500;
			break;
		}

		if (extcon_get_state(extcon, EXTCON_CHG_USB_CDP) == 1 ||
		    extcon_get_state(extcon, EXTCON_CHG_USB_ACA) == 1) {
			current_limit = 1500;
			break;
		}

		if (extcon_get_state(extcon, EXTCON_CHG_USB_DCP) == 1) {
			current_limit = 2000;
			break;
		}

		msleep(50);
	} while (time_before(jiffies, timeout));

	return current_limit;
}
EXPORT_SYMBOL_GPL(tcpm_get_usb2_current_limit_extcon);
