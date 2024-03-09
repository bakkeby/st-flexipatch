void toggle_winmode(int flag) {
    win.mode ^= flag;
}

void keyboard_select(const Arg *dummy) {
    win.mode ^= trt_kbdselect(-1, NULL, 0);
}
