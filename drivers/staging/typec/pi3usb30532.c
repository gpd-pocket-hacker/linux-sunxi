/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pm.h>
#include <linux/mod_devicetable.h>
#include "tcpm.h"


#define PI3USB30532_OPEN 0
#define PI3USB30532_OPEN_SWAP 1
#define PI3USB30532_4LANE_DP  2
#define PI3USB30532_4LANE_DP_SWAP  3
#define PI3USB30532_USB3   4
#define PI3USB30532_USB3_SWAP   5
#define PI3USB30532_USB3_2LANE_DP   6
#define PI3USB30532_USB3_2LANE_DP_SWAP   7


#define MAX_MUXES 4


struct pi3usb30532_chip
{
  int present;
  struct i2c_client *client;
  uint8_t config;
  struct tcpc_mux_dev mux;
#if 0
  struct work_struct work;
#endif
};




static struct pi3usb30532_chip muxes[MAX_MUXES];
static int next_mux = 0;



int
get (struct pi3usb30532_chip *chip, uint8_t * v)
{
  s32 ret;

  ret = i2c_smbus_read_word_swapped (chip->client, 0x00);

  if (ret < 0)
    {
      printk (KERN_ERR "pi3usb30532: failed to read config\n");
      return -1;
    }
  else
    {
      printk (KERN_ERR "pi3usb30532: read config %x\n", ret);
    }

  *v = ret & 0xff;

  return 0;
}


int
set (struct pi3usb30532_chip *chip, uint8_t v)
{
  int ret;
  uint8_t w;


  get (chip, &w);

  ret = i2c_smbus_write_byte_data (chip->client, 0x00, v);

  if (ret < 0)
    {
      printk (KERN_ERR "pi3usb30532: failed to write config\n");
      return -1;
    }
  else
    {
      printk (KERN_ERR "pi3usb30532: wrote config %x\n", v);
    }

  get (chip, &w);

  return 0;
}


#if 0
static void
pi3usb30532_worker (struct work_struct *work)
{
  struct pi3usb30532_chip *chip =
    container_of (work, struct pi3usb30532_chip, work);


  set (chip, chip->config);
}
#endif

static int
mux_set (struct tcpc_mux_dev *dev, enum tcpc_mux_mode mode,
         enum tcpc_usb_switch config, enum typec_cc_polarity polarity)
{
  struct pi3usb30532_chip *chip = (struct pi3usb30532_chip *) dev->priv_data;

  if (!chip)
    return -ENODEV;

  if (!chip->present)
    return -ENODEV;

  if ((config == TCPC_USB_SWITCH_DISCONNECT) || (mode == TYPEC_MUX_NONE))
    {
      chip->config = PI3USB30532_OPEN;
    }
  else if (config == TCPC_USB_SWITCH_CONNECT)
    {

      if (mode == TYPEC_MUX_DP)
        {
          chip->config =
            (polarity ==
             TYPEC_POLARITY_CC1) ? PI3USB30532_4LANE_DP :
            PI3USB30532_4LANE_DP_SWAP;
        }
      else if (mode == TYPEC_MUX_USB)
        {
          chip->config =
            (polarity ==
             TYPEC_POLARITY_CC1) ? PI3USB30532_USB3 : PI3USB30532_USB3_SWAP;
        }
      else if (mode == TYPEC_MUX_DOCK)
        {
          chip->config =
            (polarity ==
             TYPEC_POLARITY_CC1) ? PI3USB30532_USB3_2LANE_DP :
            PI3USB30532_USB3_2LANE_DP_SWAP;
        }
      else
        {
          printk (KERN_ERR "pi3usb30532_set: unknown mode %d\n", mode);
          return -EINVAL;
        }
    }
  else
    {
      printk (KERN_ERR "pi3usb30532_set: unknown config %d\n", config);
      return -EINVAL;
    }

  printk (KERN_ERR "pi3usb30532_set: (%d,%d,%d) mapped to %x\n", mode, config,
          polarity, chip->config);

  set (chip, chip->config);

  return 0;
}

struct tcpc_mux_dev *
pi3usb30532_mux (int instance_no)
{
  struct pi3usb30532_chip *chip;
  struct tcpc_mux_dev *mux;

  if (instance_no >= MAX_MUXES)
    return NULL;

  chip = &muxes[instance_no];

  mux = &chip->mux;

  mux->set = mux_set;
  mux->dfp_only = 0;
  mux->priv_data = chip;

  return mux;
}




static int
pi3usb30532_probe (struct i2c_client *client, const struct i2c_device_id *id)
{
  struct i2c_adapter *adapter = to_i2c_adapter (client->dev.parent);
  struct pi3usb30532_chip *chip;

  printk(KERN_ERR "pi3usb30532_probe\n");
  if (next_mux == MAX_MUXES)
    return -ENODEV;

  chip = &muxes[next_mux];

  if (!i2c_check_functionality (adapter, I2C_FUNC_SMBUS_WORD_DATA))
    return -EIO;

#if 0
  chip = devm_kzalloc (&client->dev, sizeof (*chip), GFP_KERNEL);
  if (!chip)
    return -ENOMEM;
#endif

  chip->client = client;
  i2c_set_clientdata (client, chip);

#if 0
  INIT_WORK (&chip->work, pi3usb30532_worker);
#endif

  chip->config = PI3USB30532_OPEN;

  get (chip, &chip->config);

  //schedule_work(&chip->work);
  //
  
  chip->present = 1;


  printk(KERN_ERR "pi3usb30532 instance #%d initted\n",next_mux);


  next_mux++;

  return 0;
}



#ifdef CONFIG_PM_SLEEP
static int
pi3usb30532_suspend (struct device *dev)
{
  struct pi3usb30532_chip *chip = dev_get_drvdata (dev);

  set (chip, PI3USB30532_OPEN);

  return 0;
}

static int
pi3usb30532_resume (struct device *dev)
{
  struct pi3usb30532_chip *chip = dev_get_drvdata (dev);
//
// schedule_work(&chip->work);

  /* pi3usb30532 doesn't use addresses, but byte zero is read only */
  set (chip, chip->config);
  return 0;
}
#endif

static SIMPLE_DEV_PM_OPS (pi3usb30532_pm_ops, pi3usb30532_suspend,
                          pi3usb30532_resume);

static const struct i2c_device_id pi3usb30532_id[] = {
  {"pi3usb30532"},
  {}
};

MODULE_DEVICE_TABLE (i2c, pi3usb30532_id);

static struct i2c_driver pi3usb30532_i2c_driver = {
  .driver = {
             .name = "pi3usb30532",
             .pm = &pi3usb30532_pm_ops,
             },
  .probe = pi3usb30532_probe,
  .id_table = pi3usb30532_id,
};

module_i2c_driver (pi3usb30532_i2c_driver);

MODULE_AUTHOR ("James McKenzie <kernel@madingley.org>");
MODULE_DESCRIPTION ("pi3usb30532 usb mux driver");
MODULE_LICENSE ("GPL");
