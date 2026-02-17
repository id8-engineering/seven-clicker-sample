# End to end sample using LTE and AWS

## Overview

The sample is a end to end communication sample to test LTE to AWS.
The sample is built for mikroe_clicker_2 but can run on any board that has mikroBUS socket.

### Hardware requirement:

- [Mikroe LTE IOT 7 CLICK](https://www.mikroe.com/lte-iot-7-click)
- [Mikore TEMP&HUM CLICK](https://www.mikroe.com/temp-hum-click)

## Getting started:

Clone repo and cd into it:

`git clone git@github.com:id8-engineering/seven-clicker-sample.git && cd seven-clicker-sample
`

Run:

`west init -l . && west update
`

Build and flash application:

`west build -p always -b mikroe_clicker_2 app && west flash
`
