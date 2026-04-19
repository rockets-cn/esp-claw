local mcpwm = require("mcpwm")
local delay = require("delay")

-- Update this GPIO to match the servo signal pin on your board.
local SERVO_GPIO_NUM = 14

-- Standard hobby servo timing.
local SERVO_FREQUENCY_HZ = 50
local SERVO_MIN_PULSE_US = 500
local SERVO_MAX_PULSE_US = 2500
local SERVO_PERIOD_US = 1000000 / SERVO_FREQUENCY_HZ

local function angle_to_duty_percent(angle)
    local clamped = angle
    if clamped < 0 then
        clamped = 0
    elseif clamped > 180 then
        clamped = 180
    end

    local pulse_width_us = SERVO_MIN_PULSE_US +
        (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) * clamped / 180
    return pulse_width_us * 100 / SERVO_PERIOD_US
end

print("[servo] creating servo pwm on gpio " .. tostring(SERVO_GPIO_NUM))
local pwm = mcpwm.new({
    gpio = SERVO_GPIO_NUM,
    frequency_hz = SERVO_FREQUENCY_HZ,
    duty_percent = angle_to_duty_percent(0),
})

pwm:start()
print("[servo] started at 0 degrees")
delay.delay_ms(500)

for angle = 0, 180, 5 do
    pwm:set_duty(angle_to_duty_percent(angle))
    print("[servo] angle -> " .. tostring(angle))
    delay.delay_ms(80)
end

delay.delay_ms(500)

for angle = 180, 0, -5 do
    pwm:set_duty(angle_to_duty_percent(angle))
    print("[servo] angle -> " .. tostring(angle))
    delay.delay_ms(80)
end

delay.delay_ms(500)
pwm:stop()
pwm:close()
print("[servo] done")
