#ifdef __ANDROID__

#include <imgui.h>
#include <raymob.h>

#include "android_soft_keyboard.h"

static bool backspace_state = false, backspace_new_state;

void android_soft_keyboard() {
    ImGuiIO& io = ImGui::GetIO();
    backspace_new_state = false;
    while(true) {
        int k = GetLastSoftKeyUnicode();
        if(k != 0)
            io.AddInputCharacter(k);
        k = GetLastSoftKeyCode();
        
        if(k != 0) {
            switch(k) {
            case AKEYCODE_DEL:
                backspace_new_state = true;
                break;
            }
        } else break;
        ClearLastSoftKey();
    }
    if(backspace_state != backspace_new_state) {
        io.AddKeyEvent(ImGuiKey_Backspace, backspace_new_state);
        backspace_state = backspace_new_state;
    }
    static bool WantTextInputLast = false;
    if(io.WantTextInput && !WantTextInputLast)
        ShowSoftKeyboard();
    WantTextInputLast = io.WantTextInput;
}

#endif