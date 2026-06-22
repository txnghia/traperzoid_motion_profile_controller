#include <Arduino.h>
#include "Arduino_RouterBridge.h"

static const int IN1_PIN = 9;
static const int IN2_PIN = 6;

static const int ENC_A_PIN = 2;
static const int ENC_B_PIN = 3;

static const float MOTOR_ENCODER_CPR = 44.0f;
static const float GEAR_RATIO = 168.0f;
static const float OUTPUT_CPR = MOTOR_ENCODER_CPR * GEAR_RATIO;

// Motor
volatile int g_pwmCmd = 0;

// Encoder
volatile long g_encoderCount = 0;
volatile int g_encoderDir = 0;

// Speed measurement
volatile long g_rawSpeedCps = 0;
volatile long g_filtSpeedCps = 0;
volatile long g_motorRpm = 0;
volatile long g_outputRpm = 0;
volatile long g_outputAngleDeg_x10 = 0;

long g_lastEncoderCount = 0;
unsigned long g_lastSpeedMicros = 0;

float g_filteredSpeed = 0.0f;
float g_alpha = 0.2f;

// Speed PID
volatile bool g_pidEnabled = false;

float g_targetOutputRpm = 0.0f;
float g_targetCps = 0.0f;

float g_kp = 0.05f;
float g_ki = 0.10f;
float g_kd = 0.0f;

float g_integral = 0.0f;
float g_prevError = 0.0f;

volatile int g_pidOutputPwm = 0;

// Direct position PID
volatile bool g_posEnabled = false;

float g_targetAngleDeg = 0.0f;
long g_targetPositionCounts = 0;

float g_posKp = 0.20f;
float g_posKi = 0.00f;
float g_posKd = 0.01f;

float g_posIntegral = 0.0f;
float g_posPrevError = 0.0f;

volatile int g_posOutputPwm = 0;

// Trapezoid profile + speed PID
volatile bool g_profileEnabled = false;

float g_profileTargetDeg = 0.0f;
long g_profileTargetCounts = 0;

float g_profileMaxRpm = 20.0f;
float g_profileAccelRpmPerSec = 30.0f;

float g_profileVelocityDegPerSec = 0.0f;

volatile long g_profileTargetRpm = 0;
volatile long g_profileVelocityRpm = 0;
volatile long g_profileErrorCounts = 0;

// --------------------------------------------------
// Motor
// --------------------------------------------------

void motorCoast()
{
  analogWrite(IN1_PIN, 0);
  analogWrite(IN2_PIN, 0);
  g_pwmCmd = 0;
}

void setMotor(int cmd)
{
  cmd = constrain(cmd, -255, 255);
  g_pwmCmd = cmd;

  if (cmd == 0) {
    analogWrite(IN1_PIN, 0);
    analogWrite(IN2_PIN, 0);
  }
  else if (cmd > 0) {
    analogWrite(IN1_PIN, cmd);
    analogWrite(IN2_PIN, 0);
  }
  else {
    analogWrite(IN1_PIN, 0);
    analogWrite(IN2_PIN, abs(cmd));
  }
}

// --------------------------------------------------
// Encoder ISR
// --------------------------------------------------

void isrEncA()
{
  bool a = digitalRead(ENC_A_PIN);
  bool b = digitalRead(ENC_B_PIN);

  if (a == b) {
    g_encoderCount++;
    g_encoderDir = 1;
  } else {
    g_encoderCount--;
    g_encoderDir = -1;
  }
}

void isrEncB()
{
  bool a = digitalRead(ENC_A_PIN);
  bool b = digitalRead(ENC_B_PIN);

  if (a != b) {
    g_encoderCount++;
    g_encoderDir = 1;
  } else {
    g_encoderCount--;
    g_encoderDir = -1;
  }
}

// --------------------------------------------------
// Speed PID helper
// --------------------------------------------------

void runSpeedPid(float dt)
{
  float error = g_targetCps - g_filteredSpeed;

  g_integral += error * dt;

  if (g_integral > 2000.0f) g_integral = 2000.0f;
  if (g_integral < -2000.0f) g_integral = -2000.0f;

  float derivative = (error - g_prevError) / dt;
  g_prevError = error;

  float out = g_kp * error + g_ki * g_integral + g_kd * derivative;

  int pwm = constrain((int)out, -255, 255);

  g_pidOutputPwm = pwm;
  setMotor(pwm);
}

// --------------------------------------------------
// Main control loop
// --------------------------------------------------

void updateControl()
{
  unsigned long now = micros();
  unsigned long dt_us = now - g_lastSpeedMicros;

  if (dt_us < 100000) return;

  noInterrupts();
  long count = g_encoderCount;
  interrupts();

  long delta = count - g_lastEncoderCount;
  float dt = dt_us / 1000000.0f;

  float rawSpeed = delta / dt;
  g_filteredSpeed = g_filteredSpeed + g_alpha * (rawSpeed - g_filteredSpeed);

  float motorRpm = (g_filteredSpeed / MOTOR_ENCODER_CPR) * 60.0f;
  float outputRpm = (g_filteredSpeed / OUTPUT_CPR) * 60.0f;
  float outputAngleDeg = ((float)count / OUTPUT_CPR) * 360.0f;

  noInterrupts();
  g_rawSpeedCps = (long)rawSpeed;
  g_filtSpeedCps = (long)g_filteredSpeed;
  g_motorRpm = (long)motorRpm;
  g_outputRpm = (long)outputRpm;
  g_outputAngleDeg_x10 = (long)(outputAngleDeg * 10.0f);
  interrupts();

  g_lastEncoderCount = count;
  g_lastSpeedMicros = now;

  // Speed PID only
  if (g_pidEnabled) {
    runSpeedPid(dt);
  }

  // Direct position PID
  if (g_posEnabled) {
    noInterrupts();
    long currentCount = g_encoderCount;
    interrupts();

    float error = (float)(g_targetPositionCounts - currentCount);

    g_posIntegral += error * dt;

    if (g_posIntegral > 5000.0f) g_posIntegral = 5000.0f;
    if (g_posIntegral < -5000.0f) g_posIntegral = -5000.0f;

    float derivative = (error - g_posPrevError) / dt;
    g_posPrevError = error;

    float out = g_posKp * error + g_posKi * g_posIntegral + g_posKd * derivative;
    int pwm = constrain((int)out, -255, 255);

    if (abs((int)error) < 8) {
      pwm = 0;
      g_posIntegral = 0.0f;
    }

    g_posOutputPwm = pwm;
    setMotor(pwm);
  }

  // Trapezoidal profile + speed PID
  if (g_profileEnabled) {
    noInterrupts();
    long currentCount = g_encoderCount;
    interrupts();

    long errorCounts = g_profileTargetCounts - currentCount;
    g_profileErrorCounts = errorCounts;

    float errorDeg = ((float)errorCounts / OUTPUT_CPR) * 360.0f;

    float maxVelDegPerSec = g_profileMaxRpm * 6.0f;
    float accelDegPerSec2 = g_profileAccelRpmPerSec * 6.0f;

    float stoppingVelDegPerSec = sqrtf(2.0f * accelDegPerSec2 * fabsf(errorDeg));

    float desiredVel = stoppingVelDegPerSec;
    if (desiredVel > maxVelDegPerSec) desiredVel = maxVelDegPerSec;
    if (errorDeg < 0) desiredVel = -desiredVel;

    float maxStep = accelDegPerSec2 * dt;

    if (desiredVel > g_profileVelocityDegPerSec + maxStep) {
      g_profileVelocityDegPerSec += maxStep;
    }
    else if (desiredVel < g_profileVelocityDegPerSec - maxStep) {
      g_profileVelocityDegPerSec -= maxStep;
    }
    else {
      g_profileVelocityDegPerSec = desiredVel;
    }

    if (abs(errorCounts) < 8 && fabsf(g_profileVelocityDegPerSec) < 3.0f) {
      g_profileVelocityDegPerSec = 0.0f;
      g_integral = 0.0f;
      g_prevError = 0.0f;
      setMotor(0);
    }

    float targetRpm = g_profileVelocityDegPerSec / 6.0f;

    g_profileTargetRpm = (long)targetRpm;
    g_profileVelocityRpm = (long)targetRpm;

    g_targetOutputRpm = targetRpm;
    g_targetCps = (g_targetOutputRpm * OUTPUT_CPR) / 60.0f;

    runSpeedPid(dt);
  }
}

// --------------------------------------------------
// RPC motor
// --------------------------------------------------

int rpc_motor_set(int cmd)
{
  g_pidEnabled = false;
  g_posEnabled = false;
  g_profileEnabled = false;

  g_integral = 0.0f;
  g_prevError = 0.0f;
  g_posIntegral = 0.0f;
  g_posPrevError = 0.0f;
  g_profileVelocityDegPerSec = 0.0f;

  setMotor(cmd);
  return g_pwmCmd;
}

int rpc_motor_stop()
{
  g_pidEnabled = false;
  g_posEnabled = false;
  g_profileEnabled = false;

  g_targetOutputRpm = 0.0f;
  g_targetCps = 0.0f;

  g_targetAngleDeg = 0.0f;
  g_targetPositionCounts = 0;

  g_profileTargetDeg = 0.0f;
  g_profileTargetCounts = 0;
  g_profileTargetRpm = 0;
  g_profileVelocityRpm = 0;
  g_profileErrorCounts = 0;
  g_profileVelocityDegPerSec = 0.0f;

  g_integral = 0.0f;
  g_prevError = 0.0f;
  g_posIntegral = 0.0f;
  g_posPrevError = 0.0f;

  g_pidOutputPwm = 0;
  g_posOutputPwm = 0;

  motorCoast();
  return 0;
}

int rpc_motor_get()
{
  return g_pwmCmd;
}

// --------------------------------------------------
// RPC encoder
// --------------------------------------------------

long rpc_encoder_count()
{
  noInterrupts();
  long v = g_encoderCount;
  interrupts();
  return v;
}

int rpc_encoder_dir()
{
  noInterrupts();
  int v = g_encoderDir;
  interrupts();
  return v;
}

long rpc_encoder_raw_speed()
{
  return g_rawSpeedCps;
}

long rpc_encoder_filtered_speed()
{
  return g_filtSpeedCps;
}

long rpc_encoder_motor_rpm()
{
  return g_motorRpm;
}

long rpc_encoder_output_rpm()
{
  return g_outputRpm;
}

int rpc_encoder_reset()
{
  noInterrupts();
  g_encoderCount = 0;
  g_encoderDir = 0;
  g_rawSpeedCps = 0;
  g_filtSpeedCps = 0;
  g_motorRpm = 0;
  g_outputRpm = 0;
  g_outputAngleDeg_x10 = 0;
  interrupts();

  g_lastEncoderCount = 0;
  g_filteredSpeed = 0.0f;
  g_lastSpeedMicros = micros();

  return 0;
}

// --------------------------------------------------
// RPC speed PID
// --------------------------------------------------

int rpc_pid_enable()
{
  g_posEnabled = false;
  g_profileEnabled = false;
  g_pidEnabled = true;

  g_integral = 0.0f;
  g_prevError = 0.0f;

  return 1;
}

int rpc_pid_disable()
{
  g_pidEnabled = false;
  g_integral = 0.0f;
  g_prevError = 0.0f;
  g_pidOutputPwm = 0;
  motorCoast();
  return 0;
}

int rpc_pid_set_target_output_rpm(int rpm)
{
  g_targetOutputRpm = (float)rpm;
  g_targetCps = (g_targetOutputRpm * OUTPUT_CPR) / 60.0f;
  return rpm;
}

int rpc_pid_get_target_output_rpm()
{
  return (int)g_targetOutputRpm;
}

int rpc_pid_get_enabled()
{
  return g_pidEnabled ? 1 : 0;
}

int rpc_pid_get_output_pwm()
{
  return g_pidOutputPwm;
}

int rpc_pid_set_kp(int kp_x1000)
{
  g_kp = kp_x1000 / 1000.0f;
  return kp_x1000;
}

int rpc_pid_set_ki(int ki_x1000)
{
  g_ki = ki_x1000 / 1000.0f;
  return ki_x1000;
}

int rpc_pid_set_kd(int kd_x1000)
{
  g_kd = kd_x1000 / 1000.0f;
  return kd_x1000;
}

// --------------------------------------------------
// RPC direct position PID
// --------------------------------------------------

int rpc_pos_enable()
{
  g_pidEnabled = false;
  g_profileEnabled = false;
  g_posEnabled = true;

  g_posIntegral = 0.0f;
  g_posPrevError = 0.0f;

  return 1;
}

int rpc_pos_disable()
{
  g_posEnabled = false;
  g_posIntegral = 0.0f;
  g_posPrevError = 0.0f;
  g_posOutputPwm = 0;
  motorCoast();
  return 0;
}

int rpc_pos_set_target_deg(int deg)
{
  g_targetAngleDeg = (float)deg;
  g_targetPositionCounts = (long)((g_targetAngleDeg / 360.0f) * OUTPUT_CPR);
  return deg;
}

int rpc_pos_get_target_deg()
{
  return (int)g_targetAngleDeg;
}

long rpc_pos_get_target_counts()
{
  return g_targetPositionCounts;
}

long rpc_pos_get_angle_x10()
{
  return g_outputAngleDeg_x10;
}

int rpc_pos_get_enabled()
{
  return g_posEnabled ? 1 : 0;
}

int rpc_pos_get_output_pwm()
{
  return g_posOutputPwm;
}

int rpc_pos_set_kp(int kp_x1000)
{
  g_posKp = kp_x1000 / 1000.0f;
  return kp_x1000;
}

int rpc_pos_set_ki(int ki_x1000)
{
  g_posKi = ki_x1000 / 1000.0f;
  return ki_x1000;
}

int rpc_pos_set_kd(int kd_x1000)
{
  g_posKd = kd_x1000 / 1000.0f;
  return kd_x1000;
}

// --------------------------------------------------
// RPC trapezoid profile
// --------------------------------------------------

int rpc_profile_enable()
{
  g_pidEnabled = false;
  g_posEnabled = false;
  g_profileEnabled = true;

  g_integral = 0.0f;
  g_prevError = 0.0f;
  g_profileVelocityDegPerSec = 0.0f;

  return 1;
}

int rpc_profile_disable()
{
  g_profileEnabled = false;

  g_integral = 0.0f;
  g_prevError = 0.0f;

  g_profileTargetRpm = 0;
  g_profileVelocityRpm = 0;
  g_profileErrorCounts = 0;
  g_profileVelocityDegPerSec = 0.0f;

  motorCoast();
  return 0;
}

int rpc_profile_set_target_deg(int deg)
{
  g_profileTargetDeg = (float)deg;
  g_profileTargetCounts = (long)((g_profileTargetDeg / 360.0f) * OUTPUT_CPR);
  return deg;
}

int rpc_profile_get_target_deg()
{
  return (int)g_profileTargetDeg;
}

long rpc_profile_get_target_counts()
{
  return g_profileTargetCounts;
}

int rpc_profile_get_enabled()
{
  return g_profileEnabled ? 1 : 0;
}

long rpc_profile_get_target_rpm()
{
  return g_profileTargetRpm;
}

long rpc_profile_get_velocity_rpm()
{
  return g_profileVelocityRpm;
}

long rpc_profile_get_error_counts()
{
  return g_profileErrorCounts;
}

int rpc_profile_set_max_rpm(int rpm)
{
  g_profileMaxRpm = (float)rpm;
  return rpm;
}

int rpc_profile_set_accel(int accel)
{
  g_profileAccelRpmPerSec = (float)accel;
  return accel;
}

int rpc_profile_get_max_rpm()
{
  return (int)g_profileMaxRpm;
}

int rpc_profile_get_accel()
{
  return (int)g_profileAccelRpmPerSec;
}

// --------------------------------------------------
// Setup
// --------------------------------------------------

void setup()
{
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);

  pinMode(ENC_A_PIN, INPUT_PULLUP);
  pinMode(ENC_B_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  delay(500);

  motorCoast();

  attachInterrupt(digitalPinToInterrupt(ENC_A_PIN), isrEncA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B_PIN), isrEncB, CHANGE);

  g_lastSpeedMicros = micros();

  Bridge.begin();

  Bridge.provide("motor/set", rpc_motor_set);
  Bridge.provide("motor/stop", rpc_motor_stop);
  Bridge.provide("motor/get", rpc_motor_get);

  Bridge.provide("encoder/count", rpc_encoder_count);
  Bridge.provide("encoder/dir", rpc_encoder_dir);
  Bridge.provide("encoder/raw_speed", rpc_encoder_raw_speed);
  Bridge.provide("encoder/filtered_speed", rpc_encoder_filtered_speed);
  Bridge.provide("encoder/motor_rpm", rpc_encoder_motor_rpm);
  Bridge.provide("encoder/output_rpm", rpc_encoder_output_rpm);
  Bridge.provide("encoder/reset", rpc_encoder_reset);

  Bridge.provide("pid/enable", rpc_pid_enable);
  Bridge.provide("pid/disable", rpc_pid_disable);
  Bridge.provide("pid/set_target_output_rpm", rpc_pid_set_target_output_rpm);
  Bridge.provide("pid/get_target_output_rpm", rpc_pid_get_target_output_rpm);
  Bridge.provide("pid/get_enabled", rpc_pid_get_enabled);
  Bridge.provide("pid/get_output_pwm", rpc_pid_get_output_pwm);
  Bridge.provide("pid/set_kp", rpc_pid_set_kp);
  Bridge.provide("pid/set_ki", rpc_pid_set_ki);
  Bridge.provide("pid/set_kd", rpc_pid_set_kd);

  Bridge.provide("position/enable", rpc_pos_enable);
  Bridge.provide("position/disable", rpc_pos_disable);
  Bridge.provide("position/set_target_deg", rpc_pos_set_target_deg);
  Bridge.provide("position/get_target_deg", rpc_pos_get_target_deg);
  Bridge.provide("position/get_target_counts", rpc_pos_get_target_counts);
  Bridge.provide("position/get_angle_x10", rpc_pos_get_angle_x10);
  Bridge.provide("position/get_enabled", rpc_pos_get_enabled);
  Bridge.provide("position/get_output_pwm", rpc_pos_get_output_pwm);
  Bridge.provide("position/set_kp", rpc_pos_set_kp);
  Bridge.provide("position/set_ki", rpc_pos_set_ki);
  Bridge.provide("position/set_kd", rpc_pos_set_kd);

  Bridge.provide("profile/enable", rpc_profile_enable);
  Bridge.provide("profile/disable", rpc_profile_disable);
  Bridge.provide("profile/set_target_deg", rpc_profile_set_target_deg);
  Bridge.provide("profile/get_target_deg", rpc_profile_get_target_deg);
  Bridge.provide("profile/get_target_counts", rpc_profile_get_target_counts);
  Bridge.provide("profile/get_enabled", rpc_profile_get_enabled);
  Bridge.provide("profile/get_target_rpm", rpc_profile_get_target_rpm);
  Bridge.provide("profile/get_velocity_rpm", rpc_profile_get_velocity_rpm);
  Bridge.provide("profile/get_error_counts", rpc_profile_get_error_counts);
  Bridge.provide("profile/set_max_rpm", rpc_profile_set_max_rpm);
  Bridge.provide("profile/set_accel", rpc_profile_set_accel);
  Bridge.provide("profile/get_max_rpm", rpc_profile_get_max_rpm);
  Bridge.provide("profile/get_accel", rpc_profile_get_accel);

  Serial.println("DC Motor Controller with Trapezoid Profile ready.");
}

void loop()
{
  updateControl();
}