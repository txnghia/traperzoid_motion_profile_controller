DC Motor Speed and Position Controller for Arduino UNO Q
Overview
This project demonstrates a complete DC motor control system using the Arduino UNO Q, a DRV8871 H-Bridge motor driver, and a quadrature encoder.

The project includes:

Open-loop PWM motor control
Quadrature encoder monitoring
Motor RPM and gearbox output RPM calculation
Closed-loop speed PID control
Direct position PID control
Trapezoidal motion profile with speed PID
Live browser-based trend graphs
MPU-to-MCU communication using Arduino RouterBridge RPC
Web UI using Arduino App Lab WebUI and Socket.IO
The project is useful for learning motor control, encoder feedback, PID tuning, and motion profiling.

System Architecture
Browser Web UI
     ↓
Socket.IO
     ↓
Python App on MPU
     ↓
RouterBridge RPC
     ↓
STM32U585 MCU
     ↓
PWM Signals
     ↓
DRV8871 H-Bridge
     ↓
DC Motor + Gearbox
     ↓
Quadrature Encoder Feedback
     ↓
MCU Control Loop
Hardware
Controller
Arduino UNO Q
Motor Driver
DRV8871 H-Bridge motor driver
Motor
Brushed DC motor with quadrature encoder
Encoder CPR: 44
Gearbox ratio: 162.75:1
Encoder Output CPR
Output CPR = Motor Encoder CPR × Gearbox Ratio

Output CPR = 44 × 162.75 = 7161 counts/output revolution
Wiring
DRV8871
UNO Q Pin	DRV8871 Pin
D9	IN1
D6	IN2
GND	GND
Encoder
Encoder Signal	UNO Q Pin
Channel A	D2
Channel B	D3
GND	GND
VCC	Encoder supply voltage
Make sure the encoder output voltage is compatible with the UNO Q MCU input pins.

Project Structure
dc_motor_controller/
│
├── index.html
├── app.js
├── style.css
│
├── python/
│   └── main.py
│
├── sketch/
│   └── sketch.ino
│
└── README.md
Control Modes
1. Open-Loop Motor Control
The user directly commands motor PWM.

PWM range: -255 to +255
Meaning:

+255 = full forward
0    = stop
-255 = full reverse
This mode disables all closed-loop controllers.

2. Encoder Live Monitor
The MCU decodes the quadrature encoder using interrupts.

Displayed in the browser:

Encoder count
Direction
Raw speed in counts/second
Filtered speed in counts/second
Motor RPM
Output shaft RPM
Output shaft angle
3. Closed-Loop Speed PID
This mode controls output shaft speed.

The user commands:

Target output RPM
The MCU compares target speed to measured speed and adjusts PWM using PID.

Target RPM
    ↓
Speed PID
    ↓
PWM
    ↓
DRV8871
    ↓
Motor
    ↓
Encoder speed feedback
Tunable gains:

Kp
Ki
Kd
Recommended starting values:

Kp = 0.05
Ki = 0.10
Kd = 0.00
4. Direct Position PID
This mode controls position directly.

The user commands:

Target output angle in degrees
The MCU converts target angle to encoder counts:

target_counts = target_degrees / 360 × 7161
Then the position PID directly generates PWM.

Target Angle
    ↓
Position Error
    ↓
Position PID
    ↓
PWM
    ↓
DRV8871
    ↓
Motor
    ↓
Encoder position feedback
Tunable gains:

Kp
Ki
Kd
Recommended starting values:

Kp = 0.20
Ki = 0.00
Kd = 0.01
This mode is useful for simple position experiments, but it may move aggressively for large position changes.

5. Trapezoidal Motion Profile + Speed PID
This is the preferred position-control mode for smoother motion.

Instead of driving PWM directly from position error, this mode generates a smooth target speed profile.

Target Angle
    ↓
Trapezoidal Motion Profile
    ↓
Target RPM
    ↓
Speed PID
    ↓
PWM
    ↓
DRV8871
    ↓
Motor
The trapezoidal profile has three phases:

Accelerate
Cruise
Decelerate
Speed shape:

RPM

     ________
    /        \
   /          \
__/            \__
The user controls:

Target angle
Maximum RPM
Acceleration in RPM/s
Recommended starting values:

Max RPM = 20
Acceleration = 30 RPM/s
This mode gives smoother starts and stops and is better for robotics, puppets, steering mechanisms, and mechanical systems with gears.

Live Trend Graphs
The web interface includes live trend graphs for:

Speed PID
Target RPM
Actual output RPM
Speed PID PWM
Direct Position PID
Target angle
Actual angle
Position PID PWM
Trapezoidal Profile
Target angle
Actual angle
Generated target RPM
These graphs make PID tuning and motion-profile tuning much easier.

MCU RPC Services
Motor
motor/set
motor/stop
motor/get
Encoder
encoder/count
encoder/dir
encoder/raw_speed
encoder/filtered_speed
encoder/motor_rpm
encoder/output_rpm
encoder/reset
Speed PID
pid/enable
pid/disable
pid/set_target_output_rpm
pid/get_target_output_rpm
pid/get_enabled
pid/get_output_pwm
pid/set_kp
pid/set_ki
pid/set_kd
Direct Position PID
position/enable
position/disable
position/set_target_deg
position/get_target_deg
position/get_target_counts
position/get_angle_x10
position/get_enabled
position/get_output_pwm
position/set_kp
position/set_ki
position/set_kd
Trapezoidal Profile
profile/enable
profile/disable
profile/set_target_deg
profile/get_target_deg
profile/get_target_counts
profile/get_enabled
profile/get_target_rpm
profile/get_velocity_rpm
profile/get_error_counts
profile/set_max_rpm
profile/set_accel
profile/get_max_rpm
profile/get_accel
Web UI Sections
The browser interface includes:

Open-loop motor control
Encoder live data
Closed-loop speed control
Live speed trend graph
Direct position PID
Live position trend graph
Trapezoidal profile + speed PID
Live profile trend graph
Status display
Important UNO Q Note
During development, the motor driver input pins worked best when controlled consistently with:

analogWrite()
For logic-low and logic-high states:

analogWrite(pin, 0);
analogWrite(pin, 255);
This avoided issues caused by mixing:

digitalWrite()
and:

analogWrite()
on the same PWM-capable pins.

Running the Project
1. Upload MCU Sketch
Upload:

sketch/sketch.ino
to the UNO Q MCU.

The serial monitor should show:

DC Motor Controller with Trapezoid Profile ready.
2. Start the MPU App
Run the Arduino App Lab project.

The Python application uses:

from arduino.app_utils import *
from arduino.app_bricks.web_ui import WebUI
3. Open the Browser
Open the UNO Q web interface from your computer browser.

Example:

http://<uno-q-ip-address>
Suggested Testing Order
Step 1: Encoder Test
Turn the motor shaft slowly by hand.

Verify:

Encoder count changes
Direction changes
Output angle changes
Step 2: Open Loop Test
Use PWM slider:

+80
+120
-80
-120
0
Verify motor direction and encoder feedback.

Step 3: Speed PID Test
Start with:

Target RPM = 5
Kp = 0.05
Ki = 0.10
Kd = 0.00
Then try:

10 RPM
20 RPM
Watch the speed trend graph.

Step 4: Direct Position PID Test
Start with small moves:

10°
30°
45°
Use:

Kp = 0.20
Ki = 0.00
Kd = 0.01
Step 5: Trapezoidal Profile Test
Start with:

Target = 90°
Max RPM = 10
Acceleration = 20 RPM/s
Then test:

180°
360°
-90°
Watch the generated target RPM and actual position trend.

Tuning Tips
Speed PID
If motor is slow to respond:

Increase Kp
If motor does not reach target RPM:

Increase Ki
If motor oscillates:

Reduce Kp or Ki
Usually start with:

Kd = 0
Direct Position PID
If position moves too slowly:

Increase Kp
If it overshoots:

Reduce Kp
If it oscillates near target:

Increase Kd slightly
Keep:

Ki = 0
at first.

Trapezoidal Profile
If motion is too slow:

Increase Max RPM
If acceleration is too gentle:

Increase Acceleration
If motion jerks or overshoots:

Lower Max RPM
Lower Acceleration
Tune Speed PID first
Future Improvements
Possible future enhancements:

STM32 hardware timer encoder mode
S-curve motion profile
Position limits
Soft end stops
Homing switch
Emergency stop input
Multiple motor support
Data logging
CSV export
Auto PID tuning
Command scripting
ROS2 integration
Educational Topics Covered
This project demonstrates:

H-Bridge motor driving
Quadrature encoder feedback
Interrupt-based encoder decoding
RPM measurement
Digital low-pass filtering
PID speed control
PID position control
Cascaded position-speed control
Trapezoidal motion profiling
Browser-based embedded control
Arduino UNO Q MPU/MCU cooperation
RouterBridge RPC communication
License
Educational motor-control demonstration project for Arduino UNO Q, DRV8871, and quadrature encoder based DC motors.
