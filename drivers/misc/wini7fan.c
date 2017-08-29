#include <linux/module.h>
#include <linux/platform_device.h>

#include <linux/irq.h>
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <asm/io.h>
#include <asm/iosf_mbi.h>
#include "../thermal/intel_soc_dts_iosf.h"

#define GPIOFANADR1      0xfed8c400
#define GPIOFANADR2      0xfed8c408

#define SOC_DTS_TJMAX_ENCODING          0x7F

#define SOC_DTS_OFFSET_TEMP                0xB1

struct wini7fan_dev
{
  struct platform_device *pdev;
  struct device *dev;
  unsigned int *gpd0con;
  unsigned int *gpd1con;
  u32 tj_max;
  struct work_struct work;
  struct hrtimer timer;
  int last_speed;
};


static int
get_tj_max (u32 * tj_max)
{
  u32 eax, edx;
  u32 val;
  int err;


  err = rdmsr_safe (MSR_IA32_TEMPERATURE_TARGET, &eax, &edx);
  if (err)
    goto err_ret;

  else
    {
      val = (eax >> 16) & 0xff;

      if (val)
        {
          *tj_max = val * 1000;
        }
      else
        {
          err = -EINVAL;
          goto err_ret;
        }
    }
  return 0;
err_ret:*tj_max = 0;
  return err;
}

static int
render_temp (struct wini7fan_dev *fan, u32 t)
{
  int ret;

  t &= 0xff;
  t -= SOC_DTS_TJMAX_ENCODING;
  ret = fan->tj_max - t * 1000;

  return ret;
}

static int
get_temp (struct wini7fan_dev *fan, int *_t0, int *_t1)
{
  u32 out;
  int t0;
  int t1;
  int status;

  status =
    iosf_mbi_read (BT_MBI_UNIT_PMC, MBI_REG_READ, SOC_DTS_OFFSET_TEMP, &out);

  if (status)
    return 1000000;             /*Return ludicrious temp */


  t0 = render_temp (fan, out);
  t1 = render_temp (fan, out >> 8);

  if (_t0)
    *_t0 = t0;

  if (_t1)
    *_t1 = t1;

  return (t0 > t1) ? t0 : t1;
}

static struct acpi_device_id wini7fan_acpi_match[] = {
  {"FAN02501", 0}, {},
};

static void
set_fan_speed (struct wini7fan_dev *fan, int speed)
{
  if (speed & 1)
    {
      writel (readl (fan->gpd0con) | 2, fan->gpd0con);
    }
  else
    {
      writel (readl (fan->gpd0con) & ~0x2UL, fan->gpd0con);
    }
  if (speed & 2)
    {
      writel (readl (fan->gpd1con) | 2, fan->gpd1con);
    }
  else
    {
      writel (readl (fan->gpd1con) & ~0x2UL, fan->gpd1con);
    }
}

static void
wini7fan_worker (struct work_struct *work)
{
  struct wini7fan_dev *fan = container_of (work, struct wini7fan_dev, work);
  int t, t0 = 0, t1 = 0;
  int min_speed, max_speed, speed;
  t = get_temp (fan, &t0, &t1);

  if (t < 56000)
    min_speed = 0;
  else if (t < 61000)
    min_speed = 1;
  else if (t < 66000)
    min_speed = 2;
  else
    min_speed = 3;


  if (t < 55000)
    max_speed = 0;
  else if (t < 60000)
    max_speed = 1;
  else if (t < 65000)
    max_speed = 2;
  else
    max_speed = 3;

  speed = fan->last_speed;

  if (speed > max_speed)
    speed = max_speed;
  if (speed < min_speed)
    speed = min_speed;

  if (!fan->last_speed && speed )
    speed = 3;                  /*kick start motor */

  if (speed != fan->last_speed)
  {
    printk (KERN_ERR "wini7fan: t0=%d, t1=%d new speed=%d\n", t0, t1, speed);
  }


  set_fan_speed (fan, speed);

  fan->last_speed = speed;
}

static enum hrtimer_restart
wini7fan_tick (struct hrtimer *timer)
{
  struct wini7fan_dev *fan = container_of (timer, struct wini7fan_dev, timer);

  schedule_work (&fan->work);

  hrtimer_forward_now (&fan->timer, ktime_set (1, 0));

  return HRTIMER_RESTART;
}

static int
wini7fan_probe (struct platform_device *pdev)
{
  struct wini7fan_dev *fan;

  fan = devm_kzalloc (&pdev->dev, sizeof (*fan), GFP_KERNEL);

  if (!fan)
    return -ENOMEM;

  fan->dev = &pdev->dev;
  fan->pdev = pdev;

  platform_set_drvdata (pdev, fan);

  get_tj_max (&fan->tj_max);

  printk (KERN_ERR "wini7fan start tj_max=%d\n", fan->tj_max);

  fan->gpd0con = ioremap (GPIOFANADR1, 4);
  fan->gpd1con = ioremap (GPIOFANADR2, 4);

  set_fan_speed (fan, 3);

  INIT_WORK (&fan->work, wini7fan_worker);

  hrtimer_init (&fan->timer, CLOCK_REALTIME, HRTIMER_MODE_REL);
  fan->timer.function = wini7fan_tick;

  hrtimer_start (&fan->timer, ktime_set (1, 0), HRTIMER_MODE_REL);

  schedule_work (&fan->work);
  return 0;
}

static int
wini7fan_remove (struct platform_device *pdev)
{
  struct wini7fan_dev *fan = platform_get_drvdata (pdev);

  hrtimer_cancel (&fan->timer);
  return 0;
}


#ifdef CONFIG_PM_SLEEP
static int
wini7fan_suspend (struct device *dev)
{
  struct wini7fan_dev *fan = dev_get_drvdata (dev);

  printk (KERN_ERR "wini7fan suspend\n");

  hrtimer_cancel (&fan->timer);
  set_fan_speed (fan, 0);

  return 0;
}

static int
wini7fan_resume (struct device *dev)
{
  struct wini7fan_dev *fan = dev_get_drvdata (dev);

  printk (KERN_ERR "wini7fan resume\n");

  fan->last_speed = 0;

  schedule_work (&fan->work);
  hrtimer_start (&fan->timer, ktime_set (1, 0), HRTIMER_MODE_REL);
  return 0;
}


#endif /*  */
static SIMPLE_DEV_PM_OPS (wini7fan_pm_ops, wini7fan_suspend, wini7fan_resume);

MODULE_DEVICE_TABLE (acpi, wini7fan_acpi_match);

static struct platform_driver wini7fan_driver = {
  .probe = wini7fan_probe,
  .remove = wini7fan_remove,
  .driver = {
             .name = "FAN0",
             .acpi_match_table = ACPI_PTR (wini7fan_acpi_match),
             .pm = &wini7fan_pm_ops,
             },
};

module_platform_driver (wini7fan_driver);
MODULE_AUTHOR ("yangweili@wisky.com.cn");
MODULE_DESCRIPTION ("wini7fan_driver");
MODULE_LICENSE ("GPL");
MODULE_ALIAS ("platform:wini7fan_driver");
