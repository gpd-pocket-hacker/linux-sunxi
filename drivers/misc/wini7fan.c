#include <linux/module.h>
#include <linux/platform_device.h>


#include <linux/irq.h>
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include<asm/io.h>


#define GPIOFANADR1      0xfed8c400
#define GPIOFANADR2      0xfed8c408
struct gpio_fan_data {
	struct platform_device	*pdev;
	struct device		*hwmon_dev;
	
};


static struct acpi_device_id wini7fan_acpi_match[] = {
	{"FAN02501",0},
	{ },
};

static unsigned int *gpd0con;  
static unsigned int *gpd1con;  

static int wini7fan_probe(struct platform_device *pdev)
{	
	/*
	struct gpio_desc *gpiod;
	struct device *dev = &pdev->dev;
		if (!ACPI_HANDLE(dev))
		return -ENODEV;
		const struct acpi_device_id *acpi_id;
	acpi_id = acpi_match_device(dev->driver->acpi_match_table, dev);
	if (!acpi_id) {
		dev_err(dev, "failed to get ACPI info\n");
		return -ENODEV;
	}
		gpiod = devm_gpiod_get_index(dev, "fan_1", 0);
	if (IS_ERR(gpiod)) {
		int err = PTR_ERR(gpiod);
		printk("get fan_1 failed: %d\n", err);
		return err;
	}
	gpiod_direction_input(gpiod);
	gpiod_direction_output(gpiod, 1);
	gpiod_direction_input(gpiod);
	*/
	printk(KERN_ERR "wini7fan start\n");
	gpd0con = ioremap(GPIOFANADR1,4); 
	gpd1con = ioremap(GPIOFANADR2,4);
	writel(readl(gpd0con)|2,gpd0con);
	writel(readl(gpd1con)|2,gpd1con);
	return 0;
	
}



static int wini7fan_remove(struct platform_device *pdev)
{
	return 0;
}


#ifdef CONFIG_PM_SLEEP

static int wini7fan_suspend(struct device *dev)
{
printk(KERN_ERR "wini7fan suspend\n");
if (gpd0con)
	writel(readl(gpd0con)&~0x2UL,gpd0con);
if (gpd1con)
	writel(readl(gpd0con)&~0x2UL,gpd1con);

return 0;
}

static int wini7fan_resume(struct device *dev)
{
printk(KERN_ERR "wini7fan resume\n");
if (gpd0con)
	writel(readl(gpd0con)|2,gpd0con);
if (gpd1con)
	writel(readl(gpd0con)|2,gpd1con);

return 0;
}

#endif

static SIMPLE_DEV_PM_OPS(wini7fan_pm_ops, wini7fan_suspend,wini7fan_resume);


MODULE_DEVICE_TABLE(acpi, wini7fan_acpi_match);
static struct platform_driver wini7fan_driver = {
	.probe		= wini7fan_probe,
	.remove		= wini7fan_remove,
	.driver	= {
		.name	= "FAN0",
		.acpi_match_table = ACPI_PTR(wini7fan_acpi_match),
                .pm     = &wini7fan_pm_ops,
	},
};

module_platform_driver(wini7fan_driver);

MODULE_AUTHOR("yangweili@wisky.com.cn");
MODULE_DESCRIPTION("wini7fan_driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:wini7fan_driver");
