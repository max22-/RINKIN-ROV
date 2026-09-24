#include <imgui.h>
#include <raylib.h>
#include <unordered_map>
#include <optional>
#include "virtual_joystick.h"

static bool is_touch_used(int touch_id);

class VirtualJoystickState {
public:
    void update(ImVec2 top_left, float radius) {
        this->radius = radius;
        center = ImVec2(top_left.x + radius, top_left.y + radius);
        update_touch_id();
        calc_pos();
    }

    ImVec2 get_xy_rel() const {
        return pos;
    }

    ImVec2 get_xy_abs() const {
        return {center.x + pos.x, center.y + pos.y};
    }

    bool is_active() const {
        return touch_id != -1;
    }

    int get_touch_id() const {
        return touch_id;
    }

private:
    void check_id() {
        if(touch_id < 0) return;
        const int tp_count = GetTouchPointCount();
        for(int i = 0; i < tp_count; i++) {
            if(GetTouchPointId(i) == touch_id) return;
        }
        touch_id = -1;
    }

    void update_touch_id() {
        check_id();
        if(touch_id < 0) {
            const int tp_count = GetTouchPointCount();
            for(int i = 0; i < tp_count; i++) {
                int id = GetTouchPointId(i);
                if(id == touch_id) break;
                Vector2 touch_pos = GetTouchPosition(id);
                const float dx = touch_pos.x - center.x;
                const float dy = touch_pos.y - center.y;
                if(dx * dx + dy * dy <= radius * radius && !is_touch_used(id)) {
                    touch_id = id;
                    initial_pos = ImVec2(touch_pos.x - center.x, touch_pos.y - center.y);
                    TraceLog(LOG_INFO, "touch_id = %d\n", touch_id);
                    return;
                }
            }
        }

    }

    void calc_pos() {
        if(touch_id < 0) {
            pos = ImVec2(0, 0);
            return;
        }
        Vector2 touch_pos = GetTouchPosition(touch_id);
        float dx = touch_pos.x - center.x;
        float dy = touch_pos.y - center.y;
        pos.x = dx - initial_pos.x;
        pos.y = dy - initial_pos.y;
    }


    ImVec2 center = ImVec2(0, 0);
    float radius = 0.0f;
    int touch_id = -1;
    ImVec2 initial_pos = ImVec2(0, 0);
    ImVec2 pos = ImVec2(0, 0);
};


static std::unordered_map<ImGuiID, VirtualJoystickState> joysticks;

static bool is_touch_used(int touch_id) {
    for(auto j: joysticks) {
        if(j.second.get_touch_id() == touch_id)
            return true;
    }
    return false;
}

namespace ImGui {
    bool VirtualJoystick(const char *label, float *x, float *y, float diameter) {
        ImGui::PushID(label);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 size(diameter, diameter);
        ImGui::InvisibleButton(label, size);

        ImGuiID id = ImGui::GetItemID();
        
        if(joysticks.count(id) == 0) {
            VirtualJoystickState state;
            joysticks.emplace(id, state);
        }

        float radius = diameter / 2;
        VirtualJoystickState &j = joysticks.at(id);
        j.update(p, radius);

        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        const ImVec2 center = ImVec2(p.x + radius, p.y + radius);
        const ImU32 red = ImGui::GetColorU32(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        draw_list->AddCircle(center, radius, red);

        if(j.is_active()) {
            ImVec2 pos = j.get_xy_rel();
            *x = pos.x / radius;
            *y = pos.y / radius;
            draw_list->AddCircleFilled(j.get_xy_abs(), radius * 0.5, red);
        } else {
            *x = 0.0f;
            *y = 0.0f;
        }
        
        ImGui::PopID();
        return j.is_active();
    }
}