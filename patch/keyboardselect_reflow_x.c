void keyboard_select(const Arg *dummy)
{
    win.mode ^= kbds_keyboardhandler(-1, NULL, 0, 0);
}

void searchforward(const Arg *)
{
    win.mode ^= kbds_keyboardhandler(-1, NULL, 0, 0);
    kbds_keyboardhandler(-2, NULL, 0, 0);
}

void searchbackward(const Arg *)
{
    win.mode ^= kbds_keyboardhandler(-1, NULL, 0, 0);
    kbds_keyboardhandler(-3, NULL, 0, 0);
}
