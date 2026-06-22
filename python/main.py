from arduino.app_utils import *
from arduino.app_bricks.web_ui import WebUI

ui = WebUI()

def get_status():
    pwm = int(Bridge.call("motor/get"))

    count = int(Bridge.call("encoder/count"))
    direction = int(Bridge.call("encoder/dir"))
    raw = int(Bridge.call("encoder/raw_speed"))
    filt = int(Bridge.call("encoder/filtered_speed"))
    motor_rpm = int(Bridge.call("encoder/motor_rpm"))
    output_rpm = int(Bridge.call("encoder/output_rpm"))

    pid_enabled = int(Bridge.call("pid/get_enabled"))
    target_output_rpm = int(Bridge.call("pid/get_target_output_rpm"))
    pid_pwm = int(Bridge.call("pid/get_output_pwm"))

    position_enabled = int(Bridge.call("position/get_enabled"))
    position_target_deg = int(Bridge.call("position/get_target_deg"))
    position_target_counts = int(Bridge.call("position/get_target_counts"))
    output_angle_x10 = int(Bridge.call("position/get_angle_x10"))
    position_pwm = int(Bridge.call("position/get_output_pwm"))

    profile_enabled = int(Bridge.call("profile/get_enabled"))
    profile_target_deg = int(Bridge.call("profile/get_target_deg"))
    profile_target_counts = int(Bridge.call("profile/get_target_counts"))
    profile_target_rpm = int(Bridge.call("profile/get_target_rpm"))
    profile_velocity_rpm = int(Bridge.call("profile/get_velocity_rpm"))
    profile_error_counts = int(Bridge.call("profile/get_error_counts"))
    profile_max_rpm = int(Bridge.call("profile/get_max_rpm"))
    profile_accel = int(Bridge.call("profile/get_accel"))

    return {
        "pwm": pwm,

        "count": count,
        "direction": direction,
        "raw_speed": raw,
        "filtered_speed": filt,
        "motor_rpm": motor_rpm,
        "output_rpm": output_rpm,

        "pid_enabled": pid_enabled,
        "target_output_rpm": target_output_rpm,
        "pid_pwm": pid_pwm,

        "position_enabled": position_enabled,
        "position_target_deg": position_target_deg,
        "position_target_counts": position_target_counts,
        "output_angle_deg": output_angle_x10 / 10.0,
        "position_pwm": position_pwm,

        "profile_enabled": profile_enabled,
        "profile_target_deg": profile_target_deg,
        "profile_target_counts": profile_target_counts,
        "profile_target_rpm": profile_target_rpm,
        "profile_velocity_rpm": profile_velocity_rpm,
        "profile_error_counts": profile_error_counts,
        "profile_max_rpm": profile_max_rpm,
        "profile_accel": profile_accel
    }

def send_status(client=None):
    if client:
        ui.send_message("status_update", get_status(), client)
    else:
        ui.send_message("status_update", get_status())

def motor_set(client, data):
    pwm = int(data.get("pwm", 0))
    pwm = max(-255, min(255, pwm))
    Bridge.call("motor/set", pwm)
    send_status()

def motor_stop(client, data):
    Bridge.call("motor/stop")
    send_status()

def encoder_reset(client, data):
    Bridge.call("encoder/reset")
    send_status()

def pid_enable(client, data):
    Bridge.call("pid/enable")
    send_status()

def pid_disable(client, data):
    Bridge.call("pid/disable")
    send_status()

def pid_set_target(client, data):
    rpm = int(data.get("rpm", 0))
    Bridge.call("pid/set_target_output_rpm", rpm)
    send_status()

def pid_set_gains(client, data):
    Bridge.call("pid/set_kp", int(float(data.get("kp", 0.05)) * 1000))
    Bridge.call("pid/set_ki", int(float(data.get("ki", 0.10)) * 1000))
    Bridge.call("pid/set_kd", int(float(data.get("kd", 0.0)) * 1000))
    send_status()

def position_enable(client, data):
    Bridge.call("position/enable")
    send_status()

def position_disable(client, data):
    Bridge.call("position/disable")
    send_status()

def position_set_target(client, data):
    deg = int(data.get("deg", 0))
    Bridge.call("position/set_target_deg", deg)
    send_status()

def position_set_gains(client, data):
    Bridge.call("position/set_kp", int(float(data.get("kp", 0.20)) * 1000))
    Bridge.call("position/set_ki", int(float(data.get("ki", 0.00)) * 1000))
    Bridge.call("position/set_kd", int(float(data.get("kd", 0.01)) * 1000))
    send_status()

def profile_enable(client, data):
    Bridge.call("profile/enable")
    send_status()

def profile_disable(client, data):
    Bridge.call("profile/disable")
    send_status()

def profile_set_target(client, data):
    deg = int(data.get("deg", 0))
    Bridge.call("profile/set_target_deg", deg)
    send_status()

def profile_set_params(client, data):
    max_rpm = int(data.get("max_rpm", 20))
    accel = int(data.get("accel", 30))

    Bridge.call("profile/set_max_rpm", max_rpm)
    Bridge.call("profile/set_accel", accel)

    send_status()

def get_initial_state(client, data):
    send_status(client)

def get_live_status(client, data):
    send_status(client)

ui.on_message("motor_set", motor_set)
ui.on_message("motor_stop", motor_stop)

ui.on_message("encoder_reset", encoder_reset)

ui.on_message("pid_enable", pid_enable)
ui.on_message("pid_disable", pid_disable)
ui.on_message("pid_set_target", pid_set_target)
ui.on_message("pid_set_gains", pid_set_gains)

ui.on_message("position_enable", position_enable)
ui.on_message("position_disable", position_disable)
ui.on_message("position_set_target", position_set_target)
ui.on_message("position_set_gains", position_set_gains)

ui.on_message("profile_enable", profile_enable)
ui.on_message("profile_disable", profile_disable)
ui.on_message("profile_set_target", profile_set_target)
ui.on_message("profile_set_params", profile_set_params)

ui.on_message("get_initial_state", get_initial_state)
ui.on_message("get_live_status", get_live_status)

App.run()