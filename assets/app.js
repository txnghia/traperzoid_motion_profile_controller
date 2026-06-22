console.log("DC motor controller loaded");

let socket;

let speedTrendData = [];
let positionTrendData = [];
let profileTrendData = [];

const MAX_POINTS = 120;

document.addEventListener("DOMContentLoaded", () => {
    const pwmSlider = document.getElementById("pwm-slider");
    const pwmValue = document.getElementById("pwm-value");

    const targetSlider = document.getElementById("target-rpm-slider");
    const targetValue = document.getElementById("target-rpm-value");

    const angleSlider = document.getElementById("target-angle-slider");
    const angleValue = document.getElementById("target-angle-value");

    const profileAngleSlider = document.getElementById("profile-angle-slider");
    const profileAngleValue = document.getElementById("profile-angle-value");

    const statusText = document.getElementById("status-text");
    const errorContainer = document.getElementById("error-container");

    socket = io(`http://${window.location.host}`);

    pwmSlider.oninput = () => pwmValue.textContent = pwmSlider.value;
    targetSlider.oninput = () => targetValue.textContent = targetSlider.value;
    angleSlider.oninput = () => angleValue.textContent = angleSlider.value;
    profileAngleSlider.oninput = () => profileAngleValue.textContent = profileAngleSlider.value;

    document.getElementById("send-button").onclick = () => {
        socket.emit("motor_set", {"pwm": parseInt(pwmSlider.value)});
    };

    document.getElementById("stop-button").onclick = () => {
        pwmSlider.value = 0;
        pwmValue.textContent = "0";
        socket.emit("motor_stop", {});
    };

    document.getElementById("reset-encoder-button").onclick = () => {
        socket.emit("encoder_reset", {});
    };

    document.getElementById("pid-set-target-button").onclick = () => {
        socket.emit("pid_set_target", {"rpm": parseInt(targetSlider.value)});
    };

    document.getElementById("pid-enable-button").onclick = () => {
        socket.emit("pid_enable", {});
    };

    document.getElementById("pid-disable-button").onclick = () => {
        socket.emit("pid_disable", {});
    };

    document.getElementById("pid-set-gains-button").onclick = () => {
        socket.emit("pid_set_gains", {
            "kp": parseFloat(document.getElementById("kp-input").value),
            "ki": parseFloat(document.getElementById("ki-input").value),
            "kd": parseFloat(document.getElementById("kd-input").value)
        });
    };

    document.getElementById("position-set-target-button").onclick = () => {
        socket.emit("position_set_target", {"deg": parseInt(angleSlider.value)});
    };

    document.getElementById("position-enable-button").onclick = () => {
        socket.emit("position_enable", {});
    };

    document.getElementById("position-disable-button").onclick = () => {
        socket.emit("position_disable", {});
    };

    document.getElementById("position-set-gains-button").onclick = () => {
        socket.emit("position_set_gains", {
            "kp": parseFloat(document.getElementById("pos-kp-input").value),
            "ki": parseFloat(document.getElementById("pos-ki-input").value),
            "kd": parseFloat(document.getElementById("pos-kd-input").value)
        });
    };

    document.getElementById("profile-set-target-button").onclick = () => {
        socket.emit("profile_set_target", {"deg": parseInt(profileAngleSlider.value)});
    };

    document.getElementById("profile-enable-button").onclick = () => {
        socket.emit("profile_enable", {});
    };

    document.getElementById("profile-disable-button").onclick = () => {
        socket.emit("profile_disable", {});
    };

    document.getElementById("profile-set-params-button").onclick = () => {
        socket.emit("profile_set_params", {
            "max_rpm": parseInt(document.getElementById("profile-max-rpm-input").value),
            "accel": parseInt(document.getElementById("profile-accel-input").value)
        });
    };

    socket.on("connect", () => {
        statusText.textContent = "Status: connected";
        errorContainer.textContent = "";
        socket.emit("get_initial_state", {});
    });

    socket.on("disconnect", () => {
        statusText.textContent = "Status: disconnected";
    });

    socket.on("connect_error", (err) => {
        errorContainer.textContent = "Socket error: " + err.message;
    });

    socket.on("status_update", (data) => {
        document.getElementById("pwm-value").textContent = data.pwm;

        document.getElementById("encoder-count").textContent = data.count;
        document.getElementById("encoder-dir").textContent = directionText(data.direction);
        document.getElementById("raw-speed").textContent = data.raw_speed;
        document.getElementById("filtered-speed").textContent = data.filtered_speed;
        document.getElementById("motor-rpm").textContent = data.motor_rpm;
        document.getElementById("output-rpm").textContent = data.output_rpm;
        document.getElementById("output-angle-deg").textContent = data.output_angle_deg;

        document.getElementById("pid-enabled").textContent = data.pid_enabled;
        document.getElementById("pid-pwm").textContent = data.pid_pwm;
        document.getElementById("target-output-rpm").textContent = data.target_output_rpm;

        document.getElementById("position-enabled").textContent = data.position_enabled;
        document.getElementById("position-target-deg").textContent = data.position_target_deg;
        document.getElementById("position-target-counts").textContent = data.position_target_counts;
        document.getElementById("position-pwm").textContent = data.position_pwm;

        document.getElementById("profile-enabled").textContent = data.profile_enabled;
        document.getElementById("profile-target-deg").textContent = data.profile_target_deg;
        document.getElementById("profile-target-counts").textContent = data.profile_target_counts;
        document.getElementById("profile-error-counts").textContent = data.profile_error_counts;
        document.getElementById("profile-target-rpm").textContent = data.profile_target_rpm;
        document.getElementById("profile-max-rpm").textContent = data.profile_max_rpm;
        document.getElementById("profile-accel").textContent = data.profile_accel;

        statusText.textContent = "Status: running";

        addSpeedTrendPoint(data);
        addPositionTrendPoint(data);
        addProfileTrendPoint(data);
    });

    setInterval(() => {
        if (socket && socket.connected) {
            socket.emit("get_live_status", {});
        }
    }, 200);
});

function directionText(dir) {
    if (dir > 0) return "Forward";
    if (dir < 0) return "Reverse";
    return "Stopped";
}

function addSpeedTrendPoint(data) {
    const target = data.target_output_rpm || 0;
    const actual = data.output_rpm || 0;
    const pwm = data.pid_pwm || 0;

    speedTrendData.push({ target, actual, pwm: pwm / 5.0 });

    if (speedTrendData.length > MAX_POINTS) speedTrendData.shift();

    drawTrendGraph("speed-trend-canvas", speedTrendData, "Target RPM", "Actual RPM", "PWM/5");

    document.getElementById("graph-target-rpm").textContent = target;
    document.getElementById("graph-output-rpm").textContent = actual;
    document.getElementById("graph-pid-pwm").textContent = pwm;
}

function addPositionTrendPoint(data) {
    const target = data.position_target_deg || 0;
    const actual = data.output_angle_deg || 0;
    const pwm = data.position_pwm || 0;

    positionTrendData.push({ target, actual, pwm: pwm / 5.0 });

    if (positionTrendData.length > MAX_POINTS) positionTrendData.shift();

    drawTrendGraph("position-trend-canvas", positionTrendData, "Target deg", "Actual deg", "PWM/5");

    document.getElementById("graph-target-angle").textContent = target;
    document.getElementById("graph-output-angle").textContent = Number(actual).toFixed(1);
    document.getElementById("graph-position-pwm").textContent = pwm;
}

function addProfileTrendPoint(data) {
    const target = data.profile_target_deg || 0;
    const actual = data.output_angle_deg || 0;
    const rpm = data.profile_target_rpm || 0;

    profileTrendData.push({ target, actual, pwm: rpm });

    if (profileTrendData.length > MAX_POINTS) profileTrendData.shift();

    drawTrendGraph("profile-trend-canvas", profileTrendData, "Target deg", "Actual deg", "RPM");

    document.getElementById("graph-profile-target-angle").textContent = target;
    document.getElementById("graph-profile-actual-angle").textContent = Number(actual).toFixed(1);
    document.getElementById("graph-profile-rpm").textContent = rpm;
}

function drawTrendGraph(canvasId, trendData, labelTarget, labelActual, labelPwm) {
    const canvas = document.getElementById(canvasId);
    if (!canvas) return;

    const ctx = canvas.getContext("2d");
    const w = canvas.width;
    const h = canvas.height;

    ctx.clearRect(0, 0, w, h);

    ctx.fillStyle = "#ffffff";
    ctx.fillRect(0, 0, w, h);

    const left = 45;
    const right = 15;
    const top = 15;
    const bottom = 35;

    const gw = w - left - right;
    const gh = h - top - bottom;

    let maxAbs = 10;

    for (const p of trendData) {
        maxAbs = Math.max(maxAbs, Math.abs(p.target), Math.abs(p.actual), Math.abs(p.pwm));
    }

    maxAbs = Math.ceil(maxAbs / 10) * 10;

    function yMap(v) {
        return top + gh / 2 - (v / maxAbs) * (gh / 2);
    }

    function xMap(i) {
        return left + (i / (MAX_POINTS - 1)) * gw;
    }

    ctx.strokeStyle = "#dddddd";
    ctx.lineWidth = 1;

    for (let i = -2; i <= 2; i++) {
        const y = top + gh / 2 + i * gh / 4;
        ctx.beginPath();
        ctx.moveTo(left, y);
        ctx.lineTo(left + gw, y);
        ctx.stroke();
    }

    ctx.strokeStyle = "#888888";
    ctx.beginPath();
    ctx.moveTo(left, yMap(0));
    ctx.lineTo(left + gw, yMap(0));
    ctx.stroke();

    ctx.fillStyle = "#333333";
    ctx.font = "12px Arial";
    ctx.fillText(String(maxAbs), 5, yMap(maxAbs) + 4);
    ctx.fillText("0", 20, yMap(0) + 4);
    ctx.fillText(String(-maxAbs), 5, yMap(-maxAbs) + 4);

    function drawLine(key, color) {
        if (trendData.length < 2) return;

        ctx.strokeStyle = color;
        ctx.lineWidth = 2;
        ctx.beginPath();

        for (let i = 0; i < trendData.length; i++) {
            const x = xMap(MAX_POINTS - trendData.length + i);
            const y = yMap(trendData[i][key]);

            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }

        ctx.stroke();
    }

    drawLine("target", "blue");
    drawLine("actual", "green");
    drawLine("pwm", "red");

    ctx.font = "13px Arial";

    ctx.fillStyle = "blue";
    ctx.fillText(labelTarget, left, h - 12);

    ctx.fillStyle = "green";
    ctx.fillText(labelActual, left + 120, h - 12);

    ctx.fillStyle = "red";
    ctx.fillText(labelPwm, left + 240, h - 12);
}