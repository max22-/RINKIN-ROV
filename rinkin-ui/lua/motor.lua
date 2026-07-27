local motor = {}

function clamp(x, a, b)
    if x < a then return a
    elseif x > b then return b
    else return x end
end

function motor:new(n)
    local m = {
        n = n,
        speed = 0,
        plot = plot.new("moteur " .. tostring(n), 1000)
    }
    return setmetatable(m, {__index = self})
end

function motor:set_speed(s)
    if s ~= self.speed then
        self.speed = clamp(s, -9, 9)
        self:send_speed()
    end
end

function motor:send_speed()
    udp.send("#motor," .. tostring(self.n - 1) .. "," .. tostring(self.speed) .. "!")
end

function motor:update()
    self.plot:append(self.speed)
end

function motor:slider()
    local modified
    self.speed, modified = ImGui.SliderInt("Vitesse moteur " .. tostring(self.n), self.speed, -9, 9)
    if modified then
        self:send_speed()
    end
end

function motor:display_plot()
    self.plot:display()
end

return motor
