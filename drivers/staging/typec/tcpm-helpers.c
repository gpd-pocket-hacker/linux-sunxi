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

static int tcpm_psy_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val)
{
	struct tcpc_dev *tcpc = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = tcpc->supply_voltage * 1000; /* mV -> µV */
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = tcpc->current_limit * 1000; /* mA -> µA */
		break;
	default:
		return -ENODATA;
	}

	return 0;
}

static enum power_supply_property tcpm_psy_properties[] = {
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
};

int tcpm_register_psy(struct device *dev, struct tcpc_dev *tcpc,
		      const char *name)
{
	struct power_supply_config psy_cfg = {};
	struct power_supply_desc *desc;
	int ret = 0;

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	desc->name		= name;
	desc->type		= POWER_SUPPLY_TYPE_USB;
	desc->properties	= tcpm_psy_properties;
	desc->num_properties	= ARRAY_SIZE(tcpm_psy_properties);
	desc->get_property	= tcpm_psy_get_property;
	psy_cfg.drv_data	= tcpc;

	tcpc->psy = devm_power_supply_register(dev, desc, &psy_cfg);
	if (IS_ERR(tcpc->psy)) {
		ret = PTR_ERR(tcpc->psy);
		tcpc->psy = NULL;
		dev_err(dev, "Error registering power-supply: %d\n", ret);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(tcpm_register_psy);

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

int tcpm_set_current_limit_psy(struct tcpc_dev *tcpc, u32 max_ma, u32 mv)
{
	tcpc->supply_voltage = mv;
	tcpc->current_limit = max_ma;

	if (tcpc->psy)
		power_supply_changed(tcpc->psy);

	return 0;
}
EXPORT_SYMBOL_GPL(tcpm_set_current_limit_psy);
