# Seven Clicker Sample

This is an end-to-end Internet of Things (IoT) sample project which collects
data from a temperature and humidity sensor, and publishes it to AWS IoT Core
MQTT broker using mobile connectivity (LTE).

The project is based on [Zephyr Project](https://zephyrproject.org/) and
hardware from [MIKROE](https://www.mikroe.com/).

## Hardware

Hardware used in this project:

* [J-Link PLUS Compact](https://shop.segger.com/debug-trace-probes/debug-probes/j-link/j-link-plus-compact)
* [MIKROE - CLICKER FOR STM32](https://www.mikroe.com/clicker-2-stm32f4)
* [MIKROE - LTE IOT 7 CLICK](https://www.mikroe.com/lte-iot-7-click)
* [MIKROE - TEMP&HUM CLICK](https://www.mikroe.com/temp-hum-click)

You also need:

* Micro-B USB cable
* SIM card with a data plan
* J-Link PLUS Compact adapter for the
  [CLICKER FOR STM32](https://www.mikroe.com/clicker-2-stm32f4) JTAG connector

## Build firmware

```
west build -p always -b mikroe_clicker_2
```

## Flash firmware

```
west flash -r jlink
```

## Report issues

If you run into problems, you can ask for help in our
[issue tracker on GitHub](https://github.com/id8-engineering/seven-clicker-sample/issues).
