#include "stdbool.h"
#include "app.h"
#include "comm/comm.h"

bool app_init(void)
{
    (void)comm_init();

    return true;
}
