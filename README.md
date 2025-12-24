# GliderThrow_Meter

This project proposes a simple digital Angle/Throw meter and ... Differentials, using two esp32 and two MPU 6050 6 DOFFS components.

GliderThrow meter is composed by two device, each using one esp32 and one MPU 6050.

Each device can measure the deflections in degrees / millimeters with a resolution of 0.1 degrees and can measure the differential when working together with a second unit since GliderThrow is a system that comprises two sensors, one for each wing or control surface of your airplane.

Using a dual system simplifies a lot the throw setting of your model by having a direct view of both control surfaces at the same time.

GliderThrow is primary design for setting the control surface of a RC glider but you will find that it can be used on most every RC airplane and for a variety of applications as Measuring a dihedral angle of a wing, Measuring Model Airplane Incidence angle , etc.

As the first device embedded a small http server, the data can be viewed through any web browser on a smartphone, PC, or MAC, using Windows, Linux, Android or iOS.

UI is built using bootstrap and jquery, and all the files needed are embedded in the .rodata segment of the first device.

The project is composed of two parts, the server (Esp_mad_Server directory) and the client (Esp_mad_Client directory).

Two extras libraries are used in the project : i2clibdev and MPU6050 from jrowberg (https://github.com/jrowberg/i2cdevlib).

These libraries are in the extra_components directory of the project.

For more information, please read the docs at https://gliderthrow-meter.readthedocs.io/en/latest/

## Monitoring CPU usage

The ESP32-C3 server firmware now exposes real-time FreeRTOS runtime statistics. Once the server has booted and you are connected to its access point, issue an HTTP GET request to `http://192.168.1.1/runtime_stats` (adjust the IP address if you changed the AP settings). The endpoint returns JSON with one entry per task, including its accumulated runtime ticks, stack high-water mark, and the percentage of CPU time consumed since boot. This makes it easy to verify how much CPU is used by `measure_task`, `http_server_task`, `vBattery_task`, or any other application task without attaching a debugger.

Enjoy !
