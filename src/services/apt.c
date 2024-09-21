#include "apt.h"

#include "../3ds.h"

DECL_SRV(apt) {
    u32* cmd_params = PTR(cmd_addr);
    switch (cmd.command) {
        default:
            lwarn("unknown command 0x%04x", cmd.command);
            cmd_params[0] = MAKE_IPCHEADER(1, 0);
            cmd_params[1] = -1;
            break;
    }
}